# Special preprocessor wrapper that expands macros without inlining `#include`s.

from typing import List, NamedTuple, Dict
import collections
import logging
import os
import re
import subprocess
from common import TranslationUnitInfo

class ExpandMacrosOptions(NamedTuple):
    # If this is false, the other options don't matter.
    enable: bool
    includes_before_undefs: List[str]
    undef_macros: List[str]

class FileLinePos(NamedTuple):
    fname: str
    lineno: int
    def __str__(self):
        return f'{self.fname}:{self.lineno}'

class LineInfo:
    saw_blank_expansion: bool
    expansion_occurrences: Dict[str, List[FileLinePos]]
    def __init__(self):
        self.saw_blank_expansion = False
        self.expansion_occurrences = collections.defaultdict(lambda: [])

class SourceFileInfo:
    lines: Dict[int, LineInfo]
    def __init__(self):
        self.lines = collections.defaultdict(lambda: LineInfo())

def preprocess(tu: TranslationUnitInfo, i_fname: str, custom_input_filename=None):
    subprocess.check_call(
        [tu.compiler_path, '-E', '-o', i_fname] +
        tu.compiler_args +
        [custom_input_filename if custom_input_filename is not None else tu.input_filename],
        cwd=tu.target_directory)

def expandMacros(opts: ExpandMacrosOptions, compilation_base_dir: str,
                 translation_units: List[TranslationUnitInfo]):
    if not opts.enable:
        return

    compilation_base_dir = os.path.realpath(compilation_base_dir)

    for tu in translation_units:
        # Since there may be multiple compilation database entries / translation
        # units for the same input file (presumably with different compiler
        # options), translation units must always be identified by their output
        # files (which must be distinct, otherwise it doesn't make sense).
        logging.info(f'Saving original preprocessor output for {tu.output_realpath}...')
        preprocess(tu, f'{tu.output_realpath}.pre-em.i')

    # The keys are canonical (os.path.realpath) source file paths.
    source_files: Dict[str, SourceFileInfo] = (
        collections.defaultdict(lambda: SourceFileInfo()))

    # Map the preprocessed output of all translation units back to source lines.
    for tu in translation_units:
        logging.info(f'Scanning customized preprocessor output for {tu.output_realpath}...')
        em_fname = tu.output_realpath + '.em.c'
        with open(em_fname, 'w') as em_f:
            for inc in opts.includes_before_undefs:
                em_f.write(f'#include {inc}\n')
            for macro in opts.undef_macros:
                em_f.write(f'#undef {macro}\n')
            em_f.write(f'#include "{tu.input_realpath}"\n')
            pass

        i_fname = tu.output_realpath + '.em.i'
        preprocess(tu, i_fname, em_fname)

        with open(i_fname) as i_f:
            src_fname, src_lineno = None, None
            for i_lineno0, i_line_content in enumerate(
                    l.rstrip('\n') for l in i_f.readlines()):
                i_lineno = i_lineno0 + 1
                m = re.search(r'^# (\d+) "(.*)"[^"]*', i_line_content)
                if m is not None:
                    # XXX Unescape the filename?
                    src_fname = tu.realpath(m.group(2))
                    src_lineno = int(m.group(1))
                else:
                    i_loc = FileLinePos(i_fname, i_lineno)
                    assert src_lineno is not None, (
                        f'{i_loc}: source line without preceding line marker')
                    line_info = source_files[src_fname].lines[src_lineno]
                    if i_line_content == '':
                        line_info.saw_blank_expansion = True
                    else:
                        line_info.expansion_occurrences[i_line_content].append(i_loc)
                    src_lineno += 1

    # Update source files.
    for src_fname, src_finfo in source_files.items():
        if not src_fname.startswith(compilation_base_dir + '/'):
            logging.info(f'Not updating source file {src_fname} because it is outside the base directory')
            continue
        if src_fname.endswith('.em.c'):
            # The preprocessor is giving us blank lines in these files. Just ignore them.
            continue
        logging.info(f'Updating source file {src_fname}...')
        src_fname_new = src_fname + '.new'
        with open(src_fname) as src_f, open(src_fname_new, 'w') as src_f_new:
            in_preprocessor_directive = False
            for src_lineno0, src_line_content in enumerate(
                    l.rstrip('\n') for l in src_f.readlines()):
                src_lineno = src_lineno0 + 1
                src_loc = FileLinePos(src_fname, src_lineno)
                if not in_preprocessor_directive and re.search(r'^\s*#', src_line_content):
                    in_preprocessor_directive = True
                line_info = src_finfo.lines[src_lineno]
                expansions = list(line_info.expansion_occurrences.items())
                if in_preprocessor_directive:
                    if len(expansions) > 0:
                        logging.warning(
                            f'{src_loc}: in a preprocessor directive but had a non-blank expansion: '
                            f'{repr(expansions[0][0])} at {expansions[0][1][0]}')
                    # Pass the directive through, but not the portion of any
                    # comment that starts later on the line. libtiff has code like this:
                    #
                    # #define FOO 1   /* first line of comment
                    #                    second line of comment */
                    #
                    # The preprocessor will blank the whole thing. If we kept the entire
                    # #define line but let the second line get blanked as usual, we'd
                    # end up with an unterminated comment.
                    #
                    # HOWEVER, before we do this, try to remove /*...*/ comments within
                    # the directive itself. In this case (also seen in libtiff):
                    #
                    # #define FOO /* blah */ one_line_of_code; \
                    #                        another_line_of_code;
                    #
                    # if we didn't remove /* blah */ first, then we would remove everything
                    # starting from the /* through the line continuation backslash, which
                    # would break things badly.
                    #
                    # XXX: This will break if // or /* appears inside a string literal and
                    # maybe some other obscure cases. Rely on the final verification.
                    directive_content = re.sub('/\*.*?\*/', '', src_line_content)
                    directive_content = re.sub('/[/*].*', '', directive_content)
                    src_f_new.write(directive_content + '\n')
                    # Preprocessor directives can be continued onto the next line with a backslash.
                    # XXX: This doesn't interact properly with the above comment stripping.
                    # Again, if this case comes up, the verification will catch it.
                    if not re.search(r'\\\s*$', src_line_content):
                        in_preprocessor_directive = False
                elif len(expansions) == 0:
                    if line_info.saw_blank_expansion:
                        src_f_new.write('\n')
                    else:
                        # This line number was never reached in the preprocessed output. However,
                        # we've seen examples where a preprocessed line was logically blank but
                        # the preprocessor skipped it by emitting a line marker with a later line
                        # number rather than passing the blank line through. So guess that the
                        # line is supposed to be blank.
                        src_f_new.write('\n')
                elif len(expansions) == 1:
                    # If we also saw a blank expansion, assume that was an #if branch not taken
                    # and keep the non-blank expansion.
                    # If we are wrong, we'll catch it in the final verification.
                    src_f_new.write(expansions[0][0] + '\n')
                else:
                    # This line was expanded differently at different inclusion sites.
                    # Try keeping the original content. This might be wrong if it was part
                    # of a macro call that spanned several lines; if so, we'll catch that
                    # in the final verification.
                    logging.warning(f'{src_loc}: had several different expansions, '
                                    f'including {repr(expansions[0][0])} at {expansions[0][1][0]} '
                                    f'and {repr(expansions[1][0])} at {expansions[1][1][0]}; '
                                    f'keeping original content')
                    src_f_new.write(src_line_content + '\n')
        os.rename(src_fname, src_fname + '.pre-em')
        os.rename(src_fname_new, src_fname)

    # Check that we didn't change the preprocessed output.
    verification_ok = True
    for tu in translation_units:
        logging.info(f'Verifying preprocessor output for {tu.output_realpath}...')
        preprocess(tu, f'{tu.output_realpath}.post-em.i')
        # `diff` exits nonzero if there is a difference.
        returncode = subprocess.call(['diff', '-u',
                                      f'{tu.output_realpath}.pre-em.i',
                                      f'{tu.output_realpath}.post-em.i'])
        if returncode != 0:
            verification_ok = False
    assert verification_ok, 'Verification of preprocessed output failed: see diffs above.'

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

def preprocess(tu, i_fname, custom_tu_file=None):
    subprocess.check_call(
        [tu.compiler_path, '-E', '-o', i_fname] +
        tu.compiler_args +
        [custom_tu_file if custom_tu_file is not None else tu.file],
        cwd=tu.target_directory)

def expandMacros(opts: ExpandMacrosOptions, compilation_base_dir: str,
                 translation_units: List[TranslationUnitInfo]):
    if not opts.enable:
        return

    compilation_base_dir = os.path.realpath(compilation_base_dir)

    for tu in translation_units:
        logging.info(f'Saving original preprocessed output of {tu.file}...')
        preprocess(tu, f'{tu.file}.i')

    source_files: Dict[str, SourceFileInfo] = collections.defaultdict(lambda: SourceFileInfo())

    # Map the preprocessed output of all translation units back to source lines.
    for tu in translation_units:
        logging.info(f'Scanning customized preprocessed output of {tu.file}...')
        em_fname = tu.file + '.em.c'
        with open(em_fname, 'w') as em_f:
            for inc in opts.includes_before_undefs:
                em_f.write(f'#include {inc}\n')
            for macro in opts.undef_macros:
                em_f.write(f'#undef {macro}\n')
            em_f.write(f'#include "{os.path.basename(tu.file)}"\n')
            pass

        i_fname = em_fname + '.i'
        preprocess(tu, i_fname, em_fname)

        with open(i_fname) as i_f:
            src_fname, src_lineno = None, None
            for i_lineno0, i_line_content in enumerate(
                    l.rstrip('\n') for l in i_f.readlines()):
                i_lineno = i_lineno0 + 1
                m = re.search(r'^# (\d+) "(.*)"[^"]*', i_line_content)
                if m is not None:
                    # XXX Unescape the filename, maybe using `eval`?
                    # REVIEW: Do we have to deal with relative paths or anything?
                    src_fname, src_lineno = m.group(2), int(m.group(1))
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
        if not os.path.realpath(src_fname).startswith(compilation_base_dir + '/'):
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
                    # Pass the directive through.
                    src_f_new.write(src_line_content + '\n')
                    if not re.search(r'\\\s*$', src_line_content):
                        in_preprocessor_directive = False
                elif len(expansions) == 0:
                    if line_info.saw_blank_expansion:
                        src_f_new.write('\n')
                    else:
                        # This line was never reached at all. It probably doesn't
                        # matter what we put here, but preserve the original content by default.
                        src_f_new.write(src_line_content + '\n')
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
        os.rename(src_fname_new, src_fname)

    # Check that we didn't change the preprocessed output.
    verification_ok = True
    for tu in translation_units:
        logging.info(f'Verifying preprocessed output of {tu.file}...')
        i_fname = tu.file + '.i'
        i_fname_new = i_fname + '.new'
        preprocess(tu, i_fname_new)
        # `diff` exits nonzero if there is a difference.
        # XXX Relative paths??
        returncode = subprocess.call(['diff', '-u', i_fname, i_fname_new])
        if returncode != 0:
            verification_ok = False
    assert verification_ok, 'Verification of preprocessed output failed: see diffs above.'

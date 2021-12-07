# Data structures that need to be imported by both generate_ccommands and
# expand_macros.

from dataclasses import dataclass, field
from typing import List
import functools
import os
import re

# We are assuming it's OK to cache canonical paths for the lifetime of any
# process that uses this code.
realpath_cached = functools.lru_cache(maxsize=None)(os.path.realpath)


@dataclass
class TranslationUnitInfo:
    compiler_path: str
    # Any file paths in compiler_args (-I, etc.), input_filename, and
    # output_filename may be relative to working_directory.
    compiler_args: List[str]
    working_directory: str
    input_filename: str

    # compiler_args with -c, -o, and the input filename removed so the macro
    # expander can add its own options. Don't compute this until it's requested
    # because it makes some assertions that may fail in some scenarios.
    @functools.cached_property
    def common_compiler_args(self) -> List[str]:
        self.__scan_compiler_args()
        return self.common_compiler_args

    @functools.cached_property
    def output_filename(self) -> str:
        self.__scan_compiler_args()
        return self.output_filename

    def __scan_compiler_args(self):
        assert self.input_filename == self.compiler_args[-1], (
            'TranslationUnitInfo.__scan_compiler_args expects the last '
            'compiler argument to be the input filename.')
        args_without_input = self.compiler_args[:-1]
        assert self.input_filename.endswith('.c'), (
            'TranslationUnitInfo.__scan_compiler_args currently only supports '
            'C source files.')
        # Default; overwritten if we see an `-o` option later.
        # TODO: Use removesuffix once we require Python >= 3.9.
        self.output_filename = self.input_filename[:-len('.c')] + '.o'
        self.common_compiler_args = []
        idx = 0
        while idx < len(args_without_input):
            arg = args_without_input[idx]
            idx += 1
            if arg == '-c':
                pass
            elif arg == '-o':
                self.output_filename = args_without_input[idx]
                idx += 1
            else:
                self.common_compiler_args.append(arg)

    def fullpath(self, path: str):
        return os.path.normpath(os.path.join(self.working_directory, path))

    def realpath(self, path: str):
        return realpath_cached(os.path.join(self.working_directory, path))

    @functools.cached_property
    def input_realpath(self):
        return self.realpath(self.input_filename)

    @functools.cached_property
    def output_fullpath(self):
        return self.fullpath(self.output_filename)

    @functools.cached_property
    def output_realpath(self):
        return self.realpath(self.output_filename)


CompilationDatabase = List[TranslationUnitInfo]


def assert_no_duplicate_outputs(compdb):
    output_fullpaths = set()
    for tu in compdb:
        assert tu.output_fullpath not in output_fullpaths, (
            'Multiple compilation database entries with output file '
            f'{tu.output_fullpath}: not supported by this tool.')


def unescape_compdb_command(command_str):
    # See the specification of how `command` is escaped in
    # clang/docs/JSONCompilationDatabase.rst. Clang's actual implementation in
    # `unescapeCommandLine` in clang/lib/Tooling/JSONCompilationDatabase.cpp is
    # somewhat fancier, but this should be good enough for us.
    args = []
    # Skip any leading spaces.
    pos = re.match(' *', command_str).end()
    while pos < len(command_str):
        # Each escaped argument consists of a sequence of one or more of the
        # following units: a double-quoted string of zero or more ordinary
        # characters (not \ or ") or escape sequences (\\ or \"), or a single
        # escape sequence or ordinary character other than a space. Look for the
        # next well-formed escaped argument and ignore any spaces after it. If
        # we're not at the end of the string but we can't match another
        # well-formed escaped argument, that means the command is invalid.
        #
        # We have to compile the regex in order to use the `pos` argument.
        escaped_arg_re = re.compile(
            r' *((?:\"(?:[^\\"]|\\[\\"])*\"|[^\\" ]|\\[\\"])+) *')
        m = escaped_arg_re.match(command_str, pos=pos)
        assert m, ('Improperly escaped command in compilation database: ' +
                   command_str)
        # Now decode escape sequences and remove double quotes that are not part
        # of escape sequences. `re.sub` finds non-overlapping matches from left
        # to right, so it won't start a match in the middle of an escape
        # sequence.
        args.append(re.sub(r'"|\\([\\"])', r'\1', m.group(1)))
        pos = m.end()
    return args


def compdb_entry_from_json(j):
    input_filename = j['file']
    working_directory = j['directory']
    if 'arguments' in j:
        args = j['arguments']
    elif 'command' in j:
        args = unescape_compdb_command(j['command'])
    else:
        raise AssertionError(f'Compilation database entry has no command')
    compiler_path = args[0]
    compiler_args = args[1:]
    # TODO: Should we honor j['output'] if it is set?
    return TranslationUnitInfo(compiler_path, compiler_args, working_directory,
                               input_filename)


def compdb_from_json(j):
    return [compdb_entry_from_json(ej) for ej in j]

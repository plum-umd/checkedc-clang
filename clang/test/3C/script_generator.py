# script_generator.py: Generates an unexpanded RUN script for a test file based
# on its name and flags that specify the kind of testing to be done. Used by
# both 3c-regtest.py and 3c-regtest-unconvert.py.

import os
import re
import argparse

def remove_suffix(string, suffix):
    return string[:-len(suffix)] if string.endswith(suffix) else None

# Right now, our focus is on exactly reproducing the existing RUN scripts to
# verify the conversion. Obviously, we eventually want to remove the cruft and
# add new features. TBD whether we complete the verification first and then
# change the scripts here or we maintain old scripts (for reproduction) and new
# scripts (for running tests) here in parallel for some period of time.

# Unlike all the other % codes that are substituted by `lit`, %B is a special
# code that generate_script (below) replaces with the basename of the test file
# without the `.c` extension. This reproduces some of the scripts in existing
# test files.
common_script = '''\
3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
3c -output-postfix=checked -alltypes %s
3c -alltypes %S/%B.checked.c -- | %COMPARISON
rm %S/%B.checked.c
'''

# As per https://github.com/correctcomputation/checkedc-clang/issues/328, the
# `count 0` may be replaced with `diff -w %S/%B.checked.c -` in the future.
common_script_no_diff = common_script.replace('%COMPARISON', 'count 0')

# I guess the important part here is the `-w` to work around the rewriter not
# exactly preserving whitespace?
common_script_diff_w = common_script.replace('%COMPARISON', 'diff -w %S/%B.checked.c -')

# Equivalent variant of common_script_no_diff seen in testgenerator.py.
common_script_testgenerator = '''\
3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

3c -alltypes -output-postfix=checked %s
3c -alltypes %S/%B.checked.c -- | count 0
rm %S/%B.checked.c
'''

# Additional codes substituted in generate_script:
# - %M: %B with a trailing `1` or `2` removed
# - %O ("other"): %B with a trailing `1` replaced with `2` or vice versa
# - %F{1,2}: %s if the digit matches the test filename, %S/%O otherwise
# - %2: `2` in a multi2 input file, otherwise empty string
multi_script='''\
3c -base-dir=%S -addcr -alltypes -output-postfix=checkedALL%2 %F1 %F2
3c -base-dir=%S -addcr -output-postfix=checkedNOALL%2 %F1 %F2
%clang -c %S/%M1.checkedNOALL%2.c %S/%M2.checkedNOALL%2.c
FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %S/%B.checkedNOALL%2.c %s
FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %S/%B.checkedALL%2.c %s
3c -base-dir=%S -alltypes -output-postfix=checked%2 %S/%O.c %s
3c -base-dir=%S -alltypes -output-postfix=convert_again %S/%M1.checked%2.c %S/%M2.checked%2.c
test ! -f %S/%M1.checked%2.convert_again.c
test ! -f %S/%M2.checked%2.convert_again.c
rm %S/%M1.checkedALL%2.c %S/%M2.checkedALL%2.c
rm %S/%M1.checkedNOALL%2.c %S/%M2.checkedNOALL%2.c
rm %S/%M1.checked%2.c %S/%M2.checked%2.c
'''

predefined_scripts = {
    'common': common_script_no_diff,
    'common-diff-w': common_script_diff_w,
    'common-testgenerator': common_script_testgenerator,
    'multi': multi_script
}

parser = argparse.ArgumentParser(
    # This will be shown as if it is a program name runnable by the user. It
    # isn't, but give the user a clue what this argument parser represents.
    prog='<3c-regtest script generator>',
    # Without this, the parser in 3c-regtest.py raises an error about
    # conflicting --help options. That's silly; one would expect argparse to
    # handle this specially.
    add_help=False
)
# TODO: Add help
parser.add_argument('test_file')
# Future: Redesign in terms of kinds of testing rather than merely selecting one
# of the existing 5 scripts of the test updating programs.
parser.add_argument('--predefined-script', required=True,
                    choices=list(predefined_scripts.keys()))

# `argobj` should be an argument object returned by `parser` or a parser based
# on it. Returns a list of command strings.
def generate_commands(argobj):
    test_file = argobj.test_file
    predefined_script_name = argobj.predefined_script

    # FUTURE: If this assumption is no longer valid, update this code.
    test_file_basename_without_ext = remove_suffix(os.path.basename(test_file), '.c')
    if test_file_basename_without_ext is None:
        sys.exit('Test filename does not end in `.c`.')
    script = remove_suffix(predefined_scripts[predefined_script_name], '\n')
    script = script.replace('%B', test_file_basename_without_ext)

    if predefined_script_name == 'multi':
        last = test_file_basename_without_ext[-1:]
        multi_base = test_file_basename_without_ext[:-1]
        if last == '1':
            multi_other = multi_base + '2'
            f1 = '%s'
            f2 = '%S/%O.c'
            p2 = ''
        elif last == '2':
            multi_other = multi_base + '1'
            f1 = '%S/%O.c'
            f2 = '%s'
            p2 = '2'
        else:
            sys.exit('--predefined-script is multi, '
                     'but test filename does not end in `1.c` or `2.c`.')
        script = script.replace('%F1', f1)
        script = script.replace('%F2', f2)
        script = script.replace('%M', multi_base)
        script = script.replace('%O', multi_other)
        script = script.replace('%2', p2)

    return script.split('\n')

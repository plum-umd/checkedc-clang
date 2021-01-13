#!/usr/bin/env python
#
# 3c-regtest-unconvert.py can be used to verify that an existing 3C regression
# test with a written-out RUN script has been correctly converted to use
# 3c-regtest.py.
#
# Usage: 3c-regtest-unconvert.py TEST_FILE
#
# This verifies that the TEST_FILE contains a 3c-regtest.py RUN command in
# canonical form and then prints the TEST_FILE with this RUN command replaced by
# the generated RUN script. The output should be identical to the test file
# before conversion to 3c-regtest.py.

import sys
import re
import argparse

import script_generator

parser = argparse.ArgumentParser(description='''\
Converts a 3C regression test that uses 3c-regtest.py back to one with a
written-out RUN script so you can verify that a test was correctly converted to
use 3c-regtest.py.\
''')
# TODO: Add help
parser.add_argument('test_file')

argobj = parser.parse_args()

with open(argobj.test_file) as f:
    lines = f.readlines()

new_lines = []
for l in lines:
    if '3c-regtest' in l:
        m = re.search(r"\A// RUN: %S/3c-regtest\.py (.*) %s -t %t --clang '%clang'\n\Z", l)
        if m is None:
            sys.exit('Non-canonical 3c-regtest RUN line: %s' % l)  # XXX Trailing newline
        test_type_flags_joined = m.group(1)

        # FUTURE: Will we need to handle quoting?
        test_type_flags = test_type_flags_joined.split(' ')
        # XXX: This just exits on error. We'd like to add a more meaningful message, but
        # the default Python version on gamera (2.7.18) is too old to support
        # exit_on_error=False.
        test_type_argobj = script_generator.parser.parse_args(test_type_flags + [argobj.test_file])

        run_lines = [('// RUN: %s\n' % cmd if cmd != '' else '\n')
                     for cmd in script_generator.generate_commands(test_type_argobj)]
        new_lines.extend(run_lines)
    else:
        new_lines.append(l)

sys.stdout.write(''.join(new_lines))

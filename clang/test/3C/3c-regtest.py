#!/usr/bin/env python
#
# 3c-regtest.py: Run a 3C regression test using a standard RUN script.
#
# 3c-regtest.py is intended to be invoked by a single RUN command in the test
# file. The canonical form is:
#
# // RUN: %S/3c-regtest.py TEST_TYPE_FLAGS %s -t %t --clang '%clang'
#
# 3c-regtest.py generates a RUN script based on the TEST_TYPE_FLAGS (using
# script_generator.py) and runs it using code from `lit`.
#
# The -t and --clang flags are used to pass substitution values from the outer
# `lit` configuration so that 3c-regtest.py knows what to substitute for
# occurrences of %t and %clang in its script. We'll add flags like this for all
# % codes that appear in the scripts.

# TODO: Add Windows compatibility code once we have an easy way to test on
# Windows.

import sys
import os
import platform
import argparse

import script_generator

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) +
                '/../../../llvm/utils/lit')
import lit.TestRunner

parser = argparse.ArgumentParser(description='Run a 3C regression test.',
                                 parents=[script_generator.parser])
# Substitution arguments. The test file name is already in
# script_generator.parser.
parser.add_argument('-t')
parser.add_argument('--clang')

argobj = parser.parse_args()

test_dir = os.path.dirname(os.path.abspath(argobj.test_file))

tmpName = argobj.t
tmpBase = script_generator.remove_suffix(tmpName, '.tmp')
if tmpBase is None:
    sys.exit('-t argument %s does not end with .tmp' % tmpName)

# `lit` supports more substitutions, but these are the only ones needed by the
# tests that use 3c-regtest.py so far.
substitutions = [
    # #_MARKER_# is a hack copied from getDefaultSubstitutions in
    # llvm/utils/lit/lit/TestRunner.py. To explain it a bit more fully:
    # applySubstitutions processes each before/after pair in turn and replaces
    # all occurrences. So if we simply put ('%%', '%') as either the first or
    # last pair, there is a risk of a % being interpreted as part of a %s, etc.
    # Instead, we temporarily replace %% with a string that doesn't contain %
    # and that we assume doesn't occur elsewhere in the commands. The "proper"
    # way to implement this kind of substitution processing is to make one pass
    # over the input from left to right, replacing codes as they are found, but
    # apparently that wasn't worth the extra code in `lit`.
    ('%%', '#_MARKER_#'),
    ('%s', argobj.test_file),
    ('%S', test_dir),
    ('%t', tmpName),
    ('%clang', argobj.clang),
    ('#_MARKER_#', '%')
]

commands = [cmd for cmd in script_generator.generate_commands(argobj) if cmd != '']
commands = lit.TestRunner.applySubstitutions(commands, substitutions)

class FakeTestConfig:
    def __init__(self):
        self.pipefail = True  # Is this always OK?
        self.environment = dict(os.environ)

class FakeTest:
    def __init__(self):
        self.config = FakeTestConfig()

class FakeLitConfig:
    def __init__(self):
        self.isWindows = platform.system() == 'windows'
        # Let the calling `lit` handle any timeout.
        self.maxIndividualTestTime = 0

res = lit.TestRunner.executeScriptInternal(
    FakeTest(), FakeLitConfig(), tmpBase, commands, os.getcwd())
if isinstance(res, lit.Test.Result):
    die('Error: executeScriptInternal returned unexpected Result(%s, %r)' %
        (res.code.name, res.output))

out, err, exitCode, timeoutInfo = res
sys.stdout.write(out)
sys.stderr.write(err)
sys.exit(exitCode)

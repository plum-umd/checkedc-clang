#!/bin/bash
# Usage: ./3c-regtest-verify-all-conversions.sh BASE_TREEISH
#
# BASE_TREEISH has the original versions of the tests to compare to. Typically,
# it would be origin/test-command-refactoring.verify-base .
#
# This takes a little while, so you may want to redirect the output to a file
# and view it later

set -e
set -o pipefail
base_commit="$1"
git rev-parse "$base_commit^{tree}" &>/dev/null || { echo >&2 'Invalid base tree'; exit 1; }

for f in $(git grep --files-with-matches '3c-regtest' *.c); do
	./3c-regtest-unconvert.py $f | {
		diff --label $f.orig --label $f.unconvert -u <(git show "$base_commit":./$f) - || [ $? == 1 ]
	}
done

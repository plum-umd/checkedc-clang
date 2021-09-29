#!/bin/bash
set -e

# cd from clang/tools/3c/utils/code_style to the monorepo root.
cd "$(dirname "$0")/../../../../.."

# All of the following duplicates logic in CMakeLists.txt, and the comments
# there apply. TODO: Is there any way to factor out any of the logic?

# WARNING: See the caveats in the "Things to know about `clang-tidy --fix`"
# section of clang/docs/checkedc/3C/clang-tidy.md.
# TODO: Enhance this script to account for any of those caveats?

monorepo_root_regex_quoted="$(sed -E 's,[^A-Za-z0-9],\\&,g' <<<"$PWD")"
# TODO: Support nonstandard build directory locations.
clang-tools-extra/clang-tidy/tool/run-clang-tidy.py \
  -p build \
  -fix \
  -header-filter "^$monorepo_root_regex_quoted/clang/include/clang/3C/" \
  "^$monorepo_root_regex_quoted/clang/(lib/3C|tools/3c)/.*\\.cpp\$"

# clang-tidy may mess up formatting, so clang-format should be run after it.
find clang/{include/clang/3C,lib/3C,tools/3c} \
  '(' -name '*.h' -or -name '*.cpp' ')' \
  -exec clang-format -i {} +

find clang/{test/3C,tools/3c/utils/port_tools} -name '*.py' \
  -exec yapf3 -i {} +

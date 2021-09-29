# The clang/tools/3c/utils/code_style directory contains build targets and
# supporting tools to validate the code style of 3C itself. It is for 3C
# developers, not end users. For now, we only care about whether this code works
# in our preferred Linux environment.

# Note: Several of our build targets (check-3c, lint-3c-clang-tidy) run commands
# that internally run $(nproc) threads but are counted as only 1 job by Ninja.
# When Ninja runs parallel jobs, this could badly overload the system.
# Unfortunately, there doesn't seem to be a way to get Ninja to just count each
# of our internally parallel commands as $(nproc) jobs. The closest we can come
# is to use a job pool
# (https://cmake.org/cmake/help/latest/prop_gbl/JOB_POOLS.html), which would
# still allow Ninja to run some ordinary jobs in parallel with an internally
# parallel job. However, check-3c uses add_lit_target, which is hard-coded to
# use the `console` job pool via USES_TERMINAL, and Microsoft may not look
# kindly on us changing that. So we just specify USES_TERMINAL on our internally
# parallel jobs, even though they don't actually need the terminal. This is a
# bit of a hack but shouldn't cause any problems.

# Target to run all currently automated style checks on 3C. The `lint-` name
# prefix is modeled after lint-libc in libc/CMakeLists.txt. Remember to use
# `ninja -k 0` if you want to see failures from all the style checks during a
# single build.
#
# This target just reports any style violations as errors. You can use the
# corresponding `fix-all.sh` script in this directory to fix as many errors as
# possible using the options of the respective tools.
#
# Some important checks are currently not automated. For more details, see the
# public 3C coding guidelines in clang/docs/checkedc/3C/development.md and the
# CCI engineering handbook section on code style
# (https://docs.google.com/document/d/1lnlUSOdKqXV3f8oG0GS0BLyA6peNNCY-kzRpvF8e8e8/edit#heading=h.ncfvo5mv1ma),
# parts of which have not yet been propagated to the public documentation.
add_custom_target(lint-3c
  DEPENDS
  lint-3c-clang-format
  lint-3c-clang-tidy
  lint-3c-yapf
  )

# The style checks are currently implemented as targets that re-run
# unconditionally on all relevant files at once (like check-3c, in fact).
# Ideally, we would have one target per file and soundly track the dependencies
# to avoid unnecessary repetition of work and improve parallelism, but this
# would take a lot more CMake code; we can always do it later if we care enough.
# There's some prior art for fine-grained targets for clang-tidy in
# libc/cmake/modules/LLVMLibCObjectRules.cmake.

# Currently, we use clang-format, clang-tidy, and clang-apply-replacements from
# the system $PATH. On Ubuntu, you can install them with `apt install
# clang-format clang-tidy clang-tools`.
#
# Users (Matt) who refuse to put Clang executables on the global $PATH (to
# ensure that none of our 3C-related scripts ever accidentally run a system
# Clang executable when they should be using a checkedc-clang executable) will
# need to add appropriate wrapper scripts to $PATH while running these targets.
# We considered building clang-format and clang-tidy from this repository, but
# if the build type is set to Debug, clang-tidy will be unacceptably slow.

add_custom_target(lint-3c-clang-format
  COMMENT "Linting 3C (clang-format)"
  COMMAND
  find
  ${CLANG_SOURCE_DIR}/include/clang/3C
  ${CLANG_SOURCE_DIR}/lib/3C
  # We haven't been keeping clang/test/3C compliant with clang-format
  # (https://correct-computation.slack.com/archives/G01GKGKHMFD/p1615415203010700?thread_ts=1614987194.022900&cid=G01GKGKHMFD).
  # Now that we have automation, should we reconsider this?
  #${CLANG_SOURCE_DIR}/test/3C
  ${CLANG_SOURCE_DIR}/tools/3c
  '\(' -name '*.h' -or -name '*.cpp' '\)'
  -exec clang-format -Werror --dry-run {} +
  )

# Just backslash all non-alphanumeric characters, like Python's `re.escape`.
string(REGEX REPLACE \([^A-Za-z0-9]\) \\\\\\1
  LINT_3C_CLANG_SOURCE_DIR_REGEX_QUOTED ${CLANG_SOURCE_DIR})

add_custom_target(lint-3c-clang-tidy
  COMMENT "Linting 3C (clang-tidy)"
  # Functionally, there's no need to build 3C before running clang-tidy, but
  # this dependency helps avoid overloading the system by running an internally
  # parallel clang-tidy job in parallel with many individual 3C compile jobs.
  # Also, if the code fails to compile, the user will probably want to fix that
  # before worrying about clang-tidy errors.
  DEPENDS 3c
  COMMAND
  # There doesn't seem to be a CLANG_TOOLS_EXTRA_SOURCE_DIR variable.
  ${CLANG_SOURCE_DIR}/../clang-tools-extra/clang-tidy/tool/run-clang-tidy.py
  -clang-tidy-binary ${CMAKE_CURRENT_SOURCE_DIR}/clang-tidy-wrapper.sh
  -p ${CMAKE_BINARY_DIR}
  -header-filter '^${LINT_3C_CLANG_SOURCE_DIR_REGEX_QUOTED}/include/clang/3C/'
  # Apparently Ninja requires the $ to be doubled. Should CMake be doing that
  # for us?
  '^${LINT_3C_CLANG_SOURCE_DIR_REGEX_QUOTED}/\(lib/3C|tools/3c\)/.*\\.cpp$$'
  USES_TERMINAL
  )

# This requires that yapf3 be available on $PATH. On Ubuntu, you can install it
# with `apt install yapf3`.
add_custom_target(lint-3c-yapf
  COMMENT "Linting 3C (yapf)"
  COMMAND
  find
  ${CLANG_SOURCE_DIR}/test/3C
  # ${CLANG_SOURCE_DIR}/tools/3c/utils/*.py are pretty much unmaintained and
  # currently don't comply with yapf. We don't want to format them right now
  # when we aren't maintaining them otherwise.
  ${CLANG_SOURCE_DIR}/tools/3c/utils/port_tools
  -name '*.py'
  -exec yapf3 --diff {} +
  )

# Target to fully validate a 3C change by running the regression tests and
# validating code style.
add_custom_target(validate-3c
  DEPENDS
  check-3c
  lint-3c
  )

// The -nostdlibinc option suppresses the search for include files in standard
// system include directories like /usr/local/include, /usr/include,
// /usr/include/<TARGET_ARCH>. But the <BUILD>/lib/clang/<VERSION>/include
// directory is still searched for include files.
// Therefore, in the context of a Checked C compilation, all the wrapper
// header files listed below are found while the corresponding system header
// file that is included in each of these files (via #include_next) is not
// found when -nostdlibinc is specified on the compilation command line.
//
// This test confirms the above behavior with the following checks:
//   1) The wrapper header files listed below are found.
//   2) The system header file of the same name that each wrapper header file
//      includes (via #include_next), is not found.
// The -MM -MG preprocessor options list the header files not found.
//
// RUN: %clang -target x86_64-unknown-unknown -nostdlibinc -ffreestanding -MM -MG %s | FileCheck %s


#include <assert_checked.h>
// CHECK: assert.h
#include <errno_checked.h>
// CHECK: errno.h
#include <fenv_checked.h>
// CHECK: fenv.h
#include <inttypes_checked.h>
// CHECK: inttypes.h
#include <math_checked.h>
// CHECK: math.h
#include <signal_checked.h>
// CHECK: signal.h
#include <stdio_checked.h>
// CHECK: stdio.h
#include <stdlib_checked.h>
// CHECK: stdlib.h
#include <string_checked.h>
// CHECK: string.h
#include <time_checked.h>
// CHECK: time.h
//
//
// The following four files: threads.h unistd.h sys/socket.h arpa/inet.h
// cannot be added here in this test case because a #if __has_include_next
// guard is already present in each of these files to account for the
// potential absence of the corresponding system header file.


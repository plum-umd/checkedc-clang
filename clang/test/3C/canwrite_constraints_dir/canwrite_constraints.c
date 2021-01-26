// Test that non-canWrite files are constrained not to change so that the final
// annotations of other files are consistent with the original annotations of
// the non-canWrite files.
// (https://github.com/correctcomputation/checkedc-clang/issues/387)
//
// TODO: When https://github.com/correctcomputation/checkedc-clang/issues/327 is
// fixed, rename canwrite_constraints_dir to canwrite_constraints and replace
// the absolute -I option with a .. in the #include directive.
//
// TODO: Windows compatibility?

// "Lower" case: -base-dir should default to the working directory, so we should
// not allow canwrite_constraints.h to change, and the internal type of q should
// remain wild.
//
// RUN: cd %S && 3c -addcr -extra-arg=-I${PWD%/*} -output-postfix=checked -warn-root-cause -verify %s
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_LOWER --input-file %S/canwrite_constraints.checked.c %s
// RUN: test ! -f %S/../canwrite_constraints.checked.h

// "Higher" case: When -base-dir is set to the parent directory, we can change
// canwrite_constraints.h, so both p and q should become checked.
//
// RUN: cd %S && 3c -addcr -extra-arg=-I${PWD%/*} -base-dir=${PWD%/*} -output-postfix=checked %s
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_HIGHER --input-file %S/canwrite_constraints.checked.c %s
// RUN: FileCheck -match-full-lines -check-prefixes=CHECK_HIGHER --input-file %S/../canwrite_constraints.checked.h %S/../canwrite_constraints.h
// RUN: rm %S/canwrite_constraints.checked.c %S/../canwrite_constraints.checked.h

#include "canwrite_constraints.h"

void bar(int *q) {
  // CHECK_LOWER: void bar(int *q : itype(_Ptr<int>)) {
  // CHECK_HIGHER: void bar(_Ptr<int> q) {
  foo(q);
}

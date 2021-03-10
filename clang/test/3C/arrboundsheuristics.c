// Tests for 3C.
//
// Checks wrong array heuristics.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t1.unused -
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S %s -- | %clang -c -fcheckedc-extension -x c -o %t2.unused -

int *glob;
int lenplusone;
#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size)
    : itype(_Array_ptr<T>) byte_count(size);
//CHECK_ALL: _Array_ptr<int> glob = ((void *)0);
//CHECK_NOALL: int *glob;

void foo(int *p, int idx) { p[idx] = 0; }
//CHECK_ALL: void foo(_Array_ptr<int> p, int idx) {
//CHECK_NOALL: void foo(int *p : itype(_Ptr<int>), int idx) {

void bar(int *p, int flag) {
  if (flag & 0x2) {
    p[0] = 0;
  }
}
//CHECK_ALL: void bar(_Array_ptr<int> p, int flag) {
//CHECK_NOALL: void bar(int *p : itype(_Ptr<int>), int flag) {

int gl() {
  int len;
  for (len = lenplusone; len >= 1; len--) {
    glob[len] = 0;
  }
  return 0;
}

int deflen() {
  glob = malloc((lenplusone + 1) * sizeof(int));
  return 0;
}
//CHECK: glob = malloc<int>((lenplusone+1)*sizeof(int));

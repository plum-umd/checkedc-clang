// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t.unused -
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -addcr -alltypes %t.checked/pointerarithm.c -- | diff %t.checked/pointerarithm.c -

#include <stddef.h>
_Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
int printf(const char *restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
_Unchecked char *strcpy(char *restrict dest, const char *restrict src : itype(restrict _Nt_array_ptr<const char>));

int *sus(int *x, int *y) {
  int *z = malloc(sizeof(int) * 2);
  *z = 1;
  x++;
  *x = 2;
  return z;
}
//CHECK: _Array_ptr<int> sus(_Array_ptr<int> x, _Ptr<int> y) : count(2) {
//CHECK-NEXT: _Array_ptr<int> z : count(2) = malloc<int>(sizeof(int) * 2);

int *foo() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y);
  *z = *z + 1;
  return z;
}
//CHECK: _Ptr<int> foo(void) {
//CHECK: _Ptr<int> y = &sy;
//CHECK: _Ptr<int> z = sus(_Assume_bounds_cast<_Array_ptr<int>>(x, byte_count(0)), y);

int *bar() {
  int sx = 3, sy = 4, *x = &sx, *y = &sy;
  int *z = sus(x, y) + 2;
  *z = -17;
  return z;
}
//CHECK: _Ptr<int> bar(void) {
//CHECK: _Ptr<int> y = &sy;
//CHECK: _Ptr<int> z = sus(_Assume_bounds_cast<_Array_ptr<int>>(x, byte_count(0)), y) + 2;

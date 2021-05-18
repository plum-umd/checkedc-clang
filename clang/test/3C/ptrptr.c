// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/ptrptr.c -- | diff %t.checked/ptrptr.c -

#include <stddef.h>
_Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
int printf(const char *restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
_Unchecked char *strcpy(char *restrict dest, const char *restrict src : itype(restrict _Nt_array_ptr<const char>));

void f() {
  int x[5];
  //CHECK: int x[5];
  int *pa = x;
  //CHECK: int *pa = x;
  pa += 2;
  int *p = pa;
  //CHECK: int *p = pa;
  p = (int *)5;
  //CHECK: p = (int *)5;
}

void g() {
  int *x = malloc(sizeof(int) * 1);
  //CHECK: int *x = malloc<int>(sizeof(int) * 1);
  int y[5];
  //CHECK: int y[5];
  int **p = &x;
  //CHECK: _Ptr<int *> p = &x;
  int **r = 0;
  //CHECK: _Ptr<int *> r = 0;
  *p = y;
  (*p)[0] = 1;
  r = p;
  **r = 1;
}

void foo(void) {
  int x;
  int *y = &x;
  //CHECK: _Ptr<int> y = &x;
  int **z = &y;
  //CHECK: _Ptr<_Ptr<int>> z = &y;

  int *p = &x;
  //CHECK: int *p = &x;
  int **q = &p;
  //CHECK: int **q = &p;
  q = (int **)5;
  //CHECK: q = (int **)5;

  int *p2 = &x;
  //CHECK: int *p2 = &x;
  p2 = (int *)5;
  //CHECK: p2 = (int *)5;
  int **q2 = &p2;
  //CHECK: _Ptr<int *> q2 = &p2;
}

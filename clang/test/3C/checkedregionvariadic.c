// RUN: 3c -base-dir=%S -addcr  %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

#include <stdarg.h>
#include <stddef.h>

void sum(int *ptr, int count, ...) {
  //CHECK: void sum(_Ptr<int> ptr, int count, ...) {
  va_list ap;
  int sum = 0;

  va_start(ap, count);

  for (int i = 0; i < count; i++)
    sum += va_arg(ap, int);

  *ptr = sum;
  va_end(ap);
  return;
}

void bad_sum(int **ptr, int count, ...) {
  // The expected new type for `ptr` was previously `_Ptr<int*>`. Presumably the
  // reason we aren't getting that on the "unchecked" side now is
  // https://github.com/correctcomputation/checkedc-clang/issues/704.
  //CHECK: void bad_sum(int **ptr : itype(_Ptr<_Ptr<int>>), int count, ...) {
  va_list ap;
  int sum = 0;

  *ptr = (int *)3;

  va_start(ap, count);

  for (int i = 0; i < count; i++)
    sum += va_arg(ap, int);

  **ptr = sum;
  va_end(ap);
  return;
}

int foo(int *ptr) {
  //CHECK: int foo(_Ptr<int> ptr) _Checked {
  *ptr += 2;
  sum(ptr, 1, 2, 3);
  //CHECK: _Unchecked { sum(ptr, 1, 2, 3); };
  return *ptr;
}

int bar(int *ptr) {
  //CHECK: int bar(_Ptr<int> ptr) _Checked {
  *ptr += 2;
  bad_sum(&ptr, 1, 2, 3);
  //CHECK: _Unchecked { bad_sum(&ptr, 1, 2, 3); };
  return *ptr;
}

void baz(void) {
  //CHECK: void baz(void) {
  sum(NULL, 3, 1, 2, 3);
  //CHECK: sum(NULL, 3, 1, 2, 3);
}

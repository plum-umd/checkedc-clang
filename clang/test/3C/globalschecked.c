// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

int foo(void *x) {
  int *p = (int *)x;
  return *p;
}

static int (*F)(void *) = foo;

int f(void) {
  //CHECK: int f(void) {
  int y = 2;
  F(&y);
  return 2;
}

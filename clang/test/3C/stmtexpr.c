// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o %t.unused -

int foo(void) {
  //CHECK: int foo(void) {
  return (__extension__({
    int *x = (int *)3;
    *x;
  }));
}

// Dummy function to ensure output
int dummy(int *x) {
  //CHECK: int dummy(_Ptr<int> x) _Checked {
  return *x;
}

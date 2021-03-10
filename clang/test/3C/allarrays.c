// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines --check-prefixes="CHECK" %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t1.unused -

int *foo();
//CHECK: _Array_ptr<int> foo(_Array_ptr<int> r);
void bar(void) {
  int *p = 0;
  //CHECK: _Array_ptr<int> p = 0;
  int *q = foo(p);
  //CHECK: _Array_ptr<int> q = foo(p);
  q[1] = 0;
}
int *foo(int *r);
//CHECK: _Array_ptr<int> foo(_Array_ptr<int> r);
void baz(int *t) {
  //CHECK: void baz(_Array_ptr<int> t) {
  foo(t);
}
int *foo(int *r) {
  //CHECK: _Array_ptr<int> foo(_Array_ptr<int> r) {
  return r;
}

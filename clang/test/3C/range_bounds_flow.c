// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | %clang -c -Xclang -verify -Wno-unused-value -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/range_bounds_flow.c -- | diff %t.checked/range_bounds_flow.c -

// `a` is inferred as the lower bound for `b` and `c`.
void test1() {
  int *a;
  //CHECK_ALL: _Array_ptr<int> a : count(0 + 1) = ((void *)0);
  a[0];

  int *b = a;
  b++;
  //CHECK_ALL: _Array_ptr<int> b : bounds(a, a + 0 + 1) = a;

  int *c = b;
  //CHECK_ALL: _Array_ptr<int> c : bounds(a, a + 0 + 1) = b;
  c[0];
}


// Here we need to add a temporary lower bound instead.
void test2() {
  int *a;
  //CHECK_ALL: _Array_ptr<int> __3c_tmp_a : count(0 + 1) = ((void *)0);
  //CHECK_ALL: _Array_ptr<int> a : bounds(__3c_tmp_a, __3c_tmp_a + 0 + 1) = __3c_tmp_a;
  a[0];
  a++;

  int *b = a;
  //CHECK_ALL: _Array_ptr<int> b : bounds(__3c_tmp_a, __3c_tmp_a + 0 + 1) = a;

  int *c = b;
  //CHECK_ALL: _Array_ptr<int> c : bounds(__3c_tmp_a, __3c_tmp_a + 0 + 1) = b;
  c[0];
}

int *test3(int *a, int l) {
  int *b = a;
  // CHECK_ALL: _Array_ptr<int> test3(_Array_ptr<int> a : count(l), int l) : bounds(a, a + l) _Checked {
  // CHECK_ALL: _Array_ptr<int> b : bounds(a, a + l) = a;
  b++;
  return b;
}

int *test4(int *, int);
int *test4(int *x, int l);
int *test4();
// CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_tmp_a : count(l), int l) : bounds(__3c_tmp_a, __3c_tmp_a + l);
// CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_tmp_a : count(l), int l) : bounds(__3c_tmp_a, __3c_tmp_a + l);
// CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_tmp_a : count(l), int l) : bounds(__3c_tmp_a, __3c_tmp_a + l);

int *test4(int *a, int l) {
  // CHECK_ALL: _Array_ptr<int> test4(_Array_ptr<int> __3c_tmp_a : count(l), int l) : bounds(__3c_tmp_a, __3c_tmp_a + l) _Checked {
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_tmp_a, __3c_tmp_a + l) = __3c_tmp_a;
  a++;
  return a;
}

// There are multiple possible lower bounds for `c`, but they are consistent
// with each other.
void test5(int *a, int l) {
  int *b = a;
  int *c = b;
  // CHECK_ALL: void test5(_Array_ptr<int> a : count(l), int l) _Checked {
  // CHECK_ALL: _Array_ptr<int> b : count(l) = a;
  // CHECK_ALL: _Array_ptr<int> c : bounds(b, b + l) = b;
  c++;
}

// Lower bounds aren't consistent. We can't use `a` or `b`, so a fresh lower
// bound is created  g.
void test6() {
  int *a;
  int *b;
  // CHECK_ALL: _Array_ptr<int> a : count(0 + 1) = ((void *)0);
  // CHECK_ALL: _Array_ptr<int> b : count(0 + 1) = ((void *)0);

  int *c;
  c = a;
  c = b;
  // CHECK_ALL: _Array_ptr<int> __3c_tmp_c : count(0 + 1) = ((void *)0);
  // CHECK_ALL: _Array_ptr<int> c : bounds(__3c_tmp_c, __3c_tmp_c + 0 + 1) = __3c_tmp_c;
  // CHECK_ALL: __3c_tmp_c = a, c = __3c_tmp_c;
  // CHECK_ALL: __3c_tmp_c = b, c = __3c_tmp_c;

  c++;
  c[0];
}

// Lower bound is inferred from pointer with an declared count bound.
void test7(int *a : count(l), int dummy, int l) {
  int *b = a;
  // CHECK_ALL: _Array_ptr<int> b : bounds(a, a + l) = a;
  b++;
}

// Context sensitive edges should not cause `c` to be a lower bound for `b`.
void testx(int *a){ a[0]; }
void otherxx(){
  int *b;
  int *c;
  //CHECK_ALL: _Array_ptr<int> __3c_tmp_b : count(0 + 1) = ((void *)0);
  //CHECK_ALL: _Array_ptr<int> b : bounds(__3c_tmp_b, __3c_tmp_b + 0 + 1) = __3c_tmp_b;
  //CHECK_ALL: _Array_ptr<int> c : count(0 + 1) = ((void *)0);

  testx(b);
  testx(c);
  b[0];
  c[0];
  b++;
}

struct structy { int *b; };
// CHECK_ALL: struct structy { _Array_ptr<int> b; };
void testy(struct structy d) {
  // expected-error@+2 {{inferred bounds for '__3c_tmp_e' are unknown after initialization}}
  // expected-note@+1 {{}}
  int *e = d.b;
  // CHECK_ALL: _Array_ptr<int> __3c_tmp_e : count(0 + 1) = d.b;
  // CHECK_ALL: _Array_ptr<int> e : bounds(__3c_tmp_e, __3c_tmp_e + 0 + 1) = __3c_tmp_e;

  d.b = e;
  e++;

  e[0];
}

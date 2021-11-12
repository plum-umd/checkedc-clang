// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | %clang -c -Xclang -verify -Wno-unused-value -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/range_bounds_flow.c -- | diff %t.checked/range_bounds_flow.c -

// expected-no-diagnostics

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

// Context sensitive edges should not cause `c` to be a lower bound for `b`.
void test(int *a){ a[0]; }
void other(){
  int *b;
  int *c;
  //CHECK_ALL: _Array_ptr<int> __3c_tmp_b : count(0 + 1) = ((void *)0);
  //CHECK_ALL: _Array_ptr<int> b : bounds(__3c_tmp_b, __3c_tmp_b + 0 + 1) = __3c_tmp_b;
  //CHECK_ALL: _Array_ptr<int> c : count(0 + 1) = ((void *)0);

  test(b);
  test(c);
  b[0];
  c[0];
  b++;
}

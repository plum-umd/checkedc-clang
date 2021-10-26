// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | %clang -c -Xclang -verify -Wno-unused-value -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/range_bounds.c -- | diff %t.checked/range_bounds.c -

#include<stdlib.h>

void test0(size_t l) {
  // Would get bounds, but there's pointer arithmetic. Now we split and use
  // range bound.
  int *p = malloc(l * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> __3c_tmp_p : count(l) = malloc<int>(l * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> p : bounds(__3c_tmp_p, __3c_tmp_p + l) = __3c_tmp_p;
  p++;

  // No bounds are inferred, but pointer arithemtic is used; don't split
  int *q = 0;
  // CHECK_ALL :_Array_ptr<int> q;
  q++;
}

// Parameters must be inserted inside function body. This also also checks
// that a pre-declarations gets the correct bounds and does not generate a
// second alias.
void test1(int *a, int l);
// CHECK_ALL: void test1(_Array_ptr<int> a : count(l), int l);
void test1(int *a, int l) {
  // CHECK_ALL: void test1(_Array_ptr<int> __3c_tmp_a : count(l), int l) _Checked {
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_tmp_a, __3c_tmp_a + l) = __3c_tmp_a;

  // Also check that other types assignment are recognized.
  a = a + 1;

  // The increment above means this loop reads out of bounds if `l` is the
  // length of `a`.  3c won't consider this, but, now that we give `a` a bound,
  // the access `a[l-1]` can be caught by Checked C, and the programmer can
  // correct the loop limit or the declared bound as appropriate.
  for(int i = 0; i < l; i++)
    a[i];
}

// Also check for itypes. They're interesting because the alias isn't checked.
void test2(int *a, int l);
// CHECK_ALL: void test2(int *a : itype(_Array_ptr<int>) count(l), int l);
void test2(int *a, int l) {
  // CHECK_ALL: void test2(int *__3c_tmp_a : itype(_Array_ptr<int>) count(l), int l) {
  // CHECK_ALL: int *a  = __3c_tmp_a;
  for(int i = 0; i < l; i++)
    a[i];

  a = a + 2;
  a = (int*) 1;
}

// Something more complex with multiple parameters.
void test3(int *a, int *b, int *c, int *d) {
  // CHECK_ALL: void test3(_Array_ptr<int> __3c_tmp_a : count(10), int *__3c_tmp_b : itype(_Array_ptr<int>) count(10), _Array_ptr<int> __3c_tmp_c : count(10), int *__3c_tmp_d : itype(_Array_ptr<int>) count(10)) {
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_tmp_a, __3c_tmp_a + 10) = __3c_tmp_a;
  // CHECK_ALL: int *b = __3c_tmp_b;
  // CHECK_ALL: _Array_ptr<int> c : bounds(__3c_tmp_c, __3c_tmp_c + 10) = __3c_tmp_c;
  // CHECK_ALL: int *d = __3c_tmp_d;
  a += 1, b += 2, c--, d -= 1;
  b = d = (int*) 1;

  for (int i = 0; i < 10; i++)
    a[i], b[i], c[i], d[i];
}

// Multi declarations might need to a new declarations. The order of the new
// declarations is important because a later declaration in the same multi-decl
// might reference the variable being emmitted. The new declaration of `c` must
// come before `d`.
void test4() {
  int *a = malloc(10*sizeof(int)), b, *c = malloc(10*sizeof(int)), *d = c;
  // CHECK_ALL: _Array_ptr<int> __3c_tmp_a : count(10) = malloc<int>(10*sizeof(int));
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_tmp_a, __3c_tmp_a + 10) = __3c_tmp_a;
  // CHECK_ALL: int b;
  // CHECK_ALL: _Array_ptr<int> __3c_tmp_c : count(10) = malloc<int>(10*sizeof(int));
  // CHECK_ALL: _Array_ptr<int> c : bounds(__3c_tmp_c, __3c_tmp_c + 10) = __3c_tmp_c;
  // CHECK_ALL: _Ptr<int> d = c;

  b;
  a++, c++;
}

// Test that bounds don't propagate through pointers with assigned to from
// pointer arithmetic. In this example, `b` can *not* have bounds `count(2)`.
// TODO: `b` could get `bounds(__3c_tmp_a, __3c_tmp_a + 2)`.
// The same restriction also applies to bounds on the return, but it is is not
// clear how range bounds could be assigned to the return.
int *test5() {
  // CHECK_ALL: _Array_ptr<int> test5(void) {
  int *a = malloc(2 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> __3c_tmp_a : count(2) = malloc<int>(2 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> a : bounds(__3c_tmp_a, __3c_tmp_a + 2) = __3c_tmp_a;
  a++;
  int *b = a;
  // CHECK_ALL: _Array_ptr<int> b : count(0 + 1) = a;
  // expected-error@-2 {{it is not possible to prove that the inferred bounds of 'b' imply the declared bounds of 'b' after initialization}}
  // expected-note@-3 4 {{}}
  b[0];

  return a;
}

// Assignments to the variable should update the original and the copy, as long
// as the value being assigned doesn't depend on the pointer.
void test6() {
  int *p = malloc(10 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> __3c_tmp_p : count(10) = malloc<int>(10 * sizeof(int));
  // CHECK_ALL: _Array_ptr<int> p : bounds(__3c_tmp_p, __3c_tmp_p + 10) = __3c_tmp_p;
  p++;

  // This assignment isn't touched because `p` is on the RHS.
  p = p + 1;
  // CHECK_ALL: p = p + 1;

  // Null out `p`, so we need to null the original and the duplicate.
  p = 0;
  // CHECK_ALL: __3c_tmp_p = 0, p = __3c_tmp_p;

  // A slightly more complex update to a different pointer value.
  int *q = malloc(10 * sizeof(int));
  p = q;
  // CHECK_ALL: _Array_ptr<int> q : count(10) = malloc<int>(10 * sizeof(int));
  // CHECK_ALL: __3c_tmp_p = q, p = __3c_tmp_p;

  // Don't treat a call to realloc as pointer arithmetic. Freeing `p` after
  // `p++` is highly questionable, but that's not the point here.
  p = realloc(p, 10 * sizeof(int));
  // CHECK_ALL: __3c_tmp_p = realloc<int>(p, 10 * sizeof(int)), p = __3c_tmp_p;

  // Assignment rewriting should work in more complex expression and around
  // other 3C rewriting without breaking anything.
  int *v = 1 + (p = (int*) q, p = p + 1) + 1;
  // CHECK_ALL: _Ptr<int> v = 1 + (__3c_tmp_p = (_Array_ptr<int>) q, p = __3c_tmp_p, p = p + 1) + 1;
}


// Check interaction with declaration merging. Identifiers are added on the
// first two declarations even though it's not required.
void test7(int *);
void test7();
void test7(int *a);
// CHECK_ALL: void test7(_Array_ptr<int> s : count(5));
// CHECK_ALL: void test7(_Array_ptr<int> s : count(5));
// CHECK_ALL: void test7(_Array_ptr<int> a : count(5));
void test7(int *s) {
// CHECK_ALL: void test7(_Array_ptr<int> __3c_tmp_s : count(5)) _Checked {
// CHECK_ALL: _Array_ptr<int> s : bounds(__3c_tmp_s, __3c_tmp_s + 5) = __3c_tmp_s;
  s++;
  for (int i = 0; i < 5; i++)
    s[i];
}

// A structure field is handled as it was before implementing range bounds.
// Future work could insert a new field, and update all struct initializer to
// include it.
struct s {
  int *a;
  // CHECK_ALL: _Array_ptr<int> a;
};
void test8() {
  struct s t;
  t.a++;
  t.a[0];
  // expected-error@-1 {{expression has unknown bounds}}
}

// Same as above. Future work might figure out how to emit range bounds for
// global variables.
int *glob;
// CHECK_ALL: _Array_ptr<int> glob = ((void *)0);
void test9() {
  glob++;
  glob[0];
  // expected-error@-1 {{expression has unknown bounds}}
}

// Creating a temporary local `int a _Checked[10]` would be incorrect here
// because the local variable array type does not decay to a pointer type.
// Future work can instead use checked array pointer type for the local.
void test10(int a[10]) {
// CHECK_ALL: void test10(int a _Checked[10]) _Checked {
// expected-note@-2 {{}}

  // A warning is expected because we keep the bound for the checked array. It
  // could instead be rewritten to `_Array_ptr<int> a` without a bound.
  a++;
  // expected-warning@-1 {{cannot prove declared bounds for 'a' are valid after increment}}
  // expected-note@-2 {{}}
}

// Another case where range bounds aren't allowed: if we would have to update
// an assignment expression in a macro (which would be a rewriting error), then
// the pointer cannot get range bounds.
#define set_a_to_null a = 0
#define null_with_semi 0;
#define another_macro d =
void test11(size_t n){
  int *a = malloc(sizeof(int) * n);
  //CHECK: _Array_ptr<int> a = malloc<int>(sizeof(int) * n);
  a++;
  set_a_to_null;

  // The LHS is a macro, but we should still be able to rewrite.
  int *b = malloc(sizeof(int) * n);
  // CHECK: _Array_ptr<int> __3c_tmp_b : count(n) = malloc<int>(sizeof(int) * n);
  // CHECK: _Array_ptr<int> b : bounds(__3c_tmp_b, __3c_tmp_b + n) = __3c_tmp_b;
  b++;
  b = NULL;
  // CHECK: __3c_tmp_b = NULL, b = __3c_tmp_b;

  // Like the above case, but the macro includes the semicolon, so we can't
  // rewrite.
  int *c = malloc(sizeof(int) * n);
  // CHECK: _Array_ptr<int> c = malloc<int>(sizeof(int) * n);
  c++;
  c = null_with_semi

  int *d = malloc(sizeof(int) * n);
  // CHECK: _Array_ptr<int> d = malloc<int>(sizeof(int) * n);
  d++;
  another_macro 0;
}

// Check byte_count rewriting. Interesting because range bound needs a cast to
// `_Array_ptr<char>` for pointer arithmetic with offset to be correct.
void byte_count_fn(int *a : byte_count(n), unsigned int n);
void test12(int *b, unsigned int n) {
// CHECK: void test12(_Array_ptr<int> __3c_tmp_b : byte_count(n), unsigned int n) _Checked {
// CHECK: _Array_ptr<int> b : bounds(((_Array_ptr<char>)__3c_tmp_b), ((_Array_ptr<char>)__3c_tmp_b) + n) = __3c_tmp_b;

  byte_count_fn(b, n);
  b++;
}

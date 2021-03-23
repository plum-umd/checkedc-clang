// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/fn_sets.c -- | diff %t.checked/fn_sets.c -
/* Tests relating to issue #86 Handling sets of functions */

/* In the first test case, y WILD due to the  (int*)5 assignment. This
 propagates to everything else. */

int * f(int *x) {
	//CHECK: int *f(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  return x;
}

int * g(int *y) {
	//CHECK: int *g(int *y : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  y = (int*)5;
	//CHECK: y = (int*)5;
  return 0;
}

void foo(int *z) {
	//CHECK: void foo(_Ptr<int> z) _Checked {
  int *w = (0 ? f : g)(z);
	//CHECK: _Ptr<int> w = (0 ? f : g)(z);
}


/* The second case verifies that the pointer are still marked checked in the
 absence of anything weird. */

int * f1(int *x) {
	//CHECK: _Ptr<int> f1(_Ptr<int> x) _Checked {
  return x;
}

int * g1(int *y) {
	//CHECK: _Ptr<int> g1(_Ptr<int> y) _Checked {
  return 0;
}

void foo1(int *z) {
	//CHECK: void foo1(_Ptr<int> z) _Checked {
  int *w = (0 ? f1 : g1)(z);
	//CHECK: _Ptr<int> w = (0 ? f1 : g1)(z);
}


/* Testing Something with a larger set of functions */

int *a() {
	//CHECK: int *a(void) : itype(_Ptr<int>) _Checked {
  return 0;
}
int *b() {
	//CHECK: int *b(void) : itype(_Ptr<int>) _Checked {
  return 0;
}
int *c() {
	//CHECK: int *c(void) : itype(_Ptr<int>) _Checked {
  return 0;
}
int *d() {
	//CHECK: int *d(void) : itype(_Ptr<int>) _Checked {
  return 0;
}
int *e() {
	//CHECK: int *e(void) : itype(_Ptr<int>) {
  return (int*) 1;
	//CHECK: return (int*) 1;
}
int *i() {
	//CHECK: _Ptr<int> i(void) _Checked {
  return 0;
}

void bar() {
	//CHECK: void bar() _Checked {
  int *w = (0 ? (0 ? a : b) : (0 ? c : (0 ? d : e)))();
	//CHECK: _Ptr<int> w = (0 ? (0 ? a : b) : (0 ? c : (0 ? d : e)))();
  int *x = a();
	//CHECK: _Ptr<int> x = a();
  int *y = i();
	//CHECK: _Ptr<int> y = i();
}

// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -output-postfix=checked -alltypes %s
// RUN: 3c -alltypes %S/fn_sets.checked.c -- | count 0
// RUN: rm %S/fn_sets.checked.c
/* Tests relating to issue #86 Handling sets of functions */

/* In the first test case, y WILD due to the  (int*)5 assignment. This
 propagates to everything else. */

int * f(int *x) {
	//CHECK: int * f(int *x) {
  return x;
}

int * g(int *y) {
	//CHECK: int * g(int *y) {
  y = (int*)5;
	//CHECK: y = (int*)5;
  return 0;
}

void foo(int *z) {
	//CHECK: void foo(int *z) {
  int *w = (0 ? f : g)(z);
	//CHECK: int *w = (0 ? f : g)(z);
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
	//CHECK: _Ptr<int> w =  (0 ? f1 : g1)(z);
}


/* Testing Something with a larger set of functions */

int *a() {
	//CHECK: int *a(void)  {
  return 0;
}
int *b() {
	//CHECK: int *b(void)  {
  return 0;
}
int *c() {
	//CHECK: int *c(void)  {
  return 0;
}
int *d() {
	//CHECK: int *d(void)  {
  return 0;
}
int *e() {
	//CHECK: int *e(void) {
  return (int*) 1;
	//CHECK: return (int*) 1;
}
int *i() {
	//CHECK: _Ptr<int> i(void) _Checked  {
  return 0;
}

void bar() {
  int *w = (0 ? (0 ? a : b) : (0 ? c : (0 ? d : e)))();
	//CHECK: int *w = (0 ? (0 ? a : b) : (0 ? c : (0 ? d : e)))();
  int *x = a();
	//CHECK: int *x = a();
  int *y = i();
	//CHECK: _Ptr<int> y =  i();
}

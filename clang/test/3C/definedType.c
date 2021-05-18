// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/definedType.c -- | diff %t.checked/definedType.c -

#include <stdlib.h>

// From issue 204

// sys/types.h declares ulong at least on some systems, so don't declare that
// name ourselves.
#define my_ulong unsigned long

my_ulong *TOP;
// CHECK_NOALL: my_ulong *TOP;
// CHECK_ALL: _Array_ptr<my_ulong> TOP = ((void *)0);
my_ulong channelColumns;

void DescribeChannel(void) {
  my_ulong col;
  TOP = (my_ulong *)malloc((channelColumns + 1) * sizeof(my_ulong));
  // CHECK_ALL: TOP = (_Array_ptr<unsigned long>)malloc<unsigned long>((channelColumns + 1) * sizeof(my_ulong));
  // CHECK_NOALL: TOP = (my_ulong *)malloc<unsigned long>((channelColumns + 1) * sizeof(my_ulong));
  TOP[col] = 0;
}

#define integer int
integer foo(int *p, int l) {
  // CHECK_ALL: integer foo(_Array_ptr<int> p : count(l), int l)  _Checked {
  // CHECK_NOALL: integer foo(int *p : itype(_Ptr<int>), int l) {
  return p[l - 1];
}

int *bar(integer p, integer i) {
  // CHECK: _Ptr<int> bar(integer p, integer i) _Checked {
  return 0;
}

// Macros containing only the base type are kept in checked pointer

#define baz unsigned int

baz a;
// CHECK: baz a;
baz *b;
// CHECK: _Ptr<baz> b = ((void *)0);
baz **c;
// CHECK: _Ptr<_Ptr<baz>> c = ((void *)0);
baz d[1];
// CHECK_ALL: baz d _Checked[1];
baz *e[1];
// CHECK_ALL: _Ptr<baz> e _Checked[1] = {((void *)0)};
baz **f[1];
// CHECK_ALL: _Ptr<_Ptr<baz>> f _Checked[1] = {((void *)0)};
baz (*g)[1];
// CHECK_ALL: _Ptr<baz _Checked[1]> g = ((void *)0);
baz h[1][1];
// CHECK_ALL: baz h _Checked[1] _Checked[1];

baz *i() {
  // CHECK: _Ptr<baz> i(void) _Checked {
  return 0;
}

baz **j() {
  // CHECK: _Ptr<_Ptr<baz>> j(void) _Checked {
  return 0;
}

void k(baz x, baz *y, baz **z) {}
// COM: void k(baz x, _Ptr<baz> y, _Ptr<_Ptr<baz>> z) _Checked {}

// Macros are inlined if there's a pointer in the macro
// This could probably be handled better in the future.

#define buz int *

buz l;
// CHECK: _Ptr<int> l = ((void *)0);

buz *m;
// CHECK: _Ptr<_Ptr<int>> m = ((void *)0);

// Macro should not change when wild

buz n = (buz)1;
// CHECK: buz n = (buz)1;

// This was a regression in lua. The function type is wrapped in a ParenType.
int *(lua_test0)() {
  // CHECK: _Ptr<int> lua_test0(void) _Checked {
  return 0;
}
baz *(lua_test1)() {
  // CHECK: _Ptr<baz> lua_test1(void) _Checked  {
  return 0;
}

baz(*lua_test2);
// CHECK: _Ptr<baz> lua_test2 = ((void *)0);

baz(*(*lua_test3));
// CHECK: _Ptr<_Ptr<baz>> lua_test3 = ((void *)0);

typedef int *StkId;
void lua_test4(StkId *x) {}
//CHECK: void lua_test4(_Ptr<StkId> x) _Checked {}

// Things declared inside macros should be WILD unless we start doing something
// extremely clever

// clang-format messes up this part of the file because it mistakes macro
// references for other syntactic constructs.
// clang-format off

#define declare_function(x)                                                    \
  int *foo##x(int *a) { return a; }                                            \
  int *bar##x(int *a) { return a; }

declare_function(1)

#define declare_var(x)                                                         \
  int *x##1;                                                                   \
  int **x##2;                                                                  \
  int ***x##3;

declare_var(y)

void test() {
  int *x = 0;
  int *y = 0;
  int *a = foo1(x);
  int *b = bar1(y);
  int *c = y1;
  int **d = y2;
  int ***e = y3;
}
// CHECK: void test() {
// CHECK: int *x = 0;
// CHECK: int *y = 0;
// CHECK: int *a = foo1(x);
// CHECK: int *b = bar1(y);
// CHECK: int *c = y1;
// CHECK: int **d = y2;
// CHECK: int ***e = y3;

#define parm_decl int *a, int *b

void parm_test(parm_decl) {}
// CHECK: void parm_test(parm_decl) {}

#define declare_single_var(x) int *x = 0;
int *another_test(void) {
  // CHECK: int *another_test(void) : itype(_Ptr<int>) {
  declare_single_var(z)
  // CHECK: declare_single_var(z)
  declare_single_var(y)
  // CHECK: declare_single_var(y)
  return z;
}

// clang-format on

// RUN: 3c -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -alltypes -output-postfix=checked %s
// RUN: 3c -alltypes %S/defn_then_decl.checked.c -- | count 0
// RUN: rm %S/defn_then_decl.checked.c

// Tests behavior when a function declaration appears after the definition for
// that function. A specific issue that existed caused constraints applied to
// the declaration (for example, when the declaration is in a macro) to not
// be correctly applied to the original definition.

void test0(int *p) {}
#define MYDECL void test0(int *p);
MYDECL

void test1(int *p) {}
//CHECK: void test1(_Ptr<int> p) _Checked {}
void test1(int *p);
//CHECK: void test1(_Ptr<int> p);

void test2(int *p);
//CHECK: void test2(_Ptr<int> p);
void test2(int *p) {}
//CHECK: void test2(_Ptr<int> p) _Checked {}
void test2(int *p);
//CHECK: void test2(_Ptr<int> p);

void test3(int *p) { p = 1; }
//CHECK: void test3(int *p : itype(_Ptr<int>)) { p = 1; }
void test3(int *p);
//CHECK: void test3(int *p : itype(_Ptr<int>));

void test4(int *p) {}
//CHECK: void test4(_Ptr<int> p) _Checked {}
void test4();
//CHECK: void test4(_Ptr<int> p);

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/itype_undef.c -- | diff %t.checked/itype_undef.c -

// Basic checks for adding itypes on undefined functions.
void test0(int *a);
//CHECK: void test0(int *a : itype(_Ptr<int>));

int *test1();
//CHECK: int *test1(void) : itype(_Ptr<int>);

void caller() {
//CHECK: void caller() _Checked {
  int *a = 0;
  //CHECK: _Ptr<int> a = 0;
  test0(a);

  // This block is left unchecked because it calls a function without either a
  // defintion or a prototype even though a `void` prototype is added durring
  // conversion. Checked region insertion could be improved to recognize this
  // as safe.
  {
  // CHECK: _Unchecked {
  int *b = test1();
  //CHECK: _Ptr<int> b = test1();
  }
}

// Check for undef functions with existing checked types/itypes.
void test2(int* a : itype(_Ptr<int>));
int *test3(void) : itype(_Ptr<int>);
void test4(_Ptr<int> a);
_Ptr<int> test5(void);
int *test6(void) : count(10);
void test7(int * : count(10));
//CHECK: void test2(int* a : itype(_Ptr<int>));
//CHECK: int *test3(void) : itype(_Ptr<int>);
//CHECK: void test4(_Ptr<int> a);
//CHECK: _Ptr<int> test5(void);
//CHECK: int *test6(void) : count(10);
//CHECK: void test7(int * : count(10));

// Void pointers should still be fully unchecked unless there is an existing
// checked type/itype.
void test_void0(void *);
void *test_void1();
//CHECK: void test_void0(void *);
//CHECK: void *test_void1();
void void_caller() {
  void * a = 0;
  test_void0(a);
  void *b = test_void1();
}

void test_void_itype(void * : itype(_Array_ptr<void>));
void test_void_count(void * : byte_count(0));
void test_void_checked(_Array_ptr<void>);
//CHECK: void test_void_itype(void * : itype(_Array_ptr<void>));
//CHECK: void test_void_count(void * : byte_count(0));
//CHECK: void test_void_checked(_Array_ptr<void>);

// Pointer type and bounds inference should still work.
int *test_arr0(int n);
//CHECK_ALL: int *test_arr0(int n) : itype(_Array_ptr<int>) count(n);
//CHECK_NOALL: int *test_arr0(int n) : itype(_Ptr<int>);

void test_arr1(int *a, int n);
//CHECK: void test_arr1(int *a : itype(_Ptr<int>), int n);

void arr_caller(int s) {
//CHECK_ALL: void arr_caller(int s) _Checked {
  // Returns variables use the least solution, so this will cause test_arr0 to
  // solve to _Array_ptr.
  int *b = test_arr0(s);
  //CHECK_ALL: _Array_ptr<int> b : count(s) = test_arr0(s);
  //CHECK_NOALL: int *b = test_arr0(s);
  for (int i = 0; i < s; i++)
    b[i];

  // Parameter variables use the greatest solution, so test_arr1 will still
  // solve to _Ptr. It might be a good idea to change the solution used for
  // undefined functions so that we can infer _Array_ptr here and infer it a
  // bound.
  int *c = 0;
  //CHECK_ALL: _Array_ptr<int> c : count(s) = 0;
  //CHECK_NOALL: int *c = 0;
  test_arr1(c, s);
  for (int i = 0; i < s; i++)
    c[i];
}

// If all uses of the typedef are checked, the unchecked internal constraint
// variable of the undefined function should not make the typedef unchecked.

typedef int *td0;
void typedef0(td0 a);
void typedef_caller0() {
//CHECK: typedef _Ptr<int> td0;
//CHECK: void typedef0(int *a : itype(td0));
//CHECK: void typedef_caller0() _Checked {
  td0 b = 0;
  typedef0(b);
}

// If the typedef is otherwise unchecked, we should still give a useful
// checked itype to the typedef'ed parameter.

typedef int *td1;
void typedef1(td1 a);
void typedef_caller1() {
//CHECK: typedef int *td1;
//CHECK: void typedef1(td1 a : itype(_Ptr<int>));
//CHECK: void typedef_caller1() {
  td1 b = 1;
  typedef1(b);
}

// As expected, function typedefs aren't handled well, but they should generate
// valid code.
typedef void fn_typedef0(int *);
fn_typedef0 fntd_decl0;
//CHECK: void fntd_decl0(int * : itype(_Ptr<int>));

typedef int *fn_typedef1();
fn_typedef1 fntd_decl1;
//CHECK: int *fntd_decl1(void) : itype(_Ptr<int>);

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -verify -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -verify -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -verify -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -verify -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -verify  -alltypes %t.checked/macro_function_call.c -- | diff %t.checked/macro_function_call.c -

// Test fix for https://github.com/correctcomputation/checkedc-clang/issues/439
// We cannot insert casts on function calls inside macros, so constraints must
// be generated in way that these calls are accepted by CheckedC without
// additional casts.

// 3C emits a warning if it fails inserting a cast. Ensure the test fails if
// this happens.
// expected-no-diagnostics

// Unsafe call in macro. This would require an _Assume_bounds_cast, but we
// can't insert it.  Constraints are generated so that the cast isn't needed.

#define macro0 \
  void caller0(int *a) {\
    fn0(a);\
  }
void fn0(int *b) {}
macro0

//CHECK: #define macro0 \
//CHECK:   void caller0(int *a) {\
//CHECK:     fn0(a);\
//CHECK:   }
//CHECK: void fn0(int *b) {}
//CHECK: macro0


// Same as above, but the cast is requried on the return.

#define macro1 fn1();
int *fn1(void) { return 0; }
void caller1() {
  int *a = (int *) 1;
  a = macro1
}

//CHECK: #define macro1 fn1();
//CHECK: int *fn1(void) { return 0; }
//CHECK: void caller1() {
//CHECK:   int *a = (int *) 1;
//CHECK:   a = macro1
//CHECK: }

// Checked function call in macro. Doesn't need cast, so no changes required to
// avoid cast.

#define macro2 fn2(a);
void fn2(int *b) {}
void caller2(int *a) {
  macro2
}
//CHECK: #define macro2 fn2(a);
//CHECK: void fn2(_Ptr<int> b) _Checked {}
//CHECK: void caller2(_Ptr<int> a) _Checked {
//CHECK:   macro2
//CHECK: }


// Like the first test case, but pointer to a pointer.

#define macro3 fn3(a);
void fn3(int **g) {}
void caller3() {
  int **a = (int **) 1;
  macro3
}
//CHECK: #define macro3 fn3(a);
//CHECK: void fn3(int **g) {}
//CHECK: void caller3() {
//CHECK:   int **a = (int **) 1;
//CHECK:   macro3
//CHECK: }

// Call to function pointer
#define macro4 fn(a);
void fn4a(int *g) {}
void fn4b(int *g) {}
void caller4() {
  int *a = (int *) 1;
  void (*fn)(int *) =  fn4a;
  if (0)
    fn = fn4b;
  macro4
}
//CHECK: #define macro4 fn(a);
//CHECK: void fn4a(int *g) {}
//CHECK: void fn4b(int *g) {}
//CHECK: void caller4() {
//CHECK:   int *a = (int *) 1;
//CHECK:   _Ptr<void (int *)> fn = fn4a;
//CHECK:   if (0)
//CHECK:     fn = fn4b;
//CHECK:   macro4
//CHECK: }

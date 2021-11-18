// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/attr.c -- | diff %t.checked/attr.c -

// function attributes

__attribute__((disable_tail_calls))
int *a() {
//CHECK: __attribute__((disable_tail_calls)) _Ptr<int> a(void) _Checked {
  return 0;
}

__attribute__((disable_tail_calls))
void b(int *x) {
//CHECK: __attribute__((disable_tail_calls)) void b(_Ptr<int> x) _Checked {
  return;
}

__attribute__((disable_tail_calls))
int *c(int *x) {
//CHECK: __attribute__((disable_tail_calls)) _Ptr<int> c(_Ptr<int> x) _Checked {
  return 0;
}
int *e(int *x)
__attribute__((disable_tail_calls))
//CHECK: __attribute__((disable_tail_calls)) int *e(_Ptr<int> x) : itype(_Ptr<int>)
{
  return 1;
}

__attribute__((no_stack_protector))
int *f(int *x)
__attribute__((disable_tail_calls))
//CHECK: __attribute__((no_stack_protector)) __attribute__((disable_tail_calls)) _Ptr<int> f(_Ptr<int> x)
{
//CHECK: _Checked {
  while (1){}
}

// variable attribute on param

void g(__attribute__((noescape)) int *x) {
//CHECK: void g(__attribute__((noescape)) _Ptr<int> x) _Checked {
  return;
}

void h(__attribute__((noescape)) int *x) {
//CHECK: void h(__attribute__((noescape)) int *x : itype(_Ptr<int>)) {
  x = 1;
}

int *i(__attribute__((noescape)) void *x) {
//CHECK: _For_any(T) _Ptr<int> i(__attribute__((noescape)) _Ptr<T> x) {
  return 0;
}

// variable attribute on local

void j() {
  __attribute__((nodebug)) int *a;
  __attribute__((nodebug)) int *b = 0;
  __attribute__((nodebug)) int *c = 1;

  __attribute__((nodebug)) int *d, *e = 1, **f, g, *h;
//CHECK: __attribute__((nodebug)) _Ptr<int> a = ((void *)0);
//CHECK: __attribute__((nodebug)) _Ptr<int> b = 0;
//CHECK: __attribute__((nodebug)) int *c = 1;
//CHECK: __attribute__((nodebug)) _Ptr<int> d = ((void *)0);
//CHECK: int *e __attribute__((nodebug)) = 1;
//CHECK: __attribute__((nodebug)) _Ptr<_Ptr<int>> f = ((void *)0);
//CHECK: int g __attribute__((nodebug));
//CHECK: __attribute__((nodebug)) _Ptr<int> h = ((void *)0);
}

#define FOO __attribute__((ms_abi))
int *foo()  FOO ;
int *foo() { return 0; }
//CHECK: __attribute__((ms_abi)) _Ptr<int> foo(void) ;
//CHECK: _Ptr<int> foo(void) _Checked { return 0; }

// Attribute parameter is preserved
__attribute__((deprecated("bar"))) int *bar();
int *bar() { return 0; }
//CHECK: __attribute__((deprecated("bar"))) _Ptr<int> bar(void);
//CHECK: __attribute__((deprecated("bar"))) _Ptr<int> bar(void) _Checked { return 0; }

// Because toupper is a standard libary function, it has attributes in the AST
// even though there are none in the source. This was causing issues when
// trying to get the name of the attribute.
int toupper(int c) { return 0; }

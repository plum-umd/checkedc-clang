// Tests for 3C.
//
// Tests rewriting and propagation of Nt_array_ptr constraints across functions.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S -alltypes %s -- | %clang -c -f3c-tool -fcheckedc-extension -x c -o %t1.unused -
//
char *strstr(const char *s1 : itype(_Nt_array_ptr<const char>), const char *s2 : itype(_Nt_array_ptr<const char>)) : itype(_Nt_array_ptr<char>);
// here we test the propagation of constraints
// between functions.

// we test propagation with and without function
// declaration.
// here, ntiptr will be an itype(Nt_ptr)
int funcdecl(char *ntiptr, int *iptr, int *wild);
int funcdecl(char *ntiptr, int *iptr, int *wild) {
  if (ntiptr != 0) {
    ntiptr = strstr("Hello", "world");
  }
  wild = (int *)0xdeadbeef;
  return 0;
}
//CHECK: int funcdecl(_Ptr<char> ntiptr, _Ptr<int> iptr, int *wild : itype(_Ptr<int>));
//CHECK-NEXT: int funcdecl(_Ptr<char> ntiptr, _Ptr<int> iptr, int *wild : itype(_Ptr<int>)) {

// ptr is a ARR ptr
// iptr will be itype
// wild will be a wild ptr.
int func(int *ptr, int *iptr, int *wild) {
  if (ptr != 0) {
    ptr[0] = 1;
  }
  wild = (int *)0xdeadbeef;
  return 0;
}
//CHECK: int func(_Array_ptr<int> ptr, _Ptr<int> iptr, int *wild : itype(_Ptr<int>)) {

int main() {
  int a, b, c;
  int *ap = 0;
  int *bp = 0;
  int *cp = 0;
  char *ap1 = 0;
  int *bp1 = 0;
  int *cp1 = 0;

  //ap1 = &a;
  ap = &a;
  // we will make this pointer wild.
  bp1 = bp = (int *)0xcafeba;
  cp = &c;
  cp1 = &c;
  // we will make this wild in
  // main.
  ap1 = (char *)0xdeadbe;
  // although, we are passing cp
  // to a paramter that will be
  // treated as WILD in func
  func(ap, bp, cp);
  // ap1 will be WILD in main
  // bp1 will be WILD in main
  // cp1 will _Ptr
  funcdecl(ap1, bp1, cp1);
  return 0;
}
//CHECK: int main() {
//CHECK-NEXT: int a, b, c;
//CHECK-NEXT: int *ap =  0;
//CHECK-NEXT: int *bp = 0;
//CHECK-NEXT: _Ptr<int> cp = 0;
//CHECK-NEXT: char *ap1 = 0;
//CHECK-NEXT: int *bp1 = 0;
//CHECK-NEXT: _Ptr<int> cp1 = 0;

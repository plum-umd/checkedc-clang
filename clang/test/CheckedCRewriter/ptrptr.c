// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s
//

void f() {
  int x[5];
  int *pa = x;
  pa += 2;
  int *p = pa;
  p = (int *)5;
}
// CHECK: int x[5];

void g() {
  int *x = malloc(sizeof(int)*1);
  int y[5];
  int **p = &x;
  int **r = 0;
  *p = y;
  (*p)[0] = 1;
  r = p;
  **r = 1;
}
// CHECK:  _Array_ptr<int> x: count((sizeof(int) * 1)) =  malloc(sizeof(int)*1);
// CHECK:  int y _Checked[5];
// CHECK:  _Ptr<_Array_ptr<int>> p =  &x;
// CHECK:  _Ptr<_Array_ptr<int>> r =  0;

void foo(void) {
  int x;
  int *y = &x;
  int **z = &y;

  int *p = &x;
  int **q = &p;
  q = (int **)5;

  int *p2 = &x;
  p2 = (int *)5;
  int **q2 = &p2;
}
// CHECK:  _Ptr<int> y =  &x;
// CHECK:  _Ptr<_Ptr<int>> z =  &y;
// CHECK:  int **q2 = &p2;

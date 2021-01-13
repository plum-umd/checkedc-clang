// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

void foo(int *x) {
	//CHECK: void foo(int *x : itype(_Ptr<int>)) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  int **y = &x;
	//CHECK: _Ptr<int *> y = &x;
}

void bar(int *x) {
	//CHECK: void bar(int *x : itype(_Ptr<int>)) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  int *y = *(&x);
	//CHECK: int *y = *(&x);
}

int *id(int *x) {
	//CHECK: _Ptr<int> id(_Ptr<int> x) _Checked {
  return &(*x);
}

int f(int *x) {
	//CHECK: int f(_Ptr<int> x) _Checked {
  return *x;
}

void baz(void) {
  int (*fp)(int *) = f;
	//CHECK: _Ptr<int (_Ptr<int> )> fp = f;
  int (*fp2)(int *) = &f;
	//CHECK: _Ptr<int (_Ptr<int> )> fp2 = &f;
  f((void*)0);
}

extern int xfunc(int *arg);
int (*fp)(int *);
	//CHECK: _Ptr<int (int *)> fp = ((void *)0);

void addrof(void){
  fp = &xfunc;
}

void bif(int **x) {
	//CHECK: void bif(_Ptr<_Ptr<int>> x) _Checked {
  int **w = 0;
	//CHECK: _Ptr<_Ptr<int>> w = 0;
  int *y = *(x = w);
	//CHECK: _Ptr<int> y = *(x = w);
  w = &y;
}


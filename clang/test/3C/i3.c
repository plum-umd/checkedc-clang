// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

static int * f(int *x) {
	//CHECK: static int *f(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  return x;
}
/* force output */
int *p;
	//CHECK: _Ptr<int> p = ((void *)0);

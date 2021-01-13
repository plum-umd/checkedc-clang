// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

void foo() {
  int m = 2;
  int *s = &m;
	//CHECK: _Ptr<int> s = &m;
  int q[5] = { 0 };
  int *p = (int *)5;
	//CHECK: int *p = (int *)5;
  p = q + 3;
}

// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

static int *f(int *x) {
	//CHECK: static _Ptr<int> f(_Ptr<int> x) _Checked {
  return x;
}


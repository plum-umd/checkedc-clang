// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

/*nothing in this file, save the trivially converted w, should be converted*/
int *foo(int *w) {
	//CHECK: int *foo(_Ptr<int> w) : itype(_Ptr<int>) {
  int x = 1;
  int y = 2;
  int z = 3;
  int *ret;
	//CHECK: int *ret;
  for (int i = 0; i < 4; i++) {
	//CHECK: for (int i = 0; i < 4; i++) _Checked {
    switch(i) {
	//CHECK: switch(i) _Unchecked {
    case 0:
      ret = &x;
      break;
    case 1:
      ret = &y;
      break;
    case 2:
      ret = &z;
      break;
    case 3:
      ret = (int *)5;
	//CHECK: ret = (int *)5;
      break;
    }
  }
  return ret;
}

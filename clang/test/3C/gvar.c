// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

int *x;
	//CHECK: _Ptr<int> x = ((void *)0);
extern int *x;
void foo(void) {
	//CHECK: void foo(void) _Checked {
  *x = 1;
}

extern int *y;
int *y;
	//CHECK: int *y;
int *bar(void) {
	//CHECK: _Ptr<int> bar(void) {
  y = (int*)5;
	//CHECK: y = (int*)5;
  return x;
}


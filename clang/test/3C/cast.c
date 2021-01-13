// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

int foo(int* p) {
	//CHECK: int foo(_Ptr<int> p) {
  *p = 5;
  int x = (int)p; /* cast is safe */
  return x;
}

void bar(void) {
  int a = 0;
  int *b = &a;
	//CHECK: int *b = &a;
  char *c = (char *)b;
	//CHECK: char *c = (char *)b;
  int *d = (int *)5;
	//CHECK: int *d = (int *)5;
  /*int *e = (int *)(a+5);*/
}

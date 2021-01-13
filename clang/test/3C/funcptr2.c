// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

void f(int *(*fp)(int *)) {
	//CHECK: void f(_Ptr<int *(int * : itype(_Ptr<int>)) : itype(_Ptr<int>)> fp) {
  int *x = (int *)5;
	//CHECK: int *x = (int *)5;
  int *z = (int *)5;
	//CHECK: int *z = (int *)5;
  z = fp(x);
	//CHECK: z = fp(x);
}
int *g2(int *x) {
	//CHECK: int *g2(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  return x;
}
int *g(int *x) {
	//CHECK: int *g(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
  x = (int *)5;
	//CHECK: x = (int *)5;
  return 0;
}
void h() {
  f(g);
  f(g2);
}

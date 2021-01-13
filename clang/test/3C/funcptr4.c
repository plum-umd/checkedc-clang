// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

void f(int *(*fp)(int *)) {
	//CHECK: void f(_Ptr<_Ptr<int> (_Ptr<int> )> fp) _Checked {
  fp(0);
}
int *g2(int *x) {
	//CHECK: _Ptr<int> g2(_Ptr<int> x) _Checked {
  return x;
}
int *g(int *x) {
	//CHECK: _Ptr<int> g(_Ptr<int> x) _Checked {
  return 0;
}
void h() {
  int *(*fp)(int *) = g;
	//CHECK: _Ptr<_Ptr<int> (_Ptr<int> )> fp = g;
  f(g);
  f(g2);
  g(0);
}

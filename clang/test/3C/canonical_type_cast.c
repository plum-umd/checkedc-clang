// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'


/* Examples from issue #58 */

void f(int *p) {
	//CHECK: void f(_Ptr<int> p) {
  int *x = (int *)p;
	//CHECK: _Ptr<int> x = (_Ptr<int>)p;
}

void g(int p[]) {
	//CHECK_NOALL: void g(int p[]) {
	//CHECK_ALL: void g(_Ptr<int> p) {
  int *x = (int *)p;
	//CHECK_NOALL: int *x = (int *)p;
	//CHECK_ALL:   _Ptr<int> x = (_Ptr<int>)p;
}

/* A very similar issue with function pointers */

int add1(int a){
	//CHECK: int add1(int a)_Checked {
  return a + 1;
}

void h() {
	//CHECK: void h() _Checked {
  int (*x)(int) = add1;
	//CHECK: _Ptr<int (int )> x = add1;
}

void i() {
	//CHECK: void i() _Checked {
  int (*x)(int) = (int(*)(int))add1;
	//CHECK: _Ptr<int (int )> x = (_Ptr<int (int )>)add1;
}

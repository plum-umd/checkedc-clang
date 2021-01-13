// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'


int f(int *p);
	//CHECK: int f(int *p);
void bar() {
  int (*fp)(int *p) = f;
	//CHECK: _Ptr<int (int *)> fp = f;
  f((void*)0);
}

int mul_by_2(int x) { 
	//CHECK: int mul_by_2(int x) _Checked { 
    return x * 2;
}

int (*foo(void)) (int) {
	//CHECK: _Ptr<int (int )> foo(void) _Checked {
    return mul_by_2;
} 


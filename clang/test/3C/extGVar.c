// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

/*what we're interested in*/ 
extern int *x; 

/*safe filler to ensure that conversion happens*/ 
void g(int *y) { 
	//CHECK: void g(_Ptr<int> y) _Checked { 
	*y = 2;
}


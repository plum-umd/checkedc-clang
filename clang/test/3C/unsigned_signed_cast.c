// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

/*unsigned integer cast*/

unsigned int *foo = 0;
	//CHECK: unsigned int *foo = 0;
int *bar = 0;  
	//CHECK: int *bar = 0;  


/*unsigned characters cast*/

unsigned char *yoo = 0; 
	//CHECK: unsigned char *yoo = 0; 
char *yar = 0; 
	//CHECK: char *yar = 0; 


/*C does not support unsigned floats, so we don't have to worry about that case*/

/*ensure trivial conversion with parameter*/
void test(int *x) {
	//CHECK: void test(_Ptr<int> x) {
  foo = bar; 
  yoo = yar;
}

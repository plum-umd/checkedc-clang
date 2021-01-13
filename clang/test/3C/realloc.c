// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'


#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

void foo(int *w) { 
	//CHECK: void foo(_Ptr<int> w) { 
    int *y = malloc(2*sizeof(int)); 
	//CHECK_NOALL: int *y = malloc<int>(2*sizeof(int)); 
	//CHECK_ALL:     _Array_ptr<int> y : count(2) = malloc<int>(2*sizeof(int)); 
    y[1] = 3;
    int *z = realloc(y, 5*sizeof(int)); 
	//CHECK_NOALL: int *z = realloc<int>(y, 5*sizeof(int)); 
	//CHECK_ALL:     _Array_ptr<int> z = realloc<int>(y, 5*sizeof(int)); 
    z[3] =  2;
} 

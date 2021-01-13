// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

#include <stddef.h>
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);

#include <stddef.h>
_Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
	//CHECK: _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
	//CHECK: _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
_Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
	//CHECK: _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
	//CHECK: _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

int * foo(int *x) {
	//CHECK_NOALL: int *foo(int *x : itype(_Ptr<int>)) : itype(_Ptr<int>) {
	//CHECK_ALL: _Array_ptr<int> foo(_Array_ptr<int> x) _Checked {
  x[2] = 1;
  return x;
}
void bar(void) {
  int *y = malloc(sizeof(int)*2);
	//CHECK: int *y = malloc<int>(sizeof(int)*2);
  y = (int *)5;
	//CHECK: y = (int *)5;
  int *z = foo(y);
	//CHECK_NOALL: _Ptr<int> z = foo(y);
	//CHECK_ALL:   _Ptr<int> z = foo(_Assume_bounds_cast<_Array_ptr<int>>(y, byte_count(0)));
} 

void force(int *x){}
	//CHECK: void force(_Ptr<int> x)_Checked {}

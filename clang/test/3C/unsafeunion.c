// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

union foo {
  /*fields of a union should never be converted*/
  int * p;
	//CHECK: int * p;
  char * c;
	//CHECK: char * c;
};

void bar(int *x) {
	//CHECK: void bar(_Ptr<int> x) {
  /*but pointers to unions can be*/
  union foo *g = (void *) 0;
	//CHECK: _Ptr<union foo> g = (void *) 0;
  union foo *h = calloc(5, sizeof(union foo)); 
	//CHECK_NOALL: union foo *h = calloc<union foo>(5, sizeof(union foo)); 
	//CHECK_ALL:   _Array_ptr<union foo> h : count(5) = calloc<union foo>(5, sizeof(union foo)); 
  int y = 3;
  h[2].p = &y;

  char c;
  union foo f = { .c = &c };
  *f.p = 1;
} 

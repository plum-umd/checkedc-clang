// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/arrprotosafe.c -- | diff %t.checked/arrprotosafe.c -


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: arrays through a for loop and pointer
arithmetic to assign into it*/
/*For robustness, this test is identical to arrsafe.c except in that
a prototype for sus is available, and is called by foo and bar,
while the definition for sus appears below them*/
/*In this test, foo, bar, and sus will all treat their return values safely*/

/*********************************************************************************/


#include <stddef.h>
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

struct general { 
    int data; 
    struct general *next;
	//CHECK: _Ptr<struct general> next;
};

struct warr { 
    int data1[5];
	//CHECK_NOALL: int data1[5];
	//CHECK_ALL: int data1 _Checked[5];
    char *name;
	//CHECK: _Ptr<char> name;
};

struct fptrarr { 
    int *values; 
	//CHECK: _Ptr<int> values; 
    char *name;
	//CHECK: _Ptr<char> name;
    int (*mapper)(int);
	//CHECK: _Ptr<int (int)> mapper;
};

struct fptr { 
    int *value; 
	//CHECK: _Ptr<int> value; 
    int (*func)(int);
	//CHECK: _Ptr<int (int)> func;
};  

struct arrfptr { 
    int args[5]; 
	//CHECK_NOALL: int args[5]; 
	//CHECK_ALL: int args _Checked[5]; 
    int (*funcs[5]) (int);
	//CHECK_NOALL: int (*funcs[5]) (int);
	//CHECK_ALL: _Ptr<int (int)> funcs _Checked[5];
};

int add1(int x) { 
	//CHECK: int add1(int x) _Checked { 
    return x+1;
} 

int sub1(int x) { 
	//CHECK: int sub1(int x) _Checked { 
    return x-1; 
} 

int fact(int n) { 
	//CHECK: int fact(int n) _Checked { 
    if(n==0) { 
        return 1;
    } 
    return n*fact(n-1);
} 

int fib(int n) { 
	//CHECK: int fib(int n) _Checked { 
    if(n==0) { return 0; } 
    if(n==1) { return 1; } 
    return fib(n-1) + fib(n-2);
} 

int zerohuh(int n) { 
	//CHECK: int zerohuh(int n) _Checked { 
    return !n;
}

int *mul2(int *x) { 
	//CHECK: _Ptr<int> mul2(_Ptr<int> x) _Checked { 
    *x *= 2; 
    return x;
}

int * sus(int *, int *);
	//CHECK_NOALL: int *sus(int *x : itype(_Ptr<int>), _Ptr<int> y) : itype(_Ptr<int>);
	//CHECK_ALL: _Array_ptr<int> sus(int *x : itype(_Ptr<int>), _Ptr<int> y) : count(5);

int * foo() {
	//CHECK_NOALL: _Ptr<int> foo(void) {
	//CHECK_ALL: _Array_ptr<int> foo(void) : count(5) {
        int * x = malloc(sizeof(int));
	//CHECK: _Ptr<int> x = malloc<int>(sizeof(int));
        int * y = malloc(sizeof(int));
	//CHECK: _Ptr<int> y = malloc<int>(sizeof(int));
        int * z = sus(x, y);
	//CHECK_NOALL: _Ptr<int> z = sus(x, y);
	//CHECK_ALL: _Array_ptr<int> z : count(5) = sus(x, y);
return z; }

int * bar() {
	//CHECK_NOALL: _Ptr<int> bar(void) {
	//CHECK_ALL: _Array_ptr<int> bar(void) : count(5) {
        int * x = malloc(sizeof(int));
	//CHECK: _Ptr<int> x = malloc<int>(sizeof(int));
        int * y = malloc(sizeof(int));
	//CHECK: _Ptr<int> y = malloc<int>(sizeof(int));
        int * z = sus(x, y);
	//CHECK_NOALL: _Ptr<int> z = sus(x, y);
	//CHECK_ALL: _Array_ptr<int> z : count(5) = sus(x, y);
return z; }

int * sus(int * x, int * y) {
	//CHECK_NOALL: int *sus(int *x : itype(_Ptr<int>), _Ptr<int> y) : itype(_Ptr<int>) {
	//CHECK_ALL: _Array_ptr<int> sus(int *x : itype(_Ptr<int>), _Ptr<int> y) : count(5) {
x = (int *) 5;
	//CHECK: x = (int *) 5;
        int *z = calloc(5, sizeof(int)); 
	//CHECK_NOALL: int *z = calloc<int>(5, sizeof(int)); 
	//CHECK_ALL: _Array_ptr<int> z : count(5) = calloc<int>(5, sizeof(int)); 
        int i, fac;
        int *p;
	//CHECK_NOALL: int *p;
	//CHECK_ALL: _Array_ptr<int> p = ((void *)0);
        for(i = 0, p = z, fac = 1; i < 5; ++i, p++, fac *= i) 
        { *p = fac; }
	//CHECK_NOALL: { *p = fac; }
	//CHECK_ALL: _Checked { *p = fac; }
return z; }

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -

// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/fptrarrstructprotocallee.c -- | diff %t.checked/fptrarrstructprotocallee.c -


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: using a function pointer and an array as fields
of a struct that interact with each other*/
/*For robustness, this test is identical to fptrarrstructcallee.c except in that
a prototype for sus is available, and is called by foo and bar,
while the definition for sus appears below them*/
/*In this test, foo and bar will treat their return values safely, but sus will
not, through invalid pointer arithmetic, an unsafe cast, etc*/

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
	//CHECK_NOALL: int *values; 
	//CHECK_ALL: _Array_ptr<int> values : count(5); 
    char *name;
	//CHECK: char *name;
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

struct fptrarr * sus(struct fptrarr *, struct fptrarr *);
	//CHECK: struct fptrarr *sus(struct fptrarr *x : itype(_Ptr<struct fptrarr>), _Ptr<struct fptrarr> y) : itype(_Ptr<struct fptrarr>);

struct fptrarr * foo() {
	//CHECK: _Ptr<struct fptrarr> foo(void) {
 
        char name[20]; 
        struct fptrarr * x = malloc(sizeof(struct fptrarr));
	//CHECK: _Ptr<struct fptrarr> x = malloc<struct fptrarr>(sizeof(struct fptrarr));
        struct fptrarr *y =  malloc(sizeof(struct fptrarr));
	//CHECK: _Ptr<struct fptrarr> y =  malloc<struct fptrarr>(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int)); 
	//CHECK_NOALL: int *yvals = calloc<int>(5, sizeof(int)); 
	//CHECK_ALL: _Array_ptr<int> yvals : count(5) = calloc<int>(5, sizeof(int)); 
        int i;
        for(i = 0; i < 5; i++) {
	//CHECK_NOALL: for(i = 0; i < 5; i++) {
	//CHECK_ALL: for(i = 0; i < 5; i++) _Checked {
            yvals[i] = i+1; 
            }  
        y->values = yvals; 
        y->name = name; 
        y->mapper = NULL;
        strcpy(y->name, "Example"); 
        struct fptrarr *z = sus(x, y);
	//CHECK: _Ptr<struct fptrarr> z = sus(x, y);
        
return z; }

struct fptrarr * bar() {
	//CHECK: _Ptr<struct fptrarr> bar(void) {
 
        char name[20]; 
        struct fptrarr * x = malloc(sizeof(struct fptrarr));
	//CHECK: _Ptr<struct fptrarr> x = malloc<struct fptrarr>(sizeof(struct fptrarr));
        struct fptrarr *y =  malloc(sizeof(struct fptrarr));
	//CHECK: _Ptr<struct fptrarr> y =  malloc<struct fptrarr>(sizeof(struct fptrarr));
        int *yvals = calloc(5, sizeof(int)); 
	//CHECK_NOALL: int *yvals = calloc<int>(5, sizeof(int)); 
	//CHECK_ALL: _Array_ptr<int> yvals : count(5) = calloc<int>(5, sizeof(int)); 
        int i;
        for(i = 0; i < 5; i++) {
	//CHECK_NOALL: for(i = 0; i < 5; i++) {
	//CHECK_ALL: for(i = 0; i < 5; i++) _Checked {
            yvals[i] = i+1; 
            }  
        y->values = yvals; 
        y->name = name; 
        y->mapper = NULL;
        strcpy(y->name, "Example"); 
        struct fptrarr *z = sus(x, y);
	//CHECK: _Ptr<struct fptrarr> z = sus(x, y);
        
return z; }

struct fptrarr * sus(struct fptrarr *x, struct fptrarr *y) {
	//CHECK: struct fptrarr *sus(struct fptrarr *x : itype(_Ptr<struct fptrarr>), _Ptr<struct fptrarr> y) : itype(_Ptr<struct fptrarr>) {
 
        x = (struct fptrarr *) 5; 
	//CHECK: x = (struct fptrarr *) 5; 
        char name[30]; 
        struct fptrarr *z = malloc(sizeof(struct fptrarr)); 
	//CHECK: struct fptrarr *z = malloc<struct fptrarr>(sizeof(struct fptrarr)); 
        z->values = y->values; 
        z->name = strcpy(name, "Hello World");
        z->mapper = fact; 
        int i;
        for(i = 0; i < 5; i++) { 
            z->values[i] = z->mapper(z->values[i]);
        }
        
z += 2;
return z; }

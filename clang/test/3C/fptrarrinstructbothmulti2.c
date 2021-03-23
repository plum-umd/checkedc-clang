// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -output-dir=%t.checkedALL2 %S/fptrarrinstructbothmulti1.c %s --
// RUN: 3c -base-dir=%S -addcr -output-dir=%t.checkedNOALL2 %S/fptrarrinstructbothmulti1.c %s --
// RUN: %clang -working-directory=%t.checkedNOALL2 -c fptrarrinstructbothmulti1.c fptrarrinstructbothmulti2.c
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" --input-file %t.checkedNOALL2/fptrarrinstructbothmulti2.c %s
// RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" --input-file %t.checkedALL2/fptrarrinstructbothmulti2.c %s
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %S/fptrarrinstructbothmulti1.c %s --
// RUN: 3c -base-dir=%t.checked -alltypes -output-dir=%t.convert_again %t.checked/fptrarrinstructbothmulti1.c %t.checked/fptrarrinstructbothmulti2.c --
// RUN: test ! -f %t.convert_again/fptrarrinstructbothmulti1.c
// RUN: test ! -f %t.convert_again/fptrarrinstructbothmulti2.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when there is an array
of function pointers in a struct*/
/*For robustness, this test is identical to fptrarrinstructprotoboth.c and fptrarrinstructboth.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo will treat its return value safely, but sus and bar will not,
through invalid pointer arithmetic, an unsafe cast, etc.*/

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

static int add1(int x) { 
	//CHECK: static int add1(int x) _Checked { 
    return x+1;
} 

static int sub1(int x) { 
	//CHECK: static int sub1(int x) _Checked { 
    return x-1; 
} 

static int fact(int n) { 
	//CHECK: static int fact(int n) _Checked { 
    if(n==0) { 
        return 1;
    } 
    return n*fact(n-1);
} 

static int fib(int n) { 
	//CHECK: static int fib(int n) _Checked { 
    if(n==0) { return 0; } 
    if(n==1) { return 1; } 
    return fib(n-1) + fib(n-2);
} 

static int zerohuh(int n) { 
	//CHECK: static int zerohuh(int n) _Checked { 
    return !n;
}

static int *mul2(int *x) { 
	//CHECK: static _Ptr<int> mul2(_Ptr<int> x) _Checked { 
    *x *= 2; 
    return x;
}

struct arrfptr * sus(struct arrfptr *x, struct arrfptr *y) {
	//CHECK_NOALL: struct arrfptr *sus(struct arrfptr *x : itype(_Ptr<struct arrfptr>), _Ptr<struct arrfptr> y) : itype(_Ptr<struct arrfptr>) {
	//CHECK_ALL: struct arrfptr *sus(struct arrfptr *x : itype(_Ptr<struct arrfptr>), _Ptr<struct arrfptr> y) : itype(_Array_ptr<struct arrfptr>) {
 
        x = (struct arrfptr *) 5; 
	//CHECK: x = (struct arrfptr *) 5; 
        struct arrfptr *z = malloc(sizeof(struct arrfptr)); 
	//CHECK: struct arrfptr *z = malloc<struct arrfptr>(sizeof(struct arrfptr)); 
        int i;
        for(i = 0; i < 5; i++) { 
            z->args[i] = i + 1; 
        } 
        z->funcs[0] = add1;
        z->funcs[1] = sub1; 
        z->funcs[2] = zerohuh;
        z->funcs[3] = fib;
        z->funcs[4] = fact;
        
z += 2;
return z; }

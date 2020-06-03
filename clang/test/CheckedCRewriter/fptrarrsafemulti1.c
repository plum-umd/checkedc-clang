// RUN: cconv-standalone -base-dir=%S -output-postfix=checked %s %S/fptrarrsafemulti2.c
//RUN: FileCheck -match-full-lines --input-file %S/fptrarrsafemulti1.checked.c %s
//RUN: rm %S/fptrarrsafemulti1.checked.c %S/fptrarrsafemulti2.checked.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: using a function pointer and an array in
tandem to do computations*//*For robustness, this test is identical to fptrarrprotosafe.c and fptrarrsafe.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo, bar, and sus will all treat their return values safely*/

/*********************************************************************************/


#define size_t int
#define NULL 0
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);
extern _Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern int printf(const char * restrict format : itype(restrict _Nt_array_ptr<const char>), ...);
extern _Unchecked char *strcpy(char * restrict dest, const char * restrict src : itype(restrict _Nt_array_ptr<const char>));

struct general { 
    int data; 
    struct general *next;
};
//CHECK:     _Ptr<struct general> next;


struct warr { 
    int data1[5];
    char name[];
};
//CHECK:     _Ptr<int> data1;
//CHECK-NEXT:     _Ptr<char> name;


struct fptrarr { 
    int *values; 
    char *name;
    int (*mapper)(int);
};
//CHECK:     _Ptr<int> values; 
//CHECK-NEXT:     _Ptr<char> name;
//CHECK-NEXT:     _Ptr<int (int )> mapper;


struct fptr { 
    int *value; 
    int (*func)(int*);
};  
//CHECK:     _Ptr<int> value; 
//CHECK-NEXT:     _Ptr<int (_Ptr<int> )> func;


struct arrfptr { 
    int args[5]; 
    int (*funcs[5]) (int);
};
//CHECK:     _Ptr<int> args; 
//CHECK-NEXT:     _Ptr<_Ptr<int (int )>> funcs;


int add1(int x) { 
    return x+1;
} 

int sub1(int x) { 
    return x-1; 
} 

int fact(int n) { 
    if(n==0) { 
        return 1;
    } 
    return n*fact(n-1);
} 

int fib(int n) { 
    if(n==0) { return 0; } 
    if(n==1) { return 1; } 
    return fib(n-1) + fib(n-2);
} 

int zerohuh(int n) { 
    return !n;
}

int *mul2(int *x) { 
    *x *= 2; 
    return x;
}

//CHECK: int * mul2(int *x) { 

int ** sus(int *, int *);
//CHECK: int ** sus(int *, int *);

int ** foo() {

        int *x = malloc(sizeof(int)); 
        int *y = calloc(5, sizeof(int)); 
        for(int i = 0; i < 5; i++) { 
            y[i] = i+1;
        } 
        int *z = sus(x, y);
        
return z; }
//CHECK: int ** foo() {
//CHECK:         int *x = malloc(sizeof(int)); 
//CHECK:         int *y = calloc(5, sizeof(int)); 
//CHECK:         int *z = sus(x, y);

int ** bar() {

        int *x = malloc(sizeof(int)); 
        int *y = calloc(5, sizeof(int)); 
        for(int i = 0; i < 5; i++) { 
            y[i] = i+1;
        } 
        int *z = sus(x, y);
        
return z; }
//CHECK: int ** bar() {
//CHECK:         int *x = malloc(sizeof(int)); 
//CHECK:         int *y = calloc(5, sizeof(int)); 
//CHECK:         int *z = sus(x, y);

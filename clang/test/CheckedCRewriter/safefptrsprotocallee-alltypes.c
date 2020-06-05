// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines %s


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: passing function pointers in as arguments and
returning a function pointer safely*/
/*For robustness, this test is identical to safefptrscallee-alltypes.c except in that
a prototype for sus is available, and is called by foo and bar,
while the definition for sus appears below them*/
/*In this test, foo and bar will treat their return values safely, but sus will
not, through invalid pointer arithmetic, an unsafe cast, etc*/
/*This file was converted with the alltypes flag, which adds support for arrays
and other data structures*/

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
    char *name;
};
//CHECK:     int data1 _Checked[5];
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
    int (*func)(int);
};  
//CHECK:     _Ptr<int> value; 
//CHECK-NEXT:     _Ptr<int (int )> func;


struct arrfptr { 
    int args[5]; 
    int (*funcs[5]) (int);
};
//CHECK:     int args _Checked[5]; 
//CHECK-NEXT:     _Ptr<int (int )> funcs _Checked[5];


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

//CHECK: _Ptr<int> mul2(_Ptr<int> x) { 

int * (*sus(int (*) (int), int (*) (int))) (int *);
//CHECK: _Nt_array_ptr<_Ptr<int> (_Ptr<int> )> sus(int (*x)(int), _Ptr<int (int )> y);

int * (*foo(void)) (int *) {

        int (*x)(int) = add1; 
        int (*y)(int) = sub1; 
        int *(*z)(int *) = sus(x, y);
        
return z; }
//CHECK: _Nt_array_ptr<_Ptr<int> (_Ptr<int> )> foo(void) {
//CHECK:         int (*x)(int) = add1; 

int * (*bar(void)) (int *) {

        int (*x)(int) = add1; 
        int (*y)(int) = sub1; 
        int *(*z)(int *) = sus(x, y);
        
return z; }
//CHECK: _Nt_array_ptr<_Ptr<int> (_Ptr<int> )> bar(void) {
//CHECK:         int (*x)(int) = add1; 

int * (*sus(int (*x) (int), int (*y) (int))) (int *) {
 
        x = (int (*) (int)) 5; 
        int * (*z)(int *) = mul2;
        
z += 2;
return z; }
//CHECK: _Nt_array_ptr<_Ptr<int> (_Ptr<int> )> sus(int (*x)(int), _Ptr<int (int )> y) {

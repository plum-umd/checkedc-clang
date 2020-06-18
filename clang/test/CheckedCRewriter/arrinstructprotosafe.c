// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
//RUN: cconv-standalone -output-postfix=checked %s
//RUN: %clang -Wno-everything -c %S/arrinstructprotosafe.checked.c
//RUN: rm %S/arrinstructprotosafe.checked.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: how the tool behaves when there is an array
field within a struct*/
/*For robustness, this test is identical to arrinstructsafe.c except in that
a prototype for sus is available, and is called by foo and bar,
while the definition for sus appears below them*/
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
    char *name;
};
//CHECK:     int data1[5];
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
//CHECK:     int args[5]; 
//CHECK-NEXT:     int (*funcs[5]) (int);


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

struct warr * sus(struct warr *, struct warr *);
//CHECK: _Ptr<struct warr> sus(struct warr *x, _Ptr<struct warr> y);

struct warr * foo() {
        struct warr * x = malloc(sizeof(struct warr));
        struct warr * y = malloc(sizeof(struct warr));
        struct warr * z = sus(x, y);
return z; }
//CHECK: _Ptr<struct warr> foo(void) {
//CHECK:         struct warr * x = malloc(sizeof(struct warr));

struct warr * bar() {
        struct warr * x = malloc(sizeof(struct warr));
        struct warr * y = malloc(sizeof(struct warr));
        struct warr * z = sus(x, y);
return z; }
//CHECK: _Ptr<struct warr> bar(void) {
//CHECK:         struct warr * x = malloc(sizeof(struct warr));

struct warr * sus(struct warr * x, struct warr * y) {
x = (struct warr *) 5;
        char name[20]; 
        struct warr *z = y;
        for(int i = 0; i < 5; i++) { 
            z->data1[i] = i; 
        }
        
return z; }
//CHECK: _Ptr<struct warr> sus(struct warr *x, _Ptr<struct warr> y) {

// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checkedALL %s %S/ptrTOptrsafemulti2.c
// RUN: cconv-standalone -base-dir=%S -output-postfix=checkedNOALL %s %S/ptrTOptrsafemulti2.c
//RUN: %clang -c %S/ptrTOptrsafemulti1.checkedNOALL.c %S/ptrTOptrsafemulti2.checkedNOALL.c
//RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" --input-file %S/ptrTOptrsafemulti1.checkedNOALL.c %s
//RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL" --input-file %S/ptrTOptrsafemulti1.checkedALL.c %s
//RUN: rm %S/ptrTOptrsafemulti1.checkedALL.c %S/ptrTOptrsafemulti2.checkedALL.c
//RUN: rm %S/ptrTOptrsafemulti1.checkedNOALL.c %S/ptrTOptrsafemulti2.checkedNOALL.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: having a pointer to a pointer*/
/*For robustness, this test is identical to ptrTOptrprotosafe.c and ptrTOptrsafe.c except in that
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
//CHECK_NOALL:     _Ptr<struct general> next;

//CHECK_ALL:     _Ptr<struct general> next;


struct warr { 
    int data1[5];
    char *name;
};
//CHECK_NOALL:     int data1[5];
//CHECK_NOALL:     _Ptr<char> name;

//CHECK_ALL:     int data1 _Checked[5];
//CHECK_ALL:     _Ptr<char> name;


struct fptrarr { 
    int *values; 
    char *name;
    int (*mapper)(int);
};
//CHECK_NOALL:     _Ptr<int> values; 
//CHECK_NOALL:     _Ptr<char> name;
//CHECK_NOALL:     _Ptr<int (int )> mapper;

//CHECK_ALL:     _Ptr<int> values; 
//CHECK_ALL:     _Ptr<char> name;
//CHECK_ALL:     _Ptr<int (int )> mapper;


struct fptr { 
    int *value; 
    int (*func)(int);
};  
//CHECK_NOALL:     _Ptr<int> value; 
//CHECK_NOALL:     _Ptr<int (int )> func;

//CHECK_ALL:     _Ptr<int> value; 
//CHECK_ALL:     _Ptr<int (int )> func;


struct arrfptr { 
    int args[5]; 
    int (*funcs[5]) (int);
};
//CHECK_NOALL:     int args[5]; 
//CHECK_NOALL:     int (*funcs[5]) (int);

//CHECK_ALL:     int args _Checked[5]; 
//CHECK_ALL:     _Ptr<int (int )> funcs _Checked[5];


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

//CHECK_NOALL: _Ptr<int> mul2(_Ptr<int> x) { 

//CHECK_ALL: _Ptr<int> mul2(_Ptr<int> x) { 

char *** sus(char * * *, char * * *);
//CHECK_NOALL: char *** sus(char ***, _Ptr<_Ptr<_Ptr<char>>> y);
//CHECK_ALL: _Ptr<_Array_ptr<char*>> sus(char ***, _Ptr<_Ptr<_Ptr<char>>> y);

char *** foo() {
        char * * * x = malloc(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
        char *** z = sus(x, y);
return z; }
//CHECK_NOALL: char *** foo() {
//CHECK_NOALL:         char * * * x = malloc(sizeof(char * *));
//CHECK_NOALL:         _Ptr<_Ptr<_Ptr<char>>> y =  malloc(sizeof(char * *));
//CHECK_NOALL:         char *** z = sus(x, y);
//CHECK_ALL: _Ptr<_Array_ptr<char*>> foo(void) {
//CHECK_ALL:         char * * * x = malloc(sizeof(char * *));
//CHECK_ALL:         _Ptr<_Ptr<_Ptr<char>>> y =  malloc(sizeof(char * *));
//CHECK_ALL:         _Ptr<_Array_ptr<char*>> z =  sus(x, y);

char *** bar() {
        char * * * x = malloc(sizeof(char * *));
        char * * * y = malloc(sizeof(char * *));
        char *** z = sus(x, y);
return z; }
//CHECK_NOALL: char *** bar() {
//CHECK_NOALL:         char * * * x = malloc(sizeof(char * *));
//CHECK_NOALL:         _Ptr<_Ptr<_Ptr<char>>> y =  malloc(sizeof(char * *));
//CHECK_NOALL:         char *** z = sus(x, y);
//CHECK_ALL: _Ptr<_Array_ptr<char*>> bar(void) {
//CHECK_ALL:         char * * * x = malloc(sizeof(char * *));
//CHECK_ALL:         _Ptr<_Ptr<_Ptr<char>>> y =  malloc(sizeof(char * *));
//CHECK_ALL:         _Ptr<_Array_ptr<char*>> z =  sus(x, y);

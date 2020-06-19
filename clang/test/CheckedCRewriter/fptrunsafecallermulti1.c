// RUN: cconv-standalone -base-dir=%S -alltypes -output-postfix=checkedALL %s %S/fptrunsafecallermulti2.c
// RUN: cconv-standalone -base-dir=%S -output-postfix=checkedNOALL %s %S/fptrunsafecallermulti2.c
//RUN: %clang -c %S/fptrunsafecallermulti1.checkedNOALL.c %S/fptrunsafecallermulti2.checkedNOALL.c
//RUN: FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" --input-file %S/fptrunsafecallermulti1.checkedNOALL.c %s
//RUN: FileCheck -match-full-lines -check-prefixes="CHECK_ALL" --input-file %S/fptrunsafecallermulti1.checkedALL.c %s
//RUN: rm %S/fptrunsafecallermulti1.checkedALL.c %S/fptrunsafecallermulti2.checkedALL.c
//RUN: rm %S/fptrunsafecallermulti1.checkedNOALL.c %S/fptrunsafecallermulti2.checkedNOALL.c


/*********************************************************************************/

/*This file tests three functions: two callers bar and foo, and a callee sus*/
/*In particular, this file tests: converting the callee into a function pointer
unsafely via cast and using that pointer for computations*/
/*For robustness, this test is identical to fptrunsafeprotocaller.c and fptrunsafecaller.c except in that
the callee and callers are split amongst two files to see how
the tool performs conversions*/
/*In this test, foo and sus will treat their return values safely, but bar will
not, through invalid pointer arithmetic, an unsafe cast, etc.*/

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
//CHECK_NOALL:     struct general *next;

//CHECK_ALL:     struct general *next;


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

int * sus(struct general *, struct general *);
//CHECK_NOALL: int * sus(struct general *, struct general *);
//CHECK_ALL: int * sus(struct general *, struct general *);

int * foo() {

        struct general *x = malloc(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        for(int i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int (*sus_ptr)(struct fptr *, struct fptr *) = sus;   
        int *z = (int *) sus_ptr(x, y);
        
return z; }
//CHECK_NOALL: int * foo() {
//CHECK_NOALL:         struct general *x = malloc(sizeof(struct general)); 
//CHECK_NOALL:         struct general *y = malloc(sizeof(struct general));
//CHECK_NOALL:         struct general *curr = y;
//CHECK_NOALL:         for(int i = 1; i < 5; i++, curr = curr->next) { 
//CHECK_NOALL:             curr->next = malloc(sizeof(struct general));
//CHECK_NOALL:         int (*sus_ptr)(struct fptr *, struct fptr *) = sus;   
//CHECK_NOALL:         int *z = (int *) sus_ptr(x, y);
//CHECK_ALL: int * foo() {
//CHECK_ALL:         struct general *x = malloc(sizeof(struct general)); 
//CHECK_ALL:         struct general *y = malloc(sizeof(struct general));
//CHECK_ALL:         struct general *curr = y;
//CHECK_ALL:         for(int i = 1; i < 5; i++, curr = curr->next) { 
//CHECK_ALL:             curr->next = malloc(sizeof(struct general));
//CHECK_ALL:         int (*sus_ptr)(struct fptr *, struct fptr *) = sus;   
//CHECK_ALL:         int *z = (int *) sus_ptr(x, y);

int * bar() {

        struct general *x = malloc(sizeof(struct general)); 
        struct general *y = malloc(sizeof(struct general));
        struct general *curr = y;
        for(int i = 1; i < 5; i++, curr = curr->next) { 
            curr->data = i;
            curr->next = malloc(sizeof(struct general));
            curr->next->data = i+1;
        }
        int (*sus_ptr)(struct fptr *, struct fptr *) = sus;   
        int *z = (int *) sus_ptr(x, y);
        
z += 2;
return z; }
//CHECK_NOALL: int * bar() {
//CHECK_NOALL:         struct general *x = malloc(sizeof(struct general)); 
//CHECK_NOALL:         struct general *y = malloc(sizeof(struct general));
//CHECK_NOALL:         struct general *curr = y;
//CHECK_NOALL:         for(int i = 1; i < 5; i++, curr = curr->next) { 
//CHECK_NOALL:             curr->next = malloc(sizeof(struct general));
//CHECK_NOALL:         int (*sus_ptr)(struct fptr *, struct fptr *) = sus;   
//CHECK_NOALL:         int *z = (int *) sus_ptr(x, y);
//CHECK_ALL: int * bar() {
//CHECK_ALL:         struct general *x = malloc(sizeof(struct general)); 
//CHECK_ALL:         struct general *y = malloc(sizeof(struct general));
//CHECK_ALL:         struct general *curr = y;
//CHECK_ALL:         for(int i = 1; i < 5; i++, curr = curr->next) { 
//CHECK_ALL:             curr->next = malloc(sizeof(struct general));
//CHECK_ALL:         int (*sus_ptr)(struct fptr *, struct fptr *) = sus;   
//CHECK_ALL:         int *z = (int *) sus_ptr(x, y);

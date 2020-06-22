// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/realloc.checkedNOALL.c
// RUN: rm %S/realloc.checkedNOALL.c


#define size_t int
extern _Itype_for_any(T) void *malloc(size_t size) : itype(_Array_ptr<T>) byte_count(size);
extern _Itype_for_any(T) void *realloc(void *pointer : itype(_Array_ptr<T>) byte_count(1), size_t size) : itype(_Array_ptr<T>) byte_count(size);

void foo(int *w) { 
    int *y = malloc(sizeof(int)); 
    int *z = realloc(y, 2*sizeof(int)); 
    z[1] =  2;
}
//CHECK: int *y = malloc(sizeof(int));
//CHECK_ALL: _Array_ptr<int> z = realloc(y, 2*sizeof(int)); 
//CHECK_NOALL: int *z = realloc(y, 2*sizeof(int));

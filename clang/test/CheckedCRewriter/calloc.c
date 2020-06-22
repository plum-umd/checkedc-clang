// RUN: cconv-standalone -alltypes %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL" %s
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL" %s
// RUN: cconv-standalone -output-postfix=checkedNOALL %s
// RUN: %clang -c %S/calloc.checkedNOALL.c
// RUN: rm %S/calloc.checkedNOALL.c


#define size_t int
extern _Itype_for_any(T) void *calloc(size_t nmemb, size_t size) : itype(_Array_ptr<T>) byte_count(nmemb * size);

void func(int *x : itype(_Nt_array_ptr<int>));

void foo(int *w) { 
    int *x = calloc(5, sizeof(int)); 
    x[2] = 0;
    func(x);
}
//CHECK_ALL: _Nt_array_ptr<int> x = calloc(5, sizeof(int)); 
//CHECK_NOALL: int *x = calloc(5, sizeof(int));

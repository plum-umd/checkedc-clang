// Tests for Checked C rewriter tool.
//
// Checks cast insertion while passing arguments to itype parameters.
//
// RUN: cconv-standalone %s -- | FileCheck -match-full-lines %s
// RUN: cconv-standalone %s -- | %clang_cc1 -fignore-checkedc-pointers -verify -fcheckedc-extension -x c -
// expected-no-diagnostics

int foo(int **p:itype(_Ptr<_Ptr<int>>));
int bar(int **p:itype(_Ptr<int *>));
int baz(int ((*compar)(const int *, const int *)) :
             itype(_Ptr<int (_Ptr<const int>, _Ptr<const int>)>));
             
int func(void) {
    int ((*fptr1)(const int *, const int *));
    int ((*fptr2)(const int *, const int *));
    
    int **fp1;
    int **fp2;
    int **bp1;
    int **bp2;
    
    *fp2 = 2;
    foo(fp1);
    foo(fp2);
    bar(bp1);
    bp2 = 2;
    *bp2 = 2;
    bar(bp2);
    fptr1(2, 0);
    baz(fptr1);
    baz(fptr2);
    return 0;    
}
//CHECK: _Ptr<int (const int *, _Ptr<const int> )> fptr1 = ((void *)0);
//CHECK-NEXT: _Ptr<int (_Ptr<const int> , _Ptr<const int> )> fptr2 = ((void *)0);
//CHECK: _Ptr<_Ptr<int>> fp1 = ((void *)0);
//CHECK-NEXT: _Ptr<int*> fp2 = ((void *)0);
//CHECK-NEXT: _Ptr<int*> bp1 = ((void *)0);
//CHECK-NEXT: int **bp2;
//CHECK: foo(fp1);
//CHECK-NEXT: foo(((int **)fp2));
//CHECK-NEXT: bar(bp1);
//CHECK: bar(bp2);
//CHECK-NEXT: fptr1(2, 0);
//CHECK-NEXT: baz(((int ((*)(const int *, const int *)) )fptr1));
//CHECK-NEXT: baz(fptr2);

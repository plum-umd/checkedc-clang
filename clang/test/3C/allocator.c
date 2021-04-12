// Tests for 3C.
//
// Tests for malloc and friends.
//
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S %s -- | FileCheck -match-full-lines %s
// RUN: 3c -base-dir=%S %s -- | %clang_cc1  -fno-builtin -verify -fcheckedc-extension -x c -
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked %t.checked/allocator.c -- | diff %t.checked/allocator.c -
// expected-no-diagnostics
//
#include <stddef.h>
_Itype_for_any(T) void *malloc(size_t size)
    : itype(_Array_ptr<T>) byte_count(size);
_Itype_for_any(T) void free(void *pointer : itype(_Array_ptr<T>) byte_count(0));

void dosomething(void) {
  int a = 0;
  int *b = &a;
  *b = 1;
  return;
}
//CHECK: _Ptr<int> b =  &a;

void foo(void) {
  int *a = (int *)malloc(sizeof(int));
  *a = 0;
  free(a);
  return;
}
//CHECK: void foo(void) {
//CHECK-NEXT: _Ptr<int> a = (_Ptr<int>)malloc<int>(sizeof(int));

typedef struct _listelt {
  struct _listelt *next;
  int val;
} listelt;

typedef struct _listhead {
  listelt *hd;
} listhead;

void add_some_stuff(listhead *hd) {
  listelt *l1 = (listelt *)malloc(sizeof(listelt));
  l1->next = 0;
  l1->val = 0;
  listelt *cur = hd->hd;
  while (cur) {
    cur = cur->next;
  }
  cur->next = l1;
  return;
}
//CHECK: void add_some_stuff(_Ptr<listhead>  hd) {
//CHECK-NEXT: _Ptr<listelt> l1 = (_Ptr<listelt>)malloc<listelt>(sizeof(listelt));

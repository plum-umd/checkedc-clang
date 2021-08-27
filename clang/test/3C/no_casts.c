// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/no_casts.c -- | diff %t.checked/no_casts.c -

void foo(char *a);
//CHECK: void foo(char *a : itype(_Ptr<char>));
void bar(int *a);
//CHECK: void bar(int *a : itype(_Ptr<int>));
void baz(int a[1]);
//CHECK_NOALL: void baz(int a[1]);
//CHECK_ALL: void baz(int *a : itype(int _Checked[1]));
// TODO: Checked C's clang seems to treat `int *a : itype(int _Checked[1])`
//       the same as `int a[1] : itype(int _Checked[1])`, so the current
//       rewriting is acceptable. It still would be better to keep the original
//       type as `int[1]`.

int *wild();
//CHECK: int *wild(void) : itype(_Ptr<int>);

void test() {
  foo("test");
  //CHECK: foo("test");

  int x;
  bar(&x);
  //CHECK: bar(&x);

  baz((int[1]){1});
  //CHECK_NOALL: baz((int[1]){1});
  //CHECK_ALL: baz((int _Checked[1]){1});

  bar(wild());
  //CHECK: bar(wild());
}

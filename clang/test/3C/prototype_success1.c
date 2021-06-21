//RUN: rm -rf %t*
//RUN: 3c -base-dir=%S -output-dir=%t.checked %s %S/prototype_success2.c --
//RUN: FileCheck -match-full-lines --input-file %t.checked/prototype_success1.c %s
//RUN: %clang -working-directory=%t.checked -c prototype_success1.c prototype_success2.c

/*Note: this file is part of a multi-file regression test in tandem with
  prototype_success2.c*/

/*prototypes that type-check with each other are fine*/
int *foo(_Ptr<int>, char);

/*a prototype definition combo that type-checks is also fine*/
int *bar(int *x, float *y) { return x; }

/*a C-style prototype combined with an enumerated prototype is also fine*/
int *baz(int);

/*another consideration is that if two things are different types but have the
  same general pointer structure, we're OK with it!*/
int *yoo(int *x, char y, float **z);

void trivial_conversion(int *x) {
  //CHECK: void trivial_conversion(_Ptr<int> x) {
}

// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_NOALL","CHECK" %s
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/dont_add_prototype.c -- | diff %t.checked/dont_add_prototype.c -

// Don't a void prototype on functions if we don't need to. This can potentialy
// cause rewriting error when attempting to rewrite functions defiened outside
// the basedir or defined by a macro.

// This function does not need to be rewritten. It's return type is unchecked
// after solving. It should not be rewritten to use a void prototype in order
// to avoid any potential rewritting issues.
void *test0() { return 1; }
//CHECK: void *test0() { return 1; }

// Trying to add a prototype int these examples caused a rewriting error
// because the functions are defined in a macros.

#define test_macro0 int *test_macro0()
test_macro0 {
//CHECK: test_macro0 {
  return 0;
}

#define test_macro1 test_macro1()
int *test_macro1 {
//CHECK: int *test_macro1 {
  return 0;
}

// Force conversion output.
int *a;

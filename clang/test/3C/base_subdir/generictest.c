// RUN: cd %S 

// RUN: 3c -output-dir=%t.checked/base_subdir %s -- -Xclang -verify

#include "../hasgeneric.c"


int bar(void) { 
  return 3;
}

// RUN: true

#include <stdlib.h>

void foo(void) { 
  int *b;
  b = malloc(sizeof(int));
}

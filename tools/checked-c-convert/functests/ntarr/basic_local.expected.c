#include <string_checked.h>
// basic test
// just create a NT pointer
int main() {
  // a has to identified
  // as NTArr
  // we use this as an argument
  // to string function.
  _Nt_array_ptrchar> a;
  // c should be identified as 
  // ARR as we assign it the return value of
  // string function use it.
  _Nt_array_ptrchar> c;
  // we will make this wild.
  int *d;
  int b;
  // this will make a as NTARR
  b = strlen(a);
  // this will make C as NTArr
  c = strstr("Hello", "World");
  // this should mark d as WILD. 
  d = 0xdeadbeef;
}

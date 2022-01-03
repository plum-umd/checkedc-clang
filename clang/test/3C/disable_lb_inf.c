// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -addcr -alltypes -disable-lb-inf %s -- | FileCheck -match-full-lines -check-prefixes="CHECK_ALL","CHECK" %s
// RUN: 3c -base-dir=%S -addcr -alltypes -disable-lb-inf %s -- | %clang -c -Wno-unused-value -fcheckedc-extension -f3c-tool -x c -o /dev/null -
// RUN: 3c -base-dir=%S -alltypes -disable-lb-inf -output-dir=%t.checked %s --
// RUN: 3c -base-dir=%t.checked -alltypes -disable-lb-inf %t.checked/disable_lb_inf.c -- | diff %t.checked/disable_lb_inf.c -

// 3C can generate a lower bound for `a`, but -disable-lb-inf means this does
// not happen. Since there is now lower bound, there is also no upper bound
// inferred.
void test(int *a, int n) {
//CHECK_ALL: void test(_Array_ptr<int> a, int n) {
  a++;
  for (int i = 0; i < n; i++)
    (void) a[i];

  // Prior to lower bound inference, 3C would have inferred a lower bound for
  // `b` based on the loop. With lower bound inference enabled, 3C infers this
  // bound, and also infers that `a` is the lower bound of `b`.  After
  // disabling lower bound inference, no bound is inferred.
  int *b = a;
  //CHECK_ALL: _Array_ptr<int> b = a;
  for (int i = 0; i < n; i++)
    (void) b[i];
}

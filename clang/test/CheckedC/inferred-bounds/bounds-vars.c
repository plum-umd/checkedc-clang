// Tests for variables occurring in bounds expressions for checked pointers
// like _Array_ptr and _Nt_array_ptr.
//
// RUN: %clang_cc1 -fdump-boundsvars -verify -verify-ignore-unexpected=note -verify-ignore-unexpected=warning %s | FileCheck %s

// expected-no-diagnostics

void f1(_Array_ptr<char> param1 : bounds(param1, param1 + 1),
        _Nt_array_ptr<char> param2 : bounds(param2, param2 + 1),
        int x, int y) {
  _Array_ptr<char> p : bounds(p, p + 1) = "a";
  _Nt_array_ptr<char> q : bounds(q, q + 1) = "a";

  _Where p : bounds(p + x, p + y);
  _Where q : count(x);
  _Where param1 : count(x + y);
  _Where param2 : bounds(param2, param2 + y);

  {
    int z;
    _Nt_array_ptr<char> p : bounds(p, p + 1) = "a";
    _Nt_array_ptr<char> q : bounds(q, q + 1) = "a";
    _Nt_array_ptr<char> m : bounds(m, m + 1) = "a";

    _Where q : bounds(p, p + z) _And p : bounds(p, p + z);
    _Where m : bounds(q, q + z);
  }

  {
    int w;
    _Nt_array_ptr<char> a : bounds(a, a + 1) = "a";

    w = 1 _Where a : bounds(a, a + 1);
    x = 2 _Where a : count(x + y + w);
    y = 3 _Where param2 : bounds(param1, param1 + w);
  }

// CHECK-LABEL: In function: f1
// CHECK: BoundsVars:
// CHECK: a: { a }
// CHECK: m: { m }
// CHECK: p: { p }
// CHECK: p: { p q }
// CHECK: param1: { param1 param2 }
// CHECK: param2: { param2 }
// CHECK: q: { q }
// CHECK: q: { m q }
// CHECK: w: { a param2 }
// CHECK: x: { a p param1 q }
// CHECK: y: { a p param1 param2 }
// CHECK: z: { m p q }
}

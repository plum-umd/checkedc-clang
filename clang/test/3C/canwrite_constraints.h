// Note: Only the test in which this file is non-writable verifies diagnostics
// and will use the expected warning comment below. The test in which this file
// is writable does not verify diagnostics and ignores that comment.

// "@+1" means "on the next line". If we put the comment on the same line, it
// breaks the CHECK_HIGHER.
// expected-warning@+1 {{Declaration in non-writable file}}
inline void foo(int *p) {}
// CHECK_HIGHER: inline void foo(_Ptr<int> p) _Checked {}

// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

int **func(int **p, int *x) {
	//CHECK_NOALL: int **func(int **p : itype(_Ptr<_Ptr<int>>), _Ptr<int> x) : itype(_Ptr<int *>) {
	//CHECK_ALL: _Array_ptr<_Ptr<int>> func(_Array_ptr<_Ptr<int>> p, _Ptr<int> x) _Checked {
  return &(p[1]);
} 

struct foo { int **b; int n; };
	//CHECK_NOALL: struct foo { int **b; int n; };
	//CHECK_ALL: struct foo { _Array_ptr<_Ptr<int>> b; int n; };
int **bar(struct foo *p) {
	//CHECK_NOALL: int **bar(_Ptr<struct foo> p) : itype(_Ptr<int *>) {
	//CHECK_ALL: _Array_ptr<_Ptr<int>> bar(_Ptr<struct foo> p) _Checked {
  int *n = &p->n;
	//CHECK: _Ptr<int> n = &p->n;
  return &(p->b[1]);
}

struct s { int *c; };
	//CHECK: struct s { _Ptr<int> c; };
int **getarr(struct s *q) {
	//CHECK: _Ptr<_Ptr<int>> getarr(_Ptr<struct s> q) _Checked {
  return &q->c;
}


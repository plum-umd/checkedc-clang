// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

/* Tests for the error outlined in issue 74. Pointer arithmetic on function
 pointers with -alltypes active causes the pointers be ARR pointers, but
 this operation is not defined. */

int add(int,int);

/* Baseline check of the behavior of function pointers. A function pointer
 before pointer arithmetic is used should be a checked pointer regardless of
 alltypes flag. */
void basic_fn_ptr() {
	//CHECK: void basic_fn_ptr() _Checked {
    int (*x0) (int, int) = add;
	//CHECK: _Ptr<int (int , int )> x0 = add;
}

/* Tests of bad Pointer arithmetic that should result in WILD pointers.
 As described in issue #74, these are rewritten as ARR pointers when
 alltypes is active. */
void bad_ptr_arith() {
    int (*x0) (int, int) = add;
	//CHECK: int (*x0) (int, int) = add;
    x0++;

    int (*x1) (int, int) = add;
	//CHECK: int (*x1) (int, int) = add;
    x1--;

    int (*x2) (int, int) = add;
	//CHECK: int (*x2) (int, int) = add;
    ++x2;

    int (*x3) (int, int) = add;
	//CHECK: int (*x3) (int, int) = add;
    --x3;

    int (*x4) (int, int) = add;
	//CHECK: int (*x4) (int, int) = add;
    x4 += 1;

    int (*x5) (int, int) = add;
	//CHECK: int (*x5) (int, int) = add;
    x5 -= 1;
}

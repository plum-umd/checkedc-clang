// This regression test is for a 3C bug that affected vsftpd, which only builds
// on Linux. Windows does not have `struct sigaction`, and we couldn't find a
// reasonable way to write an analogous test that works on Windows.
// UNSUPPORTED: system-windows

// RUN: %S/3c-regtest.py --predefined-script common %s -t %t --clang '%clang'

#include <signal.h>

void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))
	//CHECK: void vsf_sysutil_set_sighandler(int sig, void (*p_handlefunc)(int))
{
    int retval;
    struct sigaction sigact;
    sigact.sa_handler = p_handlefunc;
} 

/*ensure trivial conversion*/
void foo(int *x) { 
	//CHECK: void foo(_Ptr<int> x) _Checked { 

}

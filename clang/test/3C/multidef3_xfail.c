// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -output-dir=%t.checked %s --

// XFAIL: *

// The desired behavior in this case is to fail, so other checks are omitted

// Test body vs no body

void foo(char **y) {
  // this is the body
}

void foo(char *x);


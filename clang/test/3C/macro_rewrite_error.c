// RUN: 3c -base-dir=%S -extra-arg="-Wno-everything" -verify -alltypes %s --

#define args ();
typedef int (*a) args // expected-error {{Unable to rewrite converted source range. Intended rewriting: "typedef _Ptr<int (void)> a"}}
a b;

#define MIDDLE x; int *
int MIDDLE y; // expected-error {{Unable to rewrite converted source range. Intended rewriting: "_Ptr<int> y = ((void *)0)"}}

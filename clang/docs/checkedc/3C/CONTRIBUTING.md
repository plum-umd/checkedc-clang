# Contributing to 3C

Issues and pull requests related to 3C should be submitted to [CCI's
`checkedc-clang`
repository](https://github.com/correctcomputation/checkedc-clang), not
[Microsoft's](https://github.com/microsoft/checkedc-clang), except as
stated below.

## Issues

Feel free to file issues against 3C, even during this early stage in
its development; filed issues will help inform us of what users want.
Please try to provide enough information that we can reproduce the bug
you are reporting or understand the specific enhancement you are
proposing. We strive to promptly fix any issues that block basic usage
of 3C in our [supported
environments](INSTALL.md#supported-environments).

## Pull requests

We are open to outside contributions to 3C, though we do not yet have
a sense of how much work we'll be willing to spend on integrating
them. As with any open-source project, we advise you to check with us
(e.g., by filing an issue) before you put nontrivial work into a
contribution in the hope that we'll accept it. Of course, you can also
consider that even if we do not accept your contribution, you will be
able to use your work yourself (subject to your ability to keep up
with us) and we may even direct users who want the functionality to
your version.

If your contribution does not touch any 3C-specific code (or is a
codebase-wide cleanup of low risk to 3C) and you can reasonably submit
it to [Microsoft's
repository](https://github.com/microsoft/checkedc-clang) instead, we
generally prefer that you do so. If such a contribution has particular
benefit to 3C, feel free to let us know, and we may assist you in
getting your contribution accepted upstream and/or ensure it is merged
quickly to CCI's repository.

If the previous paragraph does not apply, just submit a pull request
to CCI's repository. You must grant the same license on your
contribution as the existing codebase. We do not have a formal
contributor license agreement (CLA) process at this time, but we may
set one up and require you to complete it before we accept your
contribution. Also be aware that we need to keep 5C ([our proprietary
extension of
3C](README.md#what-3c-users-should-know-about-the-development-process))
working, so you may have to wait for us to address 5C-specific
problems arising from your 3C pull request and/or we may ask you to
make specific changes to your pull request to accommodate 5C's code.

## Testing

3C has a regression test suite located in `clang/test/3C`. At the
appropriate time during development of a pull request, please run it
and correct any failures. (For example, it may not make sense to run
it on a draft pull request containing an unfinished demonstration of
an idea.) The easiest way to run it is to run the following in your
build directory:

```
ninja check-3c
```

This command will build everything needed that hasn't already been
built, run the test suite, report success or failure (exit 0 or 1, so
you can use it in scripts), and display some information about any
failures, which may or may not be enough for you to understand what
went wrong.

For deeper troubleshooting, run the following in your build directory
to build all dependencies of the test suite:

```
ninja check-3c-deps
```

Then run the following in the `clang/test/3C` directory:

```
llvm-lit -vv TEST.c
```

where `TEST.c` is the path of the test you want to run (you can also
specify more than one test). This assumes you've put the `bin`
subdirectory of your build directory on your `$PATH` or arranged some
other means of running `llvm-lit` from there. The first `-v` makes
`llvm-lit` display the stdout and stderr of failed tests; the second
makes it display the `RUN` commands as they execute so you can tell
which one failed.

Every `.c` file under `clang/test/3C` is a test file. There are a few
in subdirectories, so `*.c` will not pick up all of them; instead you
can use `llvm-lit -vv .` to specify all test files under the current
directory.

### Diagnostic verification

3C supports the standard Clang diagnostic verifier
([`VerifyDiagnosticConsumer`](https://clang.llvm.org/doxygen/classclang_1_1VerifyDiagnosticConsumer.html#details))
for testing errors and warnings reported by 3C via its main `DiagnosticsEngine`.
(Some 3C errors and warnings are reported via other means and cannot be tested
this way; the best solution we have for them right now is to `grep` the stderr
of 3C.) Diagnostic verification can be enabled via the usual `-Xclang -verify`
compiler option; other diagnostic verification options (`-Xclang
-verify=PREFIX`, etc.) should also work as normal. These must be passed as
_compiler_ options, not `3c` options; for example, if you are using `--` on the
`3c` command line, these options must be passed _after_ the `--`.

Some notes about diagnostic verification in the context of 3C:

* Parsing of the source files uses some of the compiler logic and thus may
  generate compiler warnings, just as if you ran `clang` on the code. These are
  sent to the diagnostic verifier along with diagnostics generated by 3C's
  analysis. If you find it distracting to have to include the compiler warnings
  in the set of expected diagnostics for a test, you can turn them off via the
  `-Wno-everything` compiler option (which does not affect diagnostics generated
  by 3C's analysis).

* The `3c` tool works in several passes, where each pass runs on all translation
  units: first `3c` parses the source files, then it runs several passes of
  analysis. If a pass encounters at least one error, `3c` exits at the end of
  that pass. Diagnostic verification does not change the _point_ at which `3c`
  exits, but it changes the exit _code_ to indicate the result of verification
  rather than the presence of errors. The verification includes the diagnostics
  from all passes up to the point at which `3c` exits (i.e., the same
  diagnostics that would be displayed if verification were not used). However,
  an error that doesn't go via the main `DiagnosticsEngine` will cause an
  unsuccessful exit code regardless of diagnostic verification. (This is
  typically the behavior you want for a test.)

* Diagnostic verification is independent for each translation unit, so in tests
  with multiple translation units, you'll have to be careful that preprocessing
  of each translation unit sees the correct set of `expected-*` directives for
  the diagnostics generated for that translation unit (or an
  `expected-no-diagnostics` directive if that translation unit generates no
  diagnostics, even if other translation units do generate diagnostics). Be
  warned that since which translation unit generated a given diagnostic isn't
  visible to a normal user, we don't put much work into coming up with sensible
  rules for this, but it should at least be deterministic for testing.

Note that some 3C tests use diagnostic verification on calls to `clang` rather
than `3c`, so if you see `expected-*` directives in a test, you can look at the
`RUN` commands to see which command has `-Xclang -verify` and is using the
directives. If you want to verify diagnostics of more than one `RUN` command in
the same test, you can use different directive prefixes (`-Xclang
-verify=PREFIX`).

## Coding guidelines

Please follow [LLVM coding
standards](https://llvm.org/docs/CodingStandards.html#name-types-functions-variables-and-enumerators-properly)
in your code. Specifically:

* The maximum length of a line: 80 chars

* All comments should start with a Capital letter.

* All Local variables, including fields and parameters, should start
  with a Capital letter (similar to PascalCase). Short names are
  preferred.

* A space between the conditional keyword and `(` i.e., `if (`,
  `while (`, ``for (` etc.

* Space after the type name, i.e., `Type *K` _not_ `Type* K`.

* Space before and after `:` in iterators, i.e., `for (auto &k : List)`

Our goal is that all files should be formatted with `clang-format` and
pass `clang-tidy` ([more information](clang-tidy.md)), and nonempty
files should have a final newline (surprisingly, `clang-format` cannot
enforce this). However, until we have better automation, we decided it
isn't reasonable to require contributors to manually run these tools
and fix style nits in each change; instead, we periodically run the
tools on the entire 3C codebase.

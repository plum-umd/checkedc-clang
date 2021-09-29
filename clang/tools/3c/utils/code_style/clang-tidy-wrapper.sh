#!/bin/bash
# Adds some flags we want for the lint-3c-clang-tidy target, since
# run-clang-tidy.py doesn't support passing arbitrary flags through to
# clang-tidy. :(

exec clang-tidy -warnings-as-errors='*' "$@"

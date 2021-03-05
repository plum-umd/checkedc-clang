#!/bin/bash
# usage: compile_converted_project.sh PATH_TO_clang [MAKE_ARGS...]
# The working directory must be the project.

clang="$1"
shift

find . -name '*.checked.*' | while read f_checked; do
    [[ "$f_checked" =~ ^(.*)\.checked(\..*)$ ]] || {
        echo >&2 "Weird filename: $f_checked"
        continue
    }
    f="${BASH_REMATCH[1]}${BASH_REMATCH[2]}"
    mv "$f_checked" "$f"
    # Be sure to trigger the build system to recompile.
    touch "$f"
done

# Keep going after one file fails to compile so we can see compile errors in all
# files.
make -k CC="$clang" "$@"

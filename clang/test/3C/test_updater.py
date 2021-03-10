#!/usr/bin/env python3
import fileinput 
import sys
import os

import find_bin
bin_path = find_bin.bin_path

structs = """
struct np {
    int x;
    int y;
};

struct p {
    int *x;
    char *y;
};


struct r {
    int data;
    struct r *next;
};
"""

# Let's clear all the existing annotations to get a clean fresh file with only code
def strip_existing_annotations(filename): 
    for line in fileinput.input(filename, inplace=1):
        if "// RUN" in line or "//CHECK" in line: 
            line = "" 
        sys.stdout.write(line)


def process_file_smart(name, cnameNOALL, cnameALL, diff): 
    file = open(name, "r") 
    noallfile = open(cnameNOALL, "r") 
    allfile = open(cnameALL, "r") 

    # gather all the lines
    lines = str(file.read()).split("\n") 
    noall = str(noallfile.read()).split("\n") 
    yeall = str(allfile.read()).split("\n") 

    file.close() 
    noallfile.close() 
    allfile.close() 
    
    # ensure all lines are the same length
    assert len(lines) == len(noall) == len(yeall), "fix file " + name

    # If the assertion fails, it's helpful to keep these files for
    # troubleshooting, so don't delete them until after it passes.
    os.system("rm -r tmp.checkedALL tmp.checkedNOALL")

    # our keywords that indicate we should add an annotation
    keywords = "int char struct double float".split(" ") 
    ckeywords = "_Ptr _Array_ptr _Nt_array_ptr _Checked _Unchecked".split(" ") 

    for i in range(0, len(lines)): 
        line = lines[i] 
        noline = noall[i] 
        yeline = yeall[i]
        if ("/* GENERATE CHECK */" in line or (line.find("extern") == -1 and line.find("/*") == -1 and ((any(substr in line for substr in keywords) and (line.find("*") != -1 or line.find("[") != -1)) or any(substr in noline for substr in ckeywords) or any(substr in yeline for substr in ckeywords)))): 
            if noline == yeline: 
                lines[i] += "\n\t//CHECK: " + noline.lstrip()
            else: 
                lines[i] += "\n\t//CHECK_NOALL: " + noline.lstrip()
                lines[i] += "\n\t//CHECK_ALL: " + yeline
    
    diff_opt = " -w" if diff else ""
    run = f"""\
// RUN: rm -rf %t*
// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_ALL\",\"CHECK\" %s
// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_NOALL\",\"CHECK\" %s
// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -
// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --
// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/{name} -- | diff{diff_opt} %t.checked/{name} -
"""

    file = open(name, "w+")
    file.write(run + "\n".join(lines)) 
    file.close()
    return 

def process_smart(filename, diff): 
    strip_existing_annotations(filename) 
    
    cnameNOALL = "tmp.checkedNOALL/" + filename
    cnameALL = "tmp.checkedALL/" + filename

    os.system("{}3c -alltypes -addcr -output-dir=tmp.checkedALL {} --".format(bin_path, filename))
    os.system("{}3c -addcr -output-dir=tmp.checkedNOALL {} --".format(bin_path, filename))

    process_file_smart(filename, cnameNOALL, cnameALL, diff) 
    return


# yapf: disable
manual_tests = [
    '3d-allocation.c',
    'amper.c',
    'calloc.c',
    'canonical_type_cast.c',
    'checkedregions.c',
    'compound_literal.c',
    'ex1.c',
    'extGVar.c',
    'extstructfields.c',
    'fn_sets.c',
    'fp.c',
    'fp_arith.c',
    'funcptr1.c',
    'funcptr2.c',
    'funcptr3.c',
    'funcptr4.c',
    'graphs.c',
    'graphs2.c',
    'gvar.c',
    'i1.c',
    'i2.c',
    'i3.c',
    'linkedlist.c',
    'malloc_array.c',
    'ptr_array.c',
    'ptrptr.c',
    'realloc.c',
    'realloc_complex.c',
    'refarrsubscript.c',
    'return_not_least.c',
    'single_ptr_calloc.c',
    'subtyp.c',
    'unsafeunion.c',
    'unsigned_signed_cast.c',
    'valist.c',
    'cast.c',
]
# yapf: enable

# No tests currently produce whitespace differences.
need_diff = []

for i in manual_tests: 
    process_smart(i, False) 
for i in need_diff: 
    process_smart(i, True)

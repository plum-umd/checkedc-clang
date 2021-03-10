#!/usr/bin/env python
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
        if "/* GENERATE CHECK */" in line or (line.find("extern") == -1 and line.find("/*") == -1 and ((any(substr in line for substr in keywords) and (line.find("*") != -1 or line.find("[") != -1)) or any(substr in noline for substr in ckeywords) or any(substr in yeline for substr in ckeywords))): 
            if noline == yeline: 
                lines[i] = line + "\n\t//CHECK: " + noline.lstrip()
            else: 
                lines[i] = line + "\n\t//CHECK_NOALL: " + noline.lstrip() + "\n\t//CHECK_ALL: " + yeline
    
    run = "// RUN: rm -rf %t*"
    run += "\n// RUN: 3c -base-dir=%S -alltypes -addcr %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_ALL\",\"CHECK\" %s"
    run += "\n// RUN: 3c -base-dir=%S -addcr %s -- | FileCheck -match-full-lines -check-prefixes=\"CHECK_NOALL\",\"CHECK\" %s"
    run += "\n// RUN: 3c -base-dir=%S -addcr %s -- | %clang -c -fcheckedc-extension -x c -o /dev/null -"
    run += "\n// RUN: 3c -base-dir=%S -output-dir=%t.checked -alltypes %s --"
    run += "\n// RUN: 3c -base-dir=%t.checked -alltypes %t.checked/{} -- | diff{} %t.checked/{} -\n".format(
        name, " -w" if diff else "", name)

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


manual_tests = ['3d-allocation.c',
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
 'cast.c'] 

# No tests currently produce whitespace differences.
need_diff = []

b_tests = ['b10_allsafepointerstruct.c',
 'b11_calleestructnp.c',
 'b12_callerstructnp.c',
 'b13_calleestructp.c',
 'b14_callerstructp.c',
 'b15_calleepointerstruct.c',
 'b16_callerpointerstruct.c',
 'b17_bothstructnp.c',
 'b18_bothstructp.c',
 'b19_bothpointerstruct.c',
 'b1_allsafe.c',
 'b20_allsafepointerstructproto.c',
 'b21_calleepointerstructproto.c',
 'b22_callerpointerstructproto.c',
 'b23_explicitunsafecast.c',
 'b23_retswitchexplicit.c',
 'b24_implicitunsafecast.c',
 'b24_retswitchimplicit.c',
 'b25_castprotosafe.c',
 'b26_castprotounsafe.c',
 'b26_castprotounsafeimplicit.c',
 'b26_castprotounsafeimplicitretswitch.c',
 'b27_structcastsafe.c',
 'b28_structcastexplicit.c',
 'b28_structcastimplicit.c',
 'b28_structimplicitretcast.c',
 'b29_structprotocastsafe.c',
 'b29_structprotocastsafeuseunsafe.c',
 'b2_calleeunsafe.c',
 'b30_structprotocastexplicitunsafeuseunsafe.c',
 'b30_structprotocastimplicitunsafeuseunsafe.c',
 'b30_structprotocastunsafeexplicit.c',
 'b30_structprotocastunsafeimplicit.c',
 'b30_structprotocastunsafeimplicitretswitch.c',
 'b30_structprotoconflict.c',
 'b30_structprotoconflictbodyconvert.c',
 'b3_onecallerunsafe.c',
 'b4_bothunsafe.c',
 'b5_calleeunsafeproto.c',
 'b6_callerunsafeproto.c',
 'b7_allsafeproto.c',
 'b8_allsafestructnp.c',
 'b9_allsafestructp.c']

for i in manual_tests: 
    process_smart(i, False) 
for i in need_diff: 
    process_smart(i, True)

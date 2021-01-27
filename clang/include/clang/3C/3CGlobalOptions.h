//=--3CGlobalOptions.h--------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tool options that are visible to all the components.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_3CGLOBALOPTIONS_H
#define LLVM_CLANG_3C_3CGLOBALOPTIONS_H

#include "llvm/Support/CommandLine.h"

extern bool Verbose;
extern bool DumpIntermediate;
extern bool HandleVARARGS;
extern bool SeperateMultipleFuncDecls;
extern bool EnablePropThruIType;
extern bool ConsiderAllocUnsafe;
extern bool AllTypes;
extern bool NewSolver;
extern std::string BaseDir;
extern std::vector<std::string> AllocatorFunctions;
extern bool AddCheckedRegions;
extern bool WarnRootCause;
extern bool WarnAllRootCause;
extern bool DumpUnwritableChanges;

#ifdef FIVE_C
extern bool RemoveItypes;
extern bool ForceItypes;
#endif

#endif // LLVM_CLANG_3C_3CGLOBALOPTIONS_H

//=--3C.h---------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The main interface for invoking 3C tool.
// This provides various methods that can be used to access different
// aspects of the 3C tool.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_3C_H
#define LLVM_CLANG_3C_3C_H

#include "clang/3C/3CInteractiveData.h"
#include "clang/3C/ConstraintVariables.h"
#include "clang/3C/PersistentSourceLoc.h"
#include "clang/3C/ProgramInfo.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include <mutex>

// Options used to initialize 3C tool.
//
// See clang/docs/checkedc/3C/clang-tidy.md#_3c-name-prefix
// NOLINTNEXTLINE(readability-identifier-naming)
struct _3COptions {
  bool DumpIntermediate;

  bool Verbose;

  std::string OutputPostfix;
  std::string OutputDir;

  std::string ConstraintOutputJson;

  bool DumpStats;

  std::string StatsOutputJson;

  std::string WildPtrInfoJson;

  std::string PerPtrInfoJson;

  std::vector<std::string> AllocatorFunctions;

  bool HandleVARARGS;

  bool EnablePropThruIType;

  std::string BaseDir;
  bool AllowSourcesOutsideBaseDir;

  bool EnableAllTypes;

  bool AddCheckedRegions;

  bool DisableCCTypeChecker;

  bool WarnRootCause;

  bool WarnAllRootCause;

#ifdef FIVE_C
  bool RemoveItypes;
  bool ForceItypes;
#endif

  // Currently applies only to the rewriting phase (because it is the only phase
  // that generates diagnostics, except for the declaration merging diagnostics
  // that are currently fatal) and uses the default "expected" prefix.
  bool VerifyDiagnosticOutput;

  bool DumpUnwritableChanges;
  bool AllowUnwritableChanges;

  bool AllowRewriteFailures;
};

// The main interface exposed by the 3C to interact with the tool.
//
// See clang/docs/checkedc/3C/clang-tidy.md#_3c-name-prefix
// NOLINTNEXTLINE(readability-identifier-naming)
class _3CInterface {

public:
  ProgramInfo GlobalProgramInfo;
  // Mutex for this interface.
  std::mutex InterfaceMutex;

  // If the parameters are invalid, this function prints an error message to
  // stderr and returns null.
  //
  // There's no way for a constructor to report failure (we do not use
  // exceptions), so use a factory method instead. Ideally we'd use an
  // "optional" datatype that doesn't force heap allocation, but the only such
  // datatype that is accepted in our codebase
  // (https://llvm.org/docs/ProgrammersManual.html#fallible-constructors) seems
  // too unwieldy to use right now.
  static std::unique_ptr<_3CInterface>
  create(const struct _3COptions &CCopt,
         const std::vector<std::string> &SourceFileList,
         clang::tooling::CompilationDatabase *CompDB);

  // Constraint Building.

  // Create ConstraintVariables to hold constraints
  bool addVariables();

  // Build initial constraints.
  bool buildInitialConstraints();

  // Constraint Solving.
  bool solveConstraints();

  // Interactivity.

  // Get all the WILD pointers and corresponding reason why they became WILD.
  ConstraintsInfo &getWildPtrsInfo();

  // Given a constraint key make the corresponding constraint var
  // to be non-WILD.
  bool makeSinglePtrNonWild(ConstraintKey TargetPtr);

  // Make the provided pointer non-WILD and also make all the
  // pointers, which are wild because of the same reason, as non-wild
  // as well.
  bool invalidateWildReasonGlobally(ConstraintKey PtrKey);

  // Rewriting.

  // Write all converted versions of the files in the source file list
  // to disk
  bool writeAllConvertedFilesToDisk();
  // Write the current converted state of the provided file.
  bool writeConvertedFileToDisk(const std::string &FilePath);

private:
  _3CInterface(const struct _3COptions &CCopt,
               const std::vector<std::string> &SourceFileList,
               clang::tooling::CompilationDatabase *CompDB, bool &Failed);

  // Are constraints already built?
  bool ConstraintsBuilt;
  void invalidateAllConstraintsWithReason(Constraint *ConstraintToRemove);
};

#endif // LLVM_CLANG_3C_3C_H

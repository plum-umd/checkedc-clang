//=--3CStandalone.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// 3C tool
//
//===----------------------------------------------------------------------===//

#include "clang/3C/3C.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;
// See clang/docs/checkedc/3C/clang-tidy.md#_3c-name-prefix
// NOLINTNEXTLINE(readability-identifier-naming)
static cl::OptionCategory _3CCategory("3C options");
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("");

static cl::opt<bool> OptDumpIntermediate("dump-intermediate",
                                         cl::desc("Dump intermediate "
                                                  "information"),
                                         cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool> OptVerbose("verbose",
                                cl::desc("Print verbose "
                                         "information"),
                                cl::init(false), cl::cat(_3CCategory));

static cl::opt<std::string>
    OptOutputPostfix("output-postfix",
                     cl::desc("Postfix to add to the names of rewritten "
                              "files, if not supplied writes to STDOUT"),
                     cl::init("-"), cl::cat(_3CCategory));

static cl::opt<std::string>
    OptMalloc("use-malloc",
              cl::desc("Allows for the usage of user-specified "
                       "versions of function allocators"),
              cl::init(""), cl::cat(_3CCategory));

static cl::opt<std::string>
    OptConstraintOutputJson("constraint-output",
                            cl::desc("Path to the file where all the analysis "
                                     "information will be dumped as json"),
                            cl::init("constraint_output.json"),
                            cl::cat(_3CCategory));

static cl::opt<std::string>
    OptStatsOutputJson("stats-output",
                       cl::desc("Path to the file where all the stats "
                                "will be dumped as json"),
                       cl::init("TotalConstraintStats.json"),
                       cl::cat(_3CCategory));
static cl::opt<std::string>
    OptWildPtrInfoJson("wildptrstats-output",
                       cl::desc("Path to the file where all the info "
                                "related to WILD ptr grouped by reason"
                                " will be dumped as json"),
                       cl::init("WildPtrStats.json"), cl::cat(_3CCategory));

static cl::opt<std::string> OptPerPtrWILDInfoJson(
    "perptrstats-output",
    cl::desc("Path to the file where all the info "
             "related to each WILD ptr will be dumped as json"),
    cl::init("PerWildPtrStats.json"), cl::cat(_3CCategory));

static cl::opt<bool> OptDumpStats("dump-stats", cl::desc("Dump statistics"),
                                  cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool> OptHandleVARARGS("handle-varargs",
                                      cl::desc("Enable handling of varargs "
                                               "in a "
                                               "sound manner"),
                                      cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool>
    OptEnablePropThruIType("enable-itypeprop",
                           cl::desc("Enable propagation of "
                                    "constraints through ityped "
                                    "parameters/returns."),
                           cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool> OptAllTypes("alltypes",
                                 cl::desc("Consider all Checked C types for "
                                          "conversion"),
                                 cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool> OptAddCheckedRegions("addcr",
                                          cl::desc("Add Checked "
                                                   "Regions"),
                                          cl::init(false),
                                          cl::cat(_3CCategory));

static cl::opt<bool>
    OptDiableCCTypeChecker("disccty",
                           cl::desc("Do not disable checked c type checker."),
                           cl::init(false), cl::cat(_3CCategory));

static cl::opt<std::string>
    OptBaseDir("base-dir",
               cl::desc("Base directory for the code we're translating"),
               cl::init(""), cl::cat(_3CCategory));

static cl::opt<bool> OptWarnRootCause(
    "warn-root-cause",
    cl::desc("Emit warnings indicating root causes of unchecked pointers."),
    cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool>
    OptWarnAllRootCause("warn-all-root-cause",
                        cl::desc("Emit warnings for all root causes, "
                                 "even those unlikely to be interesting."),
                        cl::init(false), cl::cat(_3CCategory));

// https://clang.llvm.org/doxygen/classclang_1_1VerifyDiagnosticConsumer.html#details
//
// Analogous to the -verify option of `clang -cc1`, but currently applies only
// to the rewriting phase (because it is the only phase that generates
// diagnostics, except for the declaration merging diagnostics that are
// currently fatal). No checking of diagnostics from the other phases is
// performed. We cannot simply have the caller pass `-extra-arg=-Xclang
// -extra-arg=-verify` because that would expect each phase to produce the same
// set of diagnostics.
static cl::opt<bool> OptVerifyDiagnosticOutput(
    "verify",
    cl::desc("Verify diagnostic output (for automated testing of 3C)."),
    cl::init(false), cl::cat(_3CCategory), cl::Hidden);

#ifdef FIVE_C
static cl::opt<bool> OptRemoveItypes(
    "remove-itypes",
    cl::desc("Remove unneeded interoperation type annotations."),
    cl::init(false), cl::cat(_3CCategory));

static cl::opt<bool> OptForceItypes(
    "force-itypes",
    cl::desc("Use interoperation types instead of regular checked pointers. "),
    cl::init(false), cl::cat(_3CCategory));
#endif

int main(int argc, const char **argv) {
  sys::PrintStackTraceOnErrorSignal(argv[0]);

  // Initialize targets for clang module support.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  CommonOptionsParser OptionsParser(argc, (const char **)(argv), _3CCategory);
  // Setup options.
  struct _3COptions CcOptions;
  CcOptions.BaseDir = OptBaseDir.getValue();
  CcOptions.EnablePropThruIType = OptEnablePropThruIType;
  CcOptions.HandleVARARGS = OptHandleVARARGS;
  CcOptions.DumpStats = OptDumpStats;
  CcOptions.OutputPostfix = OptOutputPostfix.getValue();
  CcOptions.Verbose = OptVerbose;
  CcOptions.DumpIntermediate = OptDumpIntermediate;
  CcOptions.ConstraintOutputJson = OptConstraintOutputJson.getValue();
  CcOptions.StatsOutputJson = OptStatsOutputJson.getValue();
  CcOptions.WildPtrInfoJson = OptWildPtrInfoJson.getValue();
  CcOptions.PerPtrInfoJson = OptPerPtrWILDInfoJson.getValue();
  CcOptions.AddCheckedRegions = OptAddCheckedRegions;
  CcOptions.EnableAllTypes = OptAllTypes;
  CcOptions.DisableCCTypeChecker = OptDiableCCTypeChecker;
  CcOptions.WarnRootCause = OptWarnRootCause;
  CcOptions.WarnAllRootCause = OptWarnAllRootCause;
  CcOptions.VerifyDiagnosticOutput = OptVerifyDiagnosticOutput;

#ifdef FIVE_C
  CcOptions.RemoveItypes = OptRemoveItypes;
  CcOptions.ForceItypes = OptForceItypes;
#endif

  //Add user specified function allocators
  std::string Malloc = OptMalloc.getValue();
  if (!Malloc.empty()) {
    std::string Delimiter = ",";
    size_t Pos = 0;
    std::string Token;
    while ((Pos = Malloc.find(Delimiter)) != std::string::npos) {
      Token = Malloc.substr(0, Pos);
      CcOptions.AllocatorFunctions.push_back(Token);
      Malloc.erase(0, Pos + Delimiter.length());
    }
    Token = Malloc;
    CcOptions.AllocatorFunctions.push_back(Token);
  } else
    CcOptions.AllocatorFunctions = {};

  // Create 3C Interface.
  //
  // See clang/docs/checkedc/3C/clang-tidy.md#_3c-name-prefix
  // NOLINTNEXTLINE(readability-identifier-naming)
  _3CInterface _3CInterface(CcOptions, OptionsParser.getSourcePathList(),
                            &(OptionsParser.getCompilations()));

  if (OptVerbose)
    errs() << "Calling Library to building Constraints.\n";
  // First build constraints.
  if (!_3CInterface.buildInitialConstraints()) {
    errs() << "Failure occurred while trying to build constraints. Exiting.\n";
    return 1;
  }

  if (OptVerbose) {
    errs() << "Finished Building Constraints.\n";
    errs() << "Trying to solve Constraints.\n";
  }

  // Next solve the constraints.
  if (!_3CInterface.solveConstraints(OptWarnRootCause)) {
    errs() << "Failure occurred while trying to solve constraints. Exiting.\n";
    return 1;
  }

  if (OptVerbose) {
    errs() << "Finished solving constraints.\n";
    errs() << "Trying to rewrite the converted files back.\n";
  }

  // Write all the converted files back.
  if (!_3CInterface.writeAllConvertedFilesToDisk()) {
    errs() << "Failure occurred while trying to rewrite converted files back."
              "Exiting.\n";
    return 1;
  }

  return 0;
}

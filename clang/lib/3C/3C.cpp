//=--3C.cpp-------------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Implementation of various method in 3C.h
//
//===----------------------------------------------------------------------===//

#include "clang/3C/3C.h"
#include "clang/3C/ArrayBoundsInferenceConsumer.h"
#include "clang/3C/ConstraintBuilder.h"
#include "clang/3C/IntermediateToolHook.h"
#include "clang/3C/RewriteUtils.h"
#include "clang/Tooling/ArgumentsAdjusters.h"
#include "llvm/Support/TargetSelect.h"

using namespace clang::driver;
using namespace clang::tooling;
using namespace clang;
using namespace llvm;

// Suffixes for constraint output files.ParameterGatherer
#define INITIAL_OUTPUT_SUFFIX "_initial_constraints"
#define FINAL_OUTPUT_SUFFIX "_final_output"
#define BEFORE_SOLVING_SUFFIX "_before_solving_"
#define AFTER_SUBTYPING_SUFFIX "_after_subtyping_"

cl::OptionCategory ArrBoundsInferCat("Array bounds inference options");
static cl::opt<bool>
    DebugArrSolver("debug-arr-solver",
                   cl::desc("Dump array bounds inference graph"),
                   cl::init(false), cl::cat(ArrBoundsInferCat));

bool DumpIntermediate;
bool Verbose;
std::string OutputPostfix;
std::string OutputDir;
std::string ConstraintOutputJson;
std::vector<std::string> AllocatorFunctions;
bool DumpStats;
bool HandleVARARGS;
bool EnablePropThruIType;
bool ConsiderAllocUnsafe;
std::string StatsOutputJson;
std::string WildPtrInfoJson;
std::string PerWildPtrInfoJson;
bool AllTypes;
std::string BaseDir;
bool AddCheckedRegions;
bool DisableCCTypeChecker;
bool WarnRootCause;
bool WarnAllRootCause;
std::set<std::string> FilePaths;
bool VerifyDiagnosticOutput;
bool DumpUnwritableChanges;
bool AllowUnwritableChanges;
bool AllowRewriteFailures;

#ifdef FIVE_C
bool RemoveItypes;
bool ForceItypes;
#endif

static CompilationDatabase *CurrCompDB = nullptr;
static tooling::CommandLineArguments SourceFiles;

ArgumentsAdjuster getIgnoreCheckedPointerAdjuster() {
  return [](const CommandLineArguments &Args, StringRef /*unused*/) {
    CommandLineArguments AdjustedArgs;
    bool HasAdjuster = false;
    for (size_t I = 0, E = Args.size(); I < E; ++I) {
      StringRef Arg = Args[I];
      AdjustedArgs.push_back(Args[I]);
      if (Arg == "-f3c-tool") {
        HasAdjuster = true;
        break;
      }
    }
    if (!DisableCCTypeChecker && !HasAdjuster)
      AdjustedArgs.push_back("-f3c-tool");
    return AdjustedArgs;
  };
}
ArgumentsAdjuster addVerifyAdjuster() {
  return [](const CommandLineArguments &Args, StringRef /*unused*/) {
    CommandLineArguments AdjustedArgs(Args);
    if (std::find(AdjustedArgs.begin(),AdjustedArgs.end(),"-verify")
        == AdjustedArgs.end()) {
      AdjustedArgs.push_back("-Xclang");
      AdjustedArgs.push_back("-verify");
    }
    return AdjustedArgs;
  };
}

void dumpConstraintOutputJson(const std::string &PostfixStr,
                              ProgramInfo &Info) {
  if (DumpIntermediate) {
    std::string FilePath = ConstraintOutputJson + PostfixStr + ".json";
    errs() << "Writing json output to:" << FilePath << "\n";
    std::error_code Ec;
    llvm::raw_fd_ostream OutputJson(FilePath, Ec);
    if (!OutputJson.has_error()) {
      Info.dumpJson(OutputJson);
      OutputJson.close();
    } else {
      Info.dumpJson(llvm::errs());
    }
  }
}

void runSolver(ProgramInfo &Info, std::set<std::string> &SourceFiles) {
  Constraints &CS = Info.getConstraints();

  if (Verbose) {
    errs() << "Trying to capture Constraint Variables for all functions\n";
  }

  // Sanity check.
  assert(CS.checkInitialEnvSanity() && "Invalid initial environment. ");

  dumpConstraintOutputJson(INITIAL_OUTPUT_SUFFIX, Info);

  clock_t StartTime = clock();
  CS.solve();
  if (Verbose) {
    errs() << "Solver time:" << getTimeSpentInSeconds(StartTime) << "\n";
  }
}

std::unique_ptr<_3CInterface>
_3CInterface::create(const struct _3COptions &CCopt,
                     const std::vector<std::string> &SourceFileList,
                     CompilationDatabase *CompDB) {
  bool Failed = false;
  // See clang/docs/checkedc/3C/clang-tidy.md#_3c-name-prefix
  // NOLINTNEXTLINE(readability-identifier-naming)
  std::unique_ptr<_3CInterface> _3CInter(
      new _3CInterface(CCopt, SourceFileList, CompDB, Failed));
  if (Failed) {
    return nullptr;
  }
  return _3CInter;
}

_3CInterface::_3CInterface(const struct _3COptions &CCopt,
                           const std::vector<std::string> &SourceFileList,
                           CompilationDatabase *CompDB, bool &Failed) {

  DumpIntermediate = CCopt.DumpIntermediate;
  Verbose = CCopt.Verbose;
  OutputPostfix = CCopt.OutputPostfix;
  OutputDir = CCopt.OutputDir;
  ConstraintOutputJson = CCopt.ConstraintOutputJson;
  StatsOutputJson = CCopt.StatsOutputJson;
  WildPtrInfoJson = CCopt.WildPtrInfoJson;
  PerWildPtrInfoJson = CCopt.PerPtrInfoJson;
  DumpStats = CCopt.DumpStats;
  HandleVARARGS = CCopt.HandleVARARGS;
  EnablePropThruIType = CCopt.EnablePropThruIType;
  BaseDir = CCopt.BaseDir;
  AllTypes = CCopt.EnableAllTypes;
  AddCheckedRegions = CCopt.AddCheckedRegions;
  DisableCCTypeChecker = CCopt.DisableCCTypeChecker;
  AllocatorFunctions = CCopt.AllocatorFunctions;
  WarnRootCause = CCopt.WarnRootCause || CCopt.WarnAllRootCause;
  WarnAllRootCause = CCopt.WarnAllRootCause;
  VerifyDiagnosticOutput = CCopt.VerifyDiagnosticOutput;
  DumpUnwritableChanges = CCopt.DumpUnwritableChanges;
  AllowUnwritableChanges = CCopt.AllowUnwritableChanges;
  AllowRewriteFailures = CCopt.AllowRewriteFailures;

#ifdef FIVE_C
  RemoveItypes = CCopt.RemoveItypes;
  ForceItypes = CCopt.ForceItypes;
#endif

  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  ConstraintsBuilt = false;

  if (OutputPostfix != "-" && !OutputDir.empty()) {
    errs() << "3C initialization error: Cannot use both -output-postfix and "
              "-output-dir\n";
    Failed = true;
    return;
  }
  if (OutputPostfix == "-" && OutputDir.empty() && SourceFileList.size() > 1) {
    errs() << "3C initialization error: Cannot specify more than one input "
              "file when output is to stdout\n";
    Failed = true;
    return;
  }

  std::string TmpPath;
  std::error_code EC;

  if (BaseDir.empty()) {
    BaseDir = ".";
  }

  // Get the canonical path of the base directory.
  TmpPath = BaseDir;
  EC = tryGetCanonicalFilePath(BaseDir, TmpPath);
  if (EC) {
    errs() << "3C initialization error: Failed to canonicalize base directory "
           << "\"" << BaseDir << "\": " << EC.message() << "\n";
    Failed = true;
    return;
  }
  BaseDir = TmpPath;

  if (!OutputDir.empty()) {
    // tryGetCanonicalFilePath will fail if the output dir doesn't exist yet, so
    // create it first.
    EC = llvm::sys::fs::create_directories(OutputDir);
    if (EC) {
      errs() << "3C initialization error: Failed to create output directory \""
             << OutputDir << "\": " << EC.message() << "\n";
      Failed = true;
      return;
    }
    TmpPath = OutputDir;
    EC = tryGetCanonicalFilePath(OutputDir, TmpPath);
    if (EC) {
      errs() << "3C initialization error: Failed to canonicalize output "
             << "directory \"" << OutputDir << "\": " << EC.message() << "\n";
      Failed = true;
      return;
    }
    OutputDir = TmpPath;
  }

  SourceFiles = SourceFileList;

  bool SawInputOutsideBaseDir = false;
  for (const auto &S : SourceFiles) {
    std::string AbsPath;
    EC = tryGetCanonicalFilePath(S, AbsPath);
    if (EC) {
      errs() << "3C initialization error: Failed to canonicalize source file "
             << "path \"" << S << "\": " << EC.message() << "\n";
      Failed = true;
      continue;
    }
    FilePaths.insert(AbsPath);
    if (!filePathStartsWith(AbsPath, BaseDir)) {
      errs()
          << "3C initialization "
          << (OutputDir != "" || !CCopt.AllowSourcesOutsideBaseDir ? "error"
                                                                   : "warning")
          << ": File \"" << AbsPath
          << "\" specified on the command line is outside the base directory\n";
      SawInputOutsideBaseDir = true;
    }
  }
  if (SawInputOutsideBaseDir) {
    errs() << "The base directory is currently \"" << BaseDir
           << "\" and can be changed with the -base-dir option.\n";
    if (OutputDir != "") {
      Failed = true;
      errs() << "When using -output-dir, input files outside the base "
                "directory cannot be handled because there is no way to "
                "compute their output paths.\n";
    } else if (!CCopt.AllowSourcesOutsideBaseDir) {
      Failed = true;
      errs() << "You can use the -allow-sources-outside-base-dir option to "
                "temporarily downgrade this error to a warning.\n";
    }
  }

  CurrCompDB = CompDB;
  
  GlobalProgramInfo.getPerfStats().startTotalTime();
}

bool _3CInterface::parseASTs() {

  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  auto *Tool = new ClangTool(*CurrCompDB, SourceFiles);
  Tool->appendArgumentsAdjuster(getIgnoreCheckedPointerAdjuster());
  // TODO: This currently only enables compiler diagnostic verification.
  // see https://github.com/correctcomputation/checkedc-clang/issues/425
  // for status.
  if (VerifyDiagnosticOutput)
    Tool->appendArgumentsAdjuster(addVerifyAdjuster());

  // load the ASTs
  if (Tool->buildASTs(ASTs))
    return false;

  // check for compile errors
  unsigned int Errs = 0;
  for (auto &TU : ASTs)
    Errs += TU->getDiagnostics().getClient()->getNumErrors();

  return Errs == 0;
}

bool _3CInterface::addVariables() {

  std::lock_guard<std::mutex> Lock(InterfaceMutex);

    // 1. Add Variables.
  VariableAdderConsumer VA = VariableAdderConsumer(GlobalProgramInfo, nullptr);
  unsigned int Errs = 0;
  for (auto &TU : ASTs) {
    TU->enableSourceFileDiagnostics();
    VA.HandleTranslationUnit(TU->getASTContext());
    TU->getDiagnostics().getClient()->EndSourceFile();
    Errs += TU->getDiagnostics().getClient()->getNumErrors();
  }

  return Errs == 0;
}

bool _3CInterface::buildInitialConstraints() {

  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  // 2. Gather constraints.
  ConstraintBuilderConsumer CB = ConstraintBuilderConsumer(GlobalProgramInfo, nullptr);
  unsigned int Errs = 0;
  for (auto &TU : ASTs) {
    TU->enableSourceFileDiagnostics();
    CB.HandleTranslationUnit(TU->getASTContext());
    TU->getDiagnostics().getClient()->EndSourceFile();
    Errs += TU->getDiagnostics().getClient()->getNumErrors();
  }
  if (Errs > 0) return false;

  if (!GlobalProgramInfo.link()) {
    errs() << "Linking failed!\n";
    return false;
  }

  ConstraintsBuilt = true;

  return true;
}

bool _3CInterface::solveConstraints() {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  assert(ConstraintsBuilt && "Constraints not yet built. We need to call "
                             "build constraint before trying to solve them.");
  // 3. Solve constraints.
  if (Verbose)
    errs() << "Solving constraints\n";

  if (DumpIntermediate)
    GlobalProgramInfo.dump();

  auto &PStats = GlobalProgramInfo.getPerfStats();

  PStats.startConstraintSolverTime();
  runSolver(GlobalProgramInfo, FilePaths);
  PStats.endConstraintSolverTime();

  if (Verbose)
    errs() << "Constraints solved\n";

  if (WarnRootCause)
    GlobalProgramInfo.computeInterimConstraintState(FilePaths);

  if (DumpIntermediate)
    dumpConstraintOutputJson(FINAL_OUTPUT_SUFFIX, GlobalProgramInfo);

  if (AllTypes) {
    if (DebugArrSolver)
      GlobalProgramInfo.getABoundsInfo().dumpAVarGraph(
          "arr_bounds_initial.dot");

    // Propagate initial data-flow information for Array pointers from
    // bounds declarations.
    GlobalProgramInfo.getABoundsInfo().performFlowAnalysis(&GlobalProgramInfo);

    // 4. Infer the bounds based on calls to malloc and calloc
    AllocBasedBoundsInference ABBI = AllocBasedBoundsInference(GlobalProgramInfo, nullptr);
    unsigned int Errs = 0;
    for (auto &TU : ASTs) {
      TU->enableSourceFileDiagnostics();
      ABBI.HandleTranslationUnit(TU->getASTContext());
      TU->getDiagnostics().getClient()->EndSourceFile();
      Errs += TU->getDiagnostics().getClient()->getNumErrors();
    }
    if (Errs > 0) return false;

    // Propagate the information from allocator bounds.
    GlobalProgramInfo.getABoundsInfo().performFlowAnalysis(&GlobalProgramInfo);
  }

  // 5. Run intermediate tool hook to run visitors that need to be executed
  // after constraint solving but before rewriting.
  IntermediateToolHook ITH = IntermediateToolHook(GlobalProgramInfo, nullptr);
  unsigned int Errs = 0;
  for (auto &TU : ASTs) {
    TU->enableSourceFileDiagnostics();
    ITH.HandleTranslationUnit(TU->getASTContext());
    TU->getDiagnostics().getClient()->EndSourceFile();
    Errs += TU->getDiagnostics().getClient()->getNumErrors();
  }
  if (Errs > 0) return false;

  if (AllTypes) {
    // Propagate data-flow information for Array pointers.
    GlobalProgramInfo.getABoundsInfo().performFlowAnalysis(&GlobalProgramInfo);

    if (DebugArrSolver)
      GlobalProgramInfo.getABoundsInfo().dumpAVarGraph("arr_bounds_final.dot");
  }

  if (DumpStats) {
    GlobalProgramInfo.printStats(FilePaths, llvm::errs(), true);
    GlobalProgramInfo.computeInterimConstraintState(FilePaths);
    std::error_code Ec;
    llvm::raw_fd_ostream OutputJson(StatsOutputJson, Ec);
    if (!OutputJson.has_error()) {
      GlobalProgramInfo.printStats(FilePaths, OutputJson, false, true);
      OutputJson.close();
    }
    std::string AggregateStats = StatsOutputJson + ".aggregate.json";
    llvm::raw_fd_ostream AggrJson(AggregateStats, Ec);
    if (!AggrJson.has_error()) {
      GlobalProgramInfo.print_aggregate_stats(FilePaths, AggrJson);
      AggrJson.close();
    }

    llvm::raw_fd_ostream WildPtrInfo(WildPtrInfoJson, Ec);
    if (!WildPtrInfo.has_error()) {
      GlobalProgramInfo.getInterimConstraintState().printStats(WildPtrInfo);
      WildPtrInfo.close();
    }

    llvm::raw_fd_ostream PerWildPtrInfo(PerWildPtrInfoJson, Ec);
    if (!PerWildPtrInfo.has_error()) {
      GlobalProgramInfo.getInterimConstraintState().printRootCauseStats(
          PerWildPtrInfo, GlobalProgramInfo.getConstraints());
      PerWildPtrInfo.close();
    }
  }

  return true;
}

bool _3CInterface::writeAllConvertedFilesToDisk() {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  // 6. Rewrite the input files.
  RewriteConsumer RC = RewriteConsumer(GlobalProgramInfo);
  unsigned int Errs = 0;
  for (auto &TU : ASTs) {
    TU->enableSourceFileDiagnostics();
    RC.HandleTranslationUnit(TU->getASTContext());
    TU->getDiagnostics().getClient()->EndSourceFile();
    Errs += TU->getDiagnostics().getClient()->getNumErrors();
  }
  if (Errs > 0) return false;

  GlobalProgramInfo.getPerfStats().endTotalTime();
  GlobalProgramInfo.getPerfStats().startTotalTime();
  return true;
}

ConstraintsInfo &_3CInterface::getWildPtrsInfo() {
  return GlobalProgramInfo.getInterimConstraintState();
}

bool _3CInterface::makeSinglePtrNonWild(ConstraintKey TargetPtr) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);
  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getInterimConstraintState();
  auto &CS = GlobalProgramInfo.getConstraints();

  // Get all the current WILD pointers.
  CVars OldWildPtrs = PtrDisjointSet.AllWildAtoms;

  // Delete the constraint that make the provided targetPtr WILD.
  VarAtom *VA = CS.getOrCreateVar(TargetPtr, "q", VarAtom::V_Other);
  Geq NewE(VA, CS.getWild());
  Constraint *OriginalConstraint = *CS.getConstraints().find(&NewE);
  CS.removeConstraint(OriginalConstraint);
  VA->getAllConstraints().erase(OriginalConstraint);
  delete (OriginalConstraint);

  // Reset the constraint system.
  CS.resetEnvironment();

  // Solve the constraints.
  //assert (CS == GlobalProgramInfo.getConstraints());
  runSolver(GlobalProgramInfo, FilePaths);

  // Compute new disjoint set.
  GlobalProgramInfo.computeInterimConstraintState(FilePaths);

  // Get new WILD pointers.
  CVars &NewWildPtrs = PtrDisjointSet.AllWildAtoms;

  // Get the number of pointers that have now converted to non-WILD.
  std::set_difference(OldWildPtrs.begin(), OldWildPtrs.end(),
                      NewWildPtrs.begin(), NewWildPtrs.end(),
                      std::inserter(RemovePtrs, RemovePtrs.begin()));

  return !RemovePtrs.empty();
}

void _3CInterface::invalidateAllConstraintsWithReason(
    Constraint *ConstraintToRemove) {
  // Get the reason for the current constraint.
  std::string ConstraintRsn = ConstraintToRemove->getReason();
  Constraints::ConstraintSet ToRemoveConstraints;
  Constraints &CS = GlobalProgramInfo.getConstraints();
  // Remove all constraints that have the reason.
  CS.removeAllConstraintsOnReason(ConstraintRsn, ToRemoveConstraints);

  // Free up memory by deleting all the removed constraints.
  for (auto *ToDelCons : ToRemoveConstraints) {
    assert(dyn_cast<Geq>(ToDelCons) && "We can only delete Geq constraints.");
    Geq *TCons = dyn_cast<Geq>(ToDelCons);
    auto *Vatom = dyn_cast<VarAtom>(TCons->getLHS());
    assert(Vatom != nullptr && "Equality constraint with out VarAtom as LHS");
    VarAtom *VS = CS.getOrCreateVar(Vatom->getLoc(), "q", VarAtom::V_Other);
    VS->getAllConstraints().erase(TCons);
    delete (ToDelCons);
  }
}

bool _3CInterface::invalidateWildReasonGlobally(ConstraintKey PtrKey) {
  std::lock_guard<std::mutex> Lock(InterfaceMutex);

  CVars RemovePtrs;
  RemovePtrs.clear();

  auto &PtrDisjointSet = GlobalProgramInfo.getInterimConstraintState();
  auto &CS = GlobalProgramInfo.getConstraints();

  CVars OldWildPtrs = PtrDisjointSet.AllWildAtoms;

  // Delete ALL the constraints that have the same given reason.
  VarAtom *VA = CS.getOrCreateVar(PtrKey, "q", VarAtom::V_Other);
  Geq NewE(VA, CS.getWild());
  Constraint *OriginalConstraint = *CS.getConstraints().find(&NewE);
  invalidateAllConstraintsWithReason(OriginalConstraint);

  // Reset constraint solver.
  CS.resetEnvironment();

  // Solve the constraints.
  runSolver(GlobalProgramInfo, FilePaths);

  // Recompute the WILD pointer disjoint sets.
  GlobalProgramInfo.computeInterimConstraintState(FilePaths);

  // Computed the number of removed pointers.
  CVars &NewWildPtrs = PtrDisjointSet.AllWildAtoms;

  std::set_difference(OldWildPtrs.begin(), OldWildPtrs.end(),
                      NewWildPtrs.begin(), NewWildPtrs.end(),
                      std::inserter(RemovePtrs, RemovePtrs.begin()));

  return !RemovePtrs.empty();
}

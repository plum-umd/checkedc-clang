//=--3CStats.cpp--------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of all the methods in 3CStats.h
//===----------------------------------------------------------------------===//

#include "clang/3C/3CStats.h"
#include "clang/3C/ProgramInfo.h"
#include "clang/3C/Utils.h"
#include <stdio.h>
#include <time.h>

void PerformanceStats::startCompileTime() { CompileTimeSt = clock(); }

void PerformanceStats::endCompileTime() {
  CompileTime += getTimeSpentInSeconds(CompileTimeSt);
}

void PerformanceStats::startFindUsedTagNamesTime() {
  FindUsedTagNamesTimeSt = clock();
}

void PerformanceStats::endFindUsedTagNamesTime() {
  FindUsedTagNamesTime += getTimeSpentInSeconds(FindUsedTagNamesTimeSt);
}

void PerformanceStats::startFindMultiDeclsTime() {
  FindMultiDeclsTimeSt = clock();
}

void PerformanceStats::endFindMultiDeclsTime() {
  FindMultiDeclsTime += getTimeSpentInSeconds(FindMultiDeclsTimeSt);
}

void PerformanceStats::startAddVariablesTime() { AddVariablesTimeSt = clock(); }

void PerformanceStats::endAddVariablesTime() {
  AddVariablesTime += getTimeSpentInSeconds(AddVariablesTimeSt);
}

void PerformanceStats::startConstraintBuilderTime() {
  ConstraintBuilderTimeSt = clock();
}

void PerformanceStats::endConstraintBuilderTime() {
  ConstraintBuilderTime += getTimeSpentInSeconds(ConstraintBuilderTimeSt);
}

void PerformanceStats::startConstraintSolverTime() {
  ConstraintSolverTimeSt = clock();
}

void PerformanceStats::endConstraintSolverTime() {
  ConstraintSolverTime += getTimeSpentInSeconds(ConstraintSolverTimeSt);
}

void PerformanceStats::startArrayBoundsInferenceTime() {
  ArrayBoundsInferenceTimeSt = clock();
}

void PerformanceStats::endArrayBoundsInferenceTime() {
  ArrayBoundsInferenceTime += getTimeSpentInSeconds(ArrayBoundsInferenceTimeSt);
}

void PerformanceStats::startComputeInterimConstraintStateTime() {
  ComputeInterimConstraintStateTimeSt = clock();
}

void PerformanceStats::endComputeInterimConstraintStateTime() {
  ComputeInterimConstraintStateTime += getTimeSpentInSeconds(ComputeInterimConstraintStateTimeSt);
}

void PerformanceStats::startRewritingTime() { RewritingTimeSt = clock(); }

void PerformanceStats::endRewritingTime() {
  RewritingTime += getTimeSpentInSeconds(RewritingTimeSt);
}

void PerformanceStats::startTotalTime() { TotalTimeSt = clock(); }

void PerformanceStats::endTotalTime() {
  TotalTime += getTimeSpentInSeconds(TotalTimeSt);
}

void PerformanceStats::incrementNumAssumeBounds() { NumAssumeBoundsCasts++; }
void PerformanceStats::incrementNumCheckedCasts() { NumCheckedCasts++; }

void PerformanceStats::incrementNumWildCasts() { NumWildCasts++; }

void PerformanceStats::incrementNumFixedCasts() { NumFixedCasts++; }

void PerformanceStats::incrementNumITypes() { NumITypes++; }

void PerformanceStats::incrementNumCheckedRegions() { NumCheckedRegions++; }

void PerformanceStats::incrementNumUnCheckedRegions() { NumUnCheckedRegions++; }

// Print times in a format more readable than llvm::raw_ostream's default
// exponential notation. We could use llvm::write_double, but that would make
// the printPerformanceStats code more cluttered.
static std::string timeToFriendlyString(double Time) {
  char Buf[20];
  snprintf(Buf, sizeof Buf, "%.3f", Time);
  return std::string(Buf);
}

void PerformanceStats::printPerformanceStats(llvm::raw_ostream &O,
                                             bool JsonFormat) {
  if (JsonFormat) {
    O << "[";

    O << "{\"TimeStats\": {\"TotalTime\":" << TotalTime;
    O << ", \"CompileTime\":" << CompileTime;
    O << ", \"FindUsedTagNamesTime\":" << FindUsedTagNamesTime;
    O << ", \"FindMultiDeclsTime\":" << FindMultiDeclsTime;
    O << ", \"AddVariablesTime\":" << AddVariablesTime;
    O << ", \"ConstraintBuilderTime\":" << ConstraintBuilderTime;
    O << ", \"ConstraintSolverTime\":" << ConstraintSolverTime;
    O << ", \"ArrayBoundsInferenceTime\":" << ArrayBoundsInferenceTime;
    O << ", \"ComputeInterimConstraintStateTime\":" << ComputeInterimConstraintStateTime;
    O << ", \"RewritingTime\":" << RewritingTime;
    O << "}},\n";

    O << "{\"ReWriteStats\":{";
    O << "\"NumAssumeBoundsCasts\":" << NumAssumeBoundsCasts;
    O << ", \"NumCheckedCasts\":" << NumCheckedCasts;
    O << ", \"NumWildCasts\":" << NumWildCasts;
    O << ", \"NumFixedCasts\":" << NumFixedCasts;
    O << ", \"NumITypes\":" << NumITypes;
    O << ", \"NumCheckedRegions\":" << NumCheckedRegions;
    O << ", \"NumUnCheckedRegions\":" << NumUnCheckedRegions;
    O << "}}";

    O << "]";
  } else {
    O << "TimeStats\n";
    O << "TotalTime:" << timeToFriendlyString(TotalTime) << "\n";
    O << "CompileTime:" << timeToFriendlyString(CompileTime) << "\n";
    O << "FindUsedTagNamesTime:" << timeToFriendlyString(FindUsedTagNamesTime)
      << "\n";
    O << "FindMultiDeclsTime:" << timeToFriendlyString(FindMultiDeclsTime)
      << "\n";
    O << "AddVariablesTime:" << timeToFriendlyString(AddVariablesTime) << "\n";
    O << "ConstraintBuilderTime:" << timeToFriendlyString(ConstraintBuilderTime)
      << "\n";
    O << "ConstraintSolverTime:" << timeToFriendlyString(ConstraintSolverTime)
      << "\n";
    O << "ArrayBoundsInferenceTime:"
      << timeToFriendlyString(ArrayBoundsInferenceTime) << "\n";
    O << "ComputeInterimConstraintStateTime:"
      << timeToFriendlyString(ComputeInterimConstraintStateTime) << "\n";
    O << "RewritingTime:" << timeToFriendlyString(RewritingTime) << "\n";

    O << "ReWriteStats\n";
    O << "NumAssumeBoundsCasts:" << NumAssumeBoundsCasts << "\n";
    O << "NumCheckedCasts:" << NumCheckedCasts << "\n";
    O << "NumWildCasts:" << NumWildCasts << "\n";
    O << "NumFixedCasts:" << NumFixedCasts << "\n";
    O << "NumITypes:" << NumITypes << "\n";
    O << "NumCheckedRegions:" << NumCheckedRegions << "\n";
    O << "NumUnCheckedRegions:" << NumUnCheckedRegions << "\n";
  }
}

// Record Checked/Unchecked regions.
bool StatsRecorder::VisitCompoundStmt(clang::CompoundStmt *S) {
  auto &PStats = Info->getPerfStats();
  if (S != nullptr) {
    auto PSL = PersistentSourceLoc::mkPSL(S, *Context);
    if (PSL.valid() && canWrite(PSL.getFileName())) {
      switch (S->getWrittenCheckedSpecifier()) {
      case CSS_None:
        // Do nothing
        break;
      case CSS_Unchecked:
        PStats.incrementNumUnCheckedRegions();
        break;
      case CSS_Memory:
      case CSS_Bounds:
        PStats.incrementNumCheckedRegions();
        break;
      }
    }
  }
  return true;
}

// Record itype declarations.
bool StatsRecorder::VisitDecl(clang::Decl *D) {
  auto &PStats = Info->getPerfStats();
  if (D != nullptr) {
    auto PSL = PersistentSourceLoc::mkPSL(D, *Context);
    if (PSL.valid() && canWrite(PSL.getFileName())) {
      if (DeclaratorDecl *DD = dyn_cast<DeclaratorDecl>(D)) {
        if (DD->hasInteropTypeExpr()) {
          PStats.incrementNumITypes();
        }
      }
    }
  }
  return true;
}

// Record checked to wild casts.
bool StatsRecorder::VisitCStyleCastExpr(clang::CStyleCastExpr *C) {
  auto &PStats = Info->getPerfStats();
  if (C != nullptr) {
    auto PSL = PersistentSourceLoc::mkPSL(C, *Context);
    if (PSL.valid() && canWrite(PSL.getFileName())) {
      QualType SrcT = C->getSubExpr()->getType();
      QualType DstT = C->getType();
      if (SrcT->isCheckedPointerType() && !DstT->isCheckedPointerType())
        PStats.incrementNumWildCasts();
    }
  }
  return true;
}

// Record bounds casts.
bool StatsRecorder::VisitBoundsCastExpr(clang::BoundsCastExpr *B) {
  auto &PStats = Info->getPerfStats();
  if (B != nullptr) {
    auto PSL = PersistentSourceLoc::mkPSL(B, *Context);
    if (PSL.valid() && canWrite(PSL.getFileName()))
      PStats.incrementNumAssumeBounds();
  }
  return true;
}

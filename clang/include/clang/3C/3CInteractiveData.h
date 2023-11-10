//=--3CInteractiveData.h------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structures used to communicate results of 3C interactive mode
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_3CINTERACTIVEDATA_H
#define LLVM_CLANG_3C_3CINTERACTIVEDATA_H

#include "clang/3C/ConstraintVariables.h"
#include "clang/3C/PersistentSourceLoc.h"

// Source info and reason
class RootCauseDiagnostic {
public:
  RootCauseDiagnostic() = default;
  explicit RootCauseDiagnostic(ReasonLoc &Rsn) : Main(Rsn) {}

  const std::string &getReason() { return Main.Reason; }
  void setReason(const std::string &Rsn) { Main.Reason = Rsn; }

  const PersistentSourceLoc &getLocation() const { return Main.Location; }

  void addReason(const ReasonLoc &Rsn) {
    Supplemental.push_back(Rsn);
  }

  std::vector<ReasonLoc> &additionalNotes() { return Supplemental; }

private:
  ReasonLoc Main;
  std::vector<ReasonLoc> Supplemental;
};

// Constraints information.
class ConstraintsInfo {
  friend class ProgramInfo;

public:
  ConstraintsInfo() {}
  void clear();
  CVars &getRCVars(ConstraintKey);
  CVars &getSrcCVars(ConstraintKey);
  CVars getWildAffectedCKeys(const std::set<ConstraintKey> &DWKeys);
  void printStats(llvm::raw_ostream &O);
  void printRootCauseStats(raw_ostream &O, Constraints &CS);
  int getNumPtrsAffected(ConstraintKey CK);

  std::map<ConstraintKey, RootCauseDiagnostic> RootWildAtomsWithReason;
  CVars AllWildAtoms;
  CVars InSrcWildAtoms;
  CVars TotalNonDirectWildAtoms;
  CVars InSrcNonDirectWildAtoms;
  std::set<std::string> ValidSourceFiles;
  std::map<ConstraintKey, PersistentSourceLoc> AtomSourceMap;

  void addRootCause(ConstraintKey Var, ConstraintKey RootCause) {
    RootCauses[Var].insert(RootCause);
  }

  CVars& getConstrainedBy(VarAtom *Var) {
    return getConstrainedBy(Var->getLoc());
  }

  CVars& getConstrainedBy(ConstraintKey Var) {
    return ConstrainedBy[Var];
  }

private:
  // Root cause map: This is the map of a Constraint var and a set of
  // Constraint vars (that are directly assigned WILD) which are the reason
  // for making the above constraint var WILD.
  // Example:
  //  WILD
  //  / \
  // p   q
  // \    \
  //  \    r
  //   \  /
  //    s
  // Here: s -> {p, q} and r -> {q}
  // IE: Maps a constraint variables to the set of root causes of wildness
  std::map<ConstraintKey, CVars> RootCauses;
  // This is source map: Map of Constraint var (which are directly
  // assigned WILD) and the set of constraint vars which are WILD because of
  // the above constraint.
  // For the above case, this contains: p -> {s}, q -> {r, s}
  // IE: Maps a root cause to the set of variables it constrains
  std::map<ConstraintKey, CVars> ConstrainedBy;

  // PTR versions of the above maps
  // TODO understand this better
  std::map<ConstraintVariable *, CVars> PtrRootCauses;
  std::map<ConstraintKey, std::set<ConstraintVariable *>> PtrConstrainedBy;

  // Get score for each of the ConstraintKeys, which are wild.
  // For the above example, the score of s would be 0.5, similarly
  // the score of r would be 1
  float getAtomAffectedScore(const CVars &AllKeys);

  float getPtrAffectedScore(const std::set<ConstraintVariable *> CVs);

  void printConstraintStats(raw_ostream &O, Constraints &CS,
                            ConstraintKey Cause);
};

#endif // LLVM_CLANG_3C_3CINTERACTIVEDATA_H

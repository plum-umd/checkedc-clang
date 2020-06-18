//=--Constraints.cpp----------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Constraint solver implementation
//
//===----------------------------------------------------------------------===//

#include <set>
#include "llvm/Support/CommandLine.h"

#include "clang/CConv/CCGlobalOptions.h"
#include "clang/CConv/Constraints.h"
#include "clang/CConv/ConstraintsGraph.h"
#include <iostream>

using namespace llvm;

static cl::OptionCategory SolverCategory("solver options");
static cl::opt<bool> DebugSolver("debug-solver",
  cl::desc("Dump intermediate solver state"),
  cl::init(false), cl::cat(SolverCategory));

Constraint::Constraint(ConstraintKind K, std::string &Rsn,
                       PersistentSourceLoc *PL): Constraint(K, Rsn) {
  if (PL != nullptr && PL->valid()) {
    FileName = PL->getFileName();
    LineNo = PL->getLineNo();
    ColStart = PL->getColNo();
  }
}

// Remove the constraint from the global constraint set.
bool Constraints::removeConstraint(Constraint *C) {
  removeReasonBasedConstraint(C);
  return constraints.erase(C) != 0;
}

// Check if we can add this constraint. This provides a global switch to
// control what constraints we can add to our system.
void Constraints::editConstraintHook(Constraint *C) {
  if (!AllTypes) {
    // Invalidate any pointer-type constraints
    if (Geq *E = dyn_cast<Geq>(C)) {
      if (!E->constraintIsChecked()) {
        VarAtom *LHSA = dyn_cast<VarAtom>(E->getLHS());
        VarAtom *RHSA = dyn_cast<VarAtom>(E->getRHS());
        if (LHSA != nullptr && RHSA != nullptr) {
          return;
        }
        // Make this checked only if the const atom is other than Ptr.
        if (RHSA && !dyn_cast<PtrAtom>(E->getLHS())) {
          E->setChecked(getWild());
        }
      }
    }
  }
}

// Add a constraint to the set of constraints. If the constraint is already 
// present (by syntactic equality) return false. 
bool Constraints::addConstraint(Constraint *C) {
  // Validate the constraint to be added.
  if (!check(C)) {
    C->dump();
    assert(false);
  }

  editConstraintHook(C);

  // Check if C is already in the set of constraints. 
  if (constraints.find(C) == constraints.end()) {
    constraints.insert(C);
    addReasonBasedConstraint(C);

    // Update the variables that depend on this constraint.
    if (Geq *E = dyn_cast<Geq>(C)) {
      if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
        vLHS->Constraints.insert(C);
      else if (VarAtom *vRHS = dyn_cast<VarAtom>(E->getRHS())) {
        vRHS->Constraints.insert(C);
      }
    }
    else if (Implies *I = dyn_cast<Implies>(C)) {
      Geq *E = I->getPremise();
      if (VarAtom *vLHS = dyn_cast<VarAtom>(E->getLHS()))
        vLHS->Constraints.insert(C);
    }
    else
      llvm_unreachable("unsupported constraint");
    return true;
  }

  return false;
}

bool Constraints::addReasonBasedConstraint(Constraint *C) {
  // Only insert if this is an Eq constraint and has a valid reason.
  if (Geq *E = dyn_cast<Geq>(C)) {
      if (E->getReason() != DEFAULT_REASON && !E->getReason().empty())
          return this->constraintsByReason[E->getReason()].insert(E).second;
  }
  return false;
}

bool Constraints::removeReasonBasedConstraint(Constraint *C) {
  if (Geq *E = dyn_cast<Geq>(C)) {
      // Remove if the constraint is present.
      if (this->constraintsByReason.find(E->getReason()) !=
          this->constraintsByReason.end())
          return this->constraintsByReason[E->getReason()].erase(E) > 0;
  }
  return false;
}

// Checks to see if the constraint is of a form that we expect.
// The expected forms are the following:
// EQ : (q_i = q_k)
// GEQ : (q_i >= A) for A constant
// IMPLIES : (q_i >= A) => (q_k >= B) for A,B constant
bool Constraints::check(Constraint *C) {

  if (Implies *I = dyn_cast<Implies>(C)) {
    Geq *P = I->getPremise();
    Geq *CO = I->getConclusion();
    if (!isa<VarAtom>(P->getLHS()) || isa<VarAtom>(P->getRHS()) ||
        !isa<VarAtom>(CO->getLHS()) || isa<VarAtom>(CO->getRHS()))
      return false;
  }
  else if (dyn_cast<Geq>(C) != nullptr) {
      // all good!
  }
  else
    return false; // Not Eq, Geq, or Implies; what is it?!

  return true;
}

// Make a graph G:
//- with nodes for each variable k and each qualifier constant q.
//- with edges Q --> Q’ for each constraint Q <: Q’
// Note: Constraints (q <: k ⇒ q’ <: k’) are not supported, but we shouldn’t
// actually need them. So make your algorithm die if it comes across them.
//
// For each non-constant node k in G,
//- set sol(k) = q_\bot (the least element, i.e., Ptr)
//
// For each constant node q_i, starting with the highest and working down,
//- set worklist W = { q_i }
//- while W nonempty
//-- let Q = take(W)
//-- For all edges (Q --> k) in G
//--- if sol(k) <> (sol(k) JOIN Q) then
//---- set sol(k) := (sol(k) JOIN Q)
//---- for all edges (k --> q) in G, confirm that sol(k) <: q; else fail
//---- add k to W

static bool do_solve(ConstraintsGraph &CG,
                     std::set<Implies *> SavedImplies,
                     ConstraintsEnv & env,
                     Constraints *CS, bool doLeastSolution,
                     std::set<VarAtom *> *InitVs,
                     Constraints::ConstraintSet &Conflicts,
                     EdgeType edgeType) {

  std::vector<Atom *> WorkList;
  std::set<Implies *> FiredImplies;

  // Initialize with seeded VarAtom set (pre-solved)
  if (InitVs != nullptr)
    WorkList.insert(WorkList.begin(), InitVs->begin(), InitVs->end());

  do {
    // Initialize work list with ConstAtoms.
    auto &InitC = CG.getAllConstAtoms();
    WorkList.insert(WorkList.begin(), InitC.begin(), InitC.end());

    while (!WorkList.empty()) {
      auto *Curr = *(WorkList.begin());
      // Remove the first element, get its solution
      WorkList.erase(WorkList.begin());
      ConstAtom *CurrSol = env.getAssignment(Curr);

      // get its neighbors
      std::set<Atom *> Neighbors;
      CG.getNeighbors<VarAtom>(Curr, Neighbors, doLeastSolution, edgeType);
      // update each successor's solution
      for (auto *NeighborA : Neighbors) {
        bool Changed = false;
        /*llvm::errs() << "Neighbor:" << NeighborA->getStr()
                     << " of " << Curr->getStr() << "\n";*/
        if (VarAtom *Neighbor = dyn_cast<VarAtom>(NeighborA)) {
          ConstAtom *NghSol = env.getAssignment(Neighbor);
          // update solution if doing so would change it
          // checked? --- if sol(Neighbor) <> (sol(Neighbor) JOIN Cur)
          //   else   --- if sol(Neighbor) <> (sol(Neighbor) MEET Cur)
          if ((doLeastSolution && *NghSol < *CurrSol) ||
              (!doLeastSolution && *CurrSol < *NghSol)) {
            // ---- set sol(k) := (sol(k) JOIN/MEET Q)
            Changed = env.assign(Neighbor, CurrSol);
            assert (Changed);
            WorkList.push_back(Neighbor);
            /*if (Changed) {
              llvm::s()err << "Trying to assign:" << CurrSol->getStr() << " to "
                           << K->getStr() << "\n";
            }*/
          }
        } // ignore ConstAtoms for now; will confirm solution below
      }
    }
    FiredImplies.clear();

    // If there are some implications that we saved? Propagate them.
    if (!SavedImplies.empty()) {
      // Check if Premise holds. If yes then fire the conclusion.
      for (auto *Imp : SavedImplies) {
        Geq *Pre = Imp->getPremise();
        Geq *Con = Imp->getConclusion();
        ConstAtom *Cca = env.getAssignment(Pre->getRHS());
        ConstAtom *Cva = env.getAssignment(Pre->getLHS());
        // Premise is true, so fire the conclusion.
        if (*Cca < *Cva || *Cca == *Cva) {
          /*llvm::errs() << "Firing Conclusion:";
          Con->print(llvm::errs());
          llvm::errs() << "\n";*/
          CG.addConstraint(Con, *CS);
          // Keep track of fired constraints, so that we can delete them.
          FiredImplies.insert(Imp);
        }
      }
      // Erase all the fired implies.
      for (auto *ToDel : FiredImplies) {
        SavedImplies.erase(ToDel);
      }
    }
    // Lets repeat if there are some fired constraints.
  } while (!FiredImplies.empty());

  // Check Upper/lower bounds hold; collect failures in conflicts set
  std::set<Atom *> Neighbors;
  bool ok = true;
  for (ConstAtom *Cbound : CG.getAllConstAtoms()) {
    if (CG.getNeighbors<VarAtom>(Cbound, Neighbors, !doLeastSolution, edgeType)) {
      for (Atom *A : Neighbors) {
        VarAtom *VA = dyn_cast<VarAtom>(A);
        assert (VA != nullptr && "bogus vertex");
        ConstAtom *Csol = env.getAssignment(VA);
        if ((doLeastSolution && *Cbound < *Csol) ||
            (!doLeastSolution && *Csol < *Cbound)) {
          ok = false;
          // Failed. Make a constraint to represent it
          std::string str;
          llvm::raw_string_ostream os(str);
          os << "Bad solution: "; Csol->print(os);
          os.flush();
          Geq *failedConstraint =
              doLeastSolution ?
              new Geq(VA, Cbound, str, doLeastSolution) :
              new Geq(Cbound, VA, str, doLeastSolution);
          Conflicts.insert(failedConstraint);
          // failure case.
          errs() << "Unsolvable constraints: ";
          VA->print(errs());
          errs() << "=";
          Csol->print(errs());
          errs() << (doLeastSolution? "<" : ">");
          Cbound->print(errs());
          errs() << " var will be made WILD\n";
        }
      }
    }
  }

  return ok;
}

auto isNonParam =
    [](VarAtom *VA) -> bool {
      VarAtom::VarKind VK = VA->getVarKind();
      return VK != VarAtom::V_Param;
    };

auto isNonParamReturn =
    [](VarAtom *VA) -> bool {
      VarAtom::VarKind VK = VA->getVarKind();
      return VK != VarAtom::V_Param && VK != VarAtom::V_Return;
    };

bool Constraints::graph_based_solve(ConstraintSet &Conflicts) {
  ConstraintsGraph CG;
  std::set<Implies *> SavedImplies;
  std::set<Implies *> Empty;
  ConstraintsEnv &env = environment;

  // Checked well-formedness
  environment.checkAssignment(getDefaultSolution());

  // Setup the Checked Constraint Graph.
  for (const auto &C : constraints) {
    if (Geq *G = dyn_cast<Geq>(C)) {
      CG.addConstraint(G, *this);
    }
    // Save the implies to solve them later.
    else if (Implies *Imp = dyn_cast<Implies>(C)) {
      assert(Imp->getConclusion()->constraintIsChecked() &&
             Imp->getPremise()->constraintIsChecked());
      SavedImplies.insert(Imp);
    }
    else
      llvm_unreachable("Bogus constraint type");
  }

  if (DebugSolver) {
    CG.dumpCGDot("initial_constraints_graph.dot");
  }

  // Solve Checked/unchecked constraints first
  env.doCheckedSolve(true);
  bool res = do_solve(CG, SavedImplies, env, this, true, nullptr, Conflicts, Checked);

  // now solve PtrType constraints
  if (res && AllTypes) {
    env.doCheckedSolve(false);

    // Step 1: Greatest solution
    res =
        do_solve(CG, Empty, env, this, false, nullptr, Conflicts, Ptype);

    // Step 2: Reset all solutions but for function params, and compute the least
    if (res) {
      std::set<VarAtom *> rest = env.resetSolution(isNonParam, getNTArr());
      res = do_solve(CG, Empty, env, this, true, &rest, Conflicts, Ptype);

      // Step 3: Reset local variable solutions, compute greatest
      if (res) {
        rest.clear();
        rest = env.resetSolution(isNonParamReturn, getPtr());
        res = do_solve(CG, Empty, env, this, false, &rest,
                       Conflicts, Ptype);
      }
    }
    // If PtrType solving (partly) failed, make the affected VarAtoms wild
    if (!res) {
      std::set<VarAtom *> rest;
      env.doCheckedSolve(true);
      for (const auto &C : Conflicts) {
        if (Geq *geq = dyn_cast<Geq>(C)) {
          VarAtom *VA = dyn_cast<VarAtom>(geq->getLHS());
          if (!VA)
            VA = dyn_cast<VarAtom>(geq->getRHS());
          assert(VA != nullptr);
          env.assign(VA, getWild());
          rest.insert(VA);
        }
      }
      Conflicts.clear();
      /* FIXME: Should we propagate the old res? */
      res = do_solve(CG, SavedImplies, env, this, true, &rest,
                     Conflicts, Checked);

    }
    // Final Step: Merge ptyp solution with checked solution
    env.mergePtrTypes();
  }

  if (DebugSolver) {
    CG.dumpCGDot("implication_constraints_graph.dot");
  }

  return res;
}

// Solve the system of constraints. Return true in the second position if
// the system is solved. If the system is solved, the first position is
// an empty. If the system could not be solved, the constraints in conflict
// are returned in the first position.
std::pair<Constraints::ConstraintSet, bool> Constraints::solve() {

  Constraints::ConstraintSet Conflicts;

  if (DebugSolver) {
    errs() << "constraints beginning solve\n";
    dump();
  }
  bool ok = graph_based_solve(Conflicts);

  if (DebugSolver) {
    errs() << "solution, when done solving\n";
    environment.dump();
  }

  return std::pair<Constraints::ConstraintSet, bool>(Conflicts, ok);
}

void Constraints::print(raw_ostream &O) const {
  O << "CONSTRAINTS: \n";
  for (const auto &C : constraints) {
    C->print(O);
    O << "\n";
  }
  environment.print(O);
}

void Constraints::dump(void) const {
  print(errs());
}

void Constraints::dump_json(llvm::raw_ostream &O) const {
  O << "{\"Constraints\":[";
  bool addComma = false;
  for (const auto &C : constraints) {
    if (addComma) {
      O << ",\n";
    }
    C->dump_json(O);
    addComma = true;
  }
  O << "],\n";

  environment.dump_json(O);
}

bool
Constraints::removeAllConstraintsOnReason(std::string &Reason,
                                          ConstraintSet &RemovedCons) {
  // Are there any constraints with this reason?
  bool Removed = false;
  if (this->constraintsByReason.find(Reason) !=
      this->constraintsByReason.end()) {
    RemovedCons.insert(this->constraintsByReason[Reason].begin(),
                  this->constraintsByReason[Reason].end());
    for (auto cToDel : RemovedCons) {
      Removed = this->removeConstraint(cToDel) || Removed;
    }
    return Removed;
  }
  return Removed;
}

VarAtom *Constraints::getOrCreateVar(ConstraintKey V, std::string Name,
                                     VarAtom::VarKind VK) {
  return environment.getOrCreateVar(V, getDefaultSolution(), Name, VK);
}

VarSolTy Constraints::getDefaultSolution() {
  return std::make_pair(getPtr(), getPtr());
}

VarAtom *Constraints::getFreshVar(std::string Name, VarAtom::VarKind VK) {
  return environment.getFreshVar(getDefaultSolution(), Name, VK);
}

VarAtom *Constraints::getVar(ConstraintKey V) const {
  return environment.getVar(V);
}

PtrAtom *Constraints::getPtr() const {
  return PrebuiltPtr;
}
ArrAtom *Constraints::getArr() const {
  return PrebuiltArr;
}
NTArrAtom *Constraints::getNTArr() const {
  return PrebuiltNTArr;
}
WildAtom *Constraints::getWild() const {
  return PrebuiltWild;
}

ConstAtom *Constraints::getAssignment(Atom *A) {
  environment.doCheckedSolve(true);
  return environment.getAssignment(A);
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, bool isCheckedConstraint) {
    return new Geq(Lhs, Rhs, isCheckedConstraint);
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, std::string &Rsn,
                            bool isCheckedConstraint) {
    return new Geq(Lhs, Rhs, Rsn, isCheckedConstraint);
}

Geq *Constraints::createGeq(Atom *Lhs, Atom *Rhs, std::string &Rsn,
                            PersistentSourceLoc *PL, bool isCheckedConstraint) {
    if (PL != nullptr && PL->valid()) {
        // Make this invalid, if the source location is not absolute path
        // this is to avoid crashes in clangd.
        if (PL->getFileName().c_str()[0] != '/')
            PL = nullptr;
    }
    return new Geq(Lhs, Rhs, Rsn, PL, isCheckedConstraint);
}

Implies *Constraints::createImplies(Geq *Premise,
                                    Geq *Conclusion) {
  return new Implies(Premise, Conclusion);
}

void Constraints::resetEnvironment() {
  environment.resetFullSolution(getDefaultSolution());
}

bool Constraints::checkInitialEnvSanity() {
  return environment.checkAssignment(getDefaultSolution());
}

Constraints::Constraints() {
  PrebuiltPtr = new PtrAtom();
  PrebuiltArr = new ArrAtom();
  PrebuiltNTArr = new NTArrAtom();
  PrebuiltWild = new WildAtom();
}

Constraints::~Constraints() {
  delete PrebuiltPtr;
  delete PrebuiltArr;
  delete PrebuiltNTArr;
  delete PrebuiltWild;
}

/* ConstraintsEnv methods */

void ConstraintsEnv::dump(void) const {
  print(errs());
}

void ConstraintsEnv::print(raw_ostream &O) const {
  O << "ENVIRONMENT: \n";
  for (const auto &V : environment) {
    V.first->print(O);
    O << " = [";
    O << "Checked=";
    V.second.first->print(O);
    O << ", PtrType=";
    V.second.second->print(O);
    O << "]";
    O << "\n";
  }
}

void ConstraintsEnv::dump_json(llvm::raw_ostream &O) const {
  bool addComma = false;
  O << "\"Environment\":[";
  for (const auto &V : environment) {
    if (addComma) {
      O << ",\n";
    }
    O << "{\"var\":";
    V.first->dump_json(O);
    O << ", \"value:\":{\"checked\":";
    V.second.first->dump_json(O);
    O << ", \"PtrType\":";
    V.second.second->dump_json(O);
    O << "}}";
    addComma = true;
  }
  O << "]}";
}

VarAtom *ConstraintsEnv::getFreshVar(VarSolTy InitC, std::string Name,
                                     VarAtom::VarKind VK) {
  VarAtom *NewVA = getOrCreateVar(consFreeKey, InitC, Name, VK);
  consFreeKey++;
  return NewVA;
}

VarAtom *ConstraintsEnv::getOrCreateVar(ConstraintKey V, VarSolTy InitC,
                                        std::string Name, VarAtom::VarKind VK) {
  VarAtom Tv(V,Name,VK);
  EnvironmentMap::iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else {
    VarAtom *V = new VarAtom(Tv);
    environment[V] = InitC;
    return V;
  }
}

VarAtom *ConstraintsEnv::getVar(ConstraintKey V) const {
  VarAtom Tv(V);
  EnvironmentMap::const_iterator I = environment.find(&Tv);

  if (I != environment.end())
    return I->first;
  else
    return nullptr;
}


ConstAtom *ConstraintsEnv::getAssignment(Atom *A) {
  if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
    if (useChecked) {
      return environment[VA].first;
    } else {
      return environment[VA].second;
    }
  }
  assert(dyn_cast<ConstAtom>(A) != nullptr &&
         "This is not a VarAtom or ConstAtom");
  return dyn_cast<ConstAtom>(A);
}

bool ConstraintsEnv::checkAssignment(VarSolTy sol) {
  for (const auto &EnvVar : environment) {
    if (EnvVar.second.first != sol.first || EnvVar.second.second != sol.second) {
      return false;
    }
  }
  return true;
}

bool ConstraintsEnv::assign(VarAtom *V, ConstAtom *C) {
  auto VI = environment.find(V);
  if (useChecked) {
    VI->second.first = C;
  } else {
    VI->second.second = C;
  }
  return true;
}

// Reset solution of all VarAtoms that satisfy the given predicate
//  to be the given ConstAtom
std::set<VarAtom *> ConstraintsEnv::resetSolution(VarAtomPred Pred, ConstAtom *C) {
  std::set<VarAtom *> Unchanged;
  for (auto &CurrE : environment) {
    VarAtom *VA = CurrE.first;
    if (Pred(VA)) {
      if (useChecked) {
        CurrE.second.first = C;
      } else {
        CurrE.second.second = C;
      }
    } else {
      Unchanged.insert(VA);
    }
  }
  return Unchanged;
}

void ConstraintsEnv::resetFullSolution(VarSolTy InitC) {
  for (auto &CurrE : environment) {
    CurrE.second = InitC;
  }
}

// Copy solutions from the ptyp map into the checked one
//   if the checked solution is non-WILD
void ConstraintsEnv::mergePtrTypes() {
  useChecked = true;
  for (auto &Elem : environment) {
    VarAtom *VA = dyn_cast<VarAtom>(Elem.first);
    ConstAtom *CAssign = Elem.second.first;
    if (dyn_cast<WildAtom>(CAssign) == nullptr) {
      ConstAtom *OAssign = Elem.second.second;
      assert(dyn_cast<WildAtom>(OAssign) == nullptr &&
          "Expected a checked pointer type.");
      assign(VA, OAssign);
    }
  }
}
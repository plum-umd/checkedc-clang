//=--ConstraintVariables.h----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// The class allocates constraint variables and maps program locations
// (specified by PersistentSourceLocs) to constraint variables.
//
// The allocation of constraint variables is a little nuanced. For a given
// variable, there might be multiple constraint variables. For example, some
// declaration of the form:
//
//  int **p = ... ;
//
// would be given two constraint variables, visualized like this:
//
//  int * q_(i+1) * q_i p = ... ;
//
// The constraint variable at the "highest" or outer-most level of the type
// is the lowest numbered constraint variable for a given declaration.
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINTVARIABLES_H
#define _CONSTRAINTVARIABLES_H

#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include "clang/AST/ASTContext.h"

#include "Constraints.h"

using namespace clang;

class ProgramInfo;

// Holds integers representing constraint variables, with semantics as
// defined in the text above
typedef std::set<ConstraintKey> CVars;
// Holds Atoms, one for each of the pointer (*) declared in the program.
typedef std::vector<Atom*> CAtoms;

// Base class for ConstraintVariables. A ConstraintVariable can either be a
// PointerVariableConstraint or a FunctionVariableConstraint. The difference
// is that FunctionVariableConstraints have constraints on the return value
// and on each parameter.
class ConstraintVariable {
public:
  enum ConstraintVariableKind {
      PointerVariable,
      FunctionVariable
  };

  ConstraintVariableKind getKind() const { return Kind; }

private:
  ConstraintVariableKind Kind;
protected:
  std::string OriginalType;
  // Underlying name of the C variable this ConstraintVariable represents.
  std::string Name;
  // Set of constraint variables that have been constrained due to a
  // bounds-safe interface (itype). They are remembered as being constrained
  // so that later on we do not introduce a spurious constraint
  // making those variables WILD.
  std::set<ConstraintKey> ConstrainedVars;
  // A flag to indicate that we already forced argConstraints to be equated
  // Avoids infinite recursive calls.
  bool HasEqArgumentConstraints;

  // Only subclasses should call this
  ConstraintVariable(ConstraintVariableKind K, std::string T, std::string N) :
      Kind(K),OriginalType(T),Name(N), HasEqArgumentConstraints(false) {}

public:
  // Create a "for-rewriting" representation of this ConstraintVariable.
  // The 'emitName' parameter is true when the generated string should include
  // the name of the variable, false for just the type.
  // The 'forIType' parameter is true when the generated string is expected
  // to be used inside an itype
  virtual std::string mkString(EnvironmentMap &E,
                               bool emitName=true, bool forItype=false) = 0;

  // Debug printing of the constraint variable.
  virtual void print(llvm::raw_ostream &O) const = 0;
  virtual void dump() const = 0;
  virtual void dump_json(llvm::raw_ostream &O) const = 0;

  virtual bool hasItype() = 0;

  virtual bool solutionEqualTo(Constraints &, ConstraintVariable *) = 0;

  // Constrain all pointers in this ConstraintVariable to be Wild.
  virtual void constrainToWild(Constraints &CS) = 0;
  virtual void constrainToWild(Constraints &CS, std::string &Rsn) = 0;
  virtual void constrainToWild(Constraints &CS, std::string &Rsn,
                               PersistentSourceLoc *PL) = 0;

  // Returns true if any of the constraint variables 'within' this instance
  // have a binding in E other than top. E should be the EnvironmentMap that
  // results from running unification on the set of constraints and the
  // environment.
  virtual bool anyChanges(EnvironmentMap &E) = 0;
  virtual bool hasWild(EnvironmentMap &E) = 0;
  virtual bool hasArr(EnvironmentMap &E) = 0;
  virtual bool hasNtArr(EnvironmentMap &E) = 0;

  // Force use of equality constraints in function calls for this CV
  virtual void equateArgumentConstraints(ProgramInfo &I) = 0;

  // Update this CV with information from duplicate declaration CVs
  virtual void brainTransplant(ConstraintVariable *) = 0;
  virtual void mergeDeclaration(ConstraintVariable *) = 0;

  std::string getOriginalTy() { return OriginalType; }
  // Get the original type string that can be directly
  // used for rewriting.
  std::string getRewritableOriginalTy();
  std::string getName() const { return Name; }

  virtual ConstraintVariable *getCopy(Constraints &CS) = 0;

  virtual ~ConstraintVariable() {};

  // Sometimes, constraint variables can be produced that are empty. This
  // tests for the existence of those constraint variables.
  virtual bool isEmpty(void) const = 0;
};

enum ConsAction {
  Safe_to_Wild,
  Wild_to_Safe,
  Same_to_Same
};

void constrainConsVarGeq(std::set<ConstraintVariable *> &LHS,
                         std::set<ConstraintVariable *> &RHS,
                         Constraints &CS,
                         PersistentSourceLoc *PL,
                         ConsAction CA,
                         bool doEqType,
                         ProgramInfo *Info);
void constrainConsVarGeq(ConstraintVariable *LHS, ConstraintVariable *RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool doEqType, ProgramInfo *Info);

// True if [C] is a PVConstraint that contains at least one Atom (i.e.,
//   it represents a C pointer)
bool isAValidPVConstraint(ConstraintVariable *C);

class PointerVariableConstraint;
class FunctionVariableConstraint;

// Represents an individual constraint on a pointer variable.
// This could contain a reference to a FunctionVariableConstraint
// in the case of a function pointer declaration.
class PointerVariableConstraint : public ConstraintVariable {
public:
  enum Qualification {
      ConstQualification,
      StaticQualification
  };

  static PointerVariableConstraint *getWildPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getPtrPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getNonPtrPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getNamedNonPtrPVConstraint(StringRef name, Constraints &CS);

private:
  std::string BaseType;
  CAtoms vars;
  FunctionVariableConstraint *FV;
  std::map<uint32_t, Qualification> QualMap;
  enum OriginalArrType {
      O_Pointer,
      O_SizedArray,
      O_UnSizedArray
  };
  // Map from pointer idx to original type and size.
  // If the original variable U was:
  //  * A pointer, then U -> (a,b) , a = O_Pointer, b has no meaning.
  //  * A sized array, then U -> (a,b) , a = O_SizedArray, b is static size.
  //  * An unsized array, then U -(a,b) , a = O_UnSizedArray, b has no meaning.
  std::map<uint32_t ,std::pair<OriginalArrType,uint64_t>> arrSizes;
  // If for all U in arrSizes, any U -> (a,b) where a = O_SizedArray or
  // O_UnSizedArray, arrPresent is true.
  bool ArrPresent;
  // Is there an itype associated with this constraint? If there is, how was it
  // originally stored in the program?
  std::string ItypeStr;
  // Get the qualifier string (e.g., const, etc) for the provided
  // pointer type into the provided string stream (ss).
  void getQualString(uint32_t TypeIdx, std::ostringstream &Ss);
  // This function tries to emit an array size for the variable.
  // and returns true if the variable is an array and a size is emitted.
  bool emitArraySize(std::ostringstream &Pss, uint32_t TypeIdx, bool &EmitName,
                     bool &EmittedCheckedAnnotation, bool Nt);
  // Flag to indicate that this constraint is a part of function prototype
  // e.g., Parameters or Return.
  bool partOFFuncPrototype;
  // For the function parameters and returns,
  // this set contains the constraint variable of
  // the values used as arguments.
  std::set<ConstraintVariable *> argumentConstraints;
  // Get solution for the atom of a pointer.
  const ConstAtom *getSolution(const Atom *A,
                               EnvironmentMap &E) const;

  PointerVariableConstraint(PointerVariableConstraint *Ot,
                            Constraints &CS);
  PointerVariableConstraint *Parent;

public:
  // Constructor for when we know a CVars and a type string.
  PointerVariableConstraint(CAtoms V, std::string T, std::string Name,
                            FunctionVariableConstraint *F, bool isArr,
                            bool isItype, std::string is) :
          ConstraintVariable(PointerVariable, "" /*not used*/, Name),
          BaseType(T),vars(V),FV(F),
        ArrPresent(isArr), ItypeStr(is),
           partOFFuncPrototype(false), Parent(nullptr) {}

  std::string getTy() { return BaseType; }
  bool getArrPresent() { return ArrPresent; }

  // Is an itype present for this constraint? If yes,
  // what is the text of that itype?
  bool hasItype() { return ItypeStr.size() > 0; }
  std::string getItype() { return ItypeStr; }

  bool solutionEqualTo(Constraints &CS, ConstraintVariable *CV);

  // Constructor for when we have a Decl. K is the current free
  // constraint variable index. We don't need to explicitly pass
  // the name because it's available in 'D'.
  PointerVariableConstraint(clang::DeclaratorDecl *D,
                            Constraints &CS, const clang::ASTContext &C);

  // Constructor for when we only have a Type. Needs a string name
  // N for the name of the variable that this represents.
  PointerVariableConstraint(const clang::QualType &QT,
                            clang::DeclaratorDecl *D, std::string N,
                            Constraints &CS,
                            const clang::ASTContext &C,
                            std::string *inFunc = nullptr);

  const CAtoms &getCvars() const { return vars; }

  void brainTransplant(ConstraintVariable *From);
  void mergeDeclaration(ConstraintVariable *From);

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == PointerVariable;
  }

  std::string mkString(EnvironmentMap &E, bool EmitName =true,
                       bool ForItype =false);

  FunctionVariableConstraint *getFV() { return FV; }

  void print(llvm::raw_ostream &O) const ;
  void dump() const { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const;
  void constrainToWild(Constraints &CS);
  void constrainToWild(Constraints &CS, std::string &Rsn);
  void constrainToWild(Constraints &CS, std::string &Rsn,
                       PersistentSourceLoc *PL);
  void constrainOuterTo(Constraints &CS, ConstAtom *C);
  bool anyChanges(EnvironmentMap &E);
  bool anyArgumentIsWild(EnvironmentMap &E);
  bool hasWild(EnvironmentMap &E);
  bool hasArr(EnvironmentMap &E);
  bool hasNtArr(EnvironmentMap &E);

  void equateArgumentConstraints(ProgramInfo &I);

  bool isPartOfFunctionPrototype() const  { return partOFFuncPrototype; }
  // Add the provided constraint variable as an argument constraint.
  bool addArgumentConstraint(ConstraintVariable *DstCons, ProgramInfo &Info);
  // Get the set of constraint variables corresponding to the arguments.
  std::set<ConstraintVariable *> &getArgumentConstraints();

  bool isEmpty(void) const { return vars.size() == 0; }

  ConstraintVariable *getCopy(Constraints &CS);

  virtual ~PointerVariableConstraint() {};
};

typedef PointerVariableConstraint PVConstraint;
// Name for function return, for debugging
#define RETVAR "$ret"

// Constraints on a function type. Also contains a 'name' parameter for
// when a re-write of a function pointer is needed.
class FunctionVariableConstraint : public ConstraintVariable {
private:
  FunctionVariableConstraint(FunctionVariableConstraint *Ot,
                             Constraints &CS);
  // N constraints on the return value of the function.
  std::set<ConstraintVariable *> returnVars;
  // A vector of K sets of N constraints on the parameter values, for
  // K parameters accepted by the function.
  std::vector<std::set<ConstraintVariable *>> paramVars;
  // File name in which this declaration is found.
  std::string FileName;
  bool Hasproto;
  bool Hasbody;
  bool IsStatic;
  FunctionVariableConstraint *Parent;
  // Flag to indicate whether this is a function pointer or not.
  bool IsFunctionPtr;

  void equateFVConstraintVars(std::set<ConstraintVariable *> &Cset,
                              ProgramInfo &Info);
public:
  FunctionVariableConstraint() :
          ConstraintVariable(FunctionVariable, "", ""),
                                 FileName(""), Hasproto(false),
        Hasbody(false), IsStatic(false), Parent(nullptr),
                                 IsFunctionPtr(false) { }

  FunctionVariableConstraint(clang::DeclaratorDecl *D,
                             Constraints &CS, const clang::ASTContext &C);
  FunctionVariableConstraint(const clang::Type *Ty,
                             clang::DeclaratorDecl *D, std::string N,
                             Constraints &CS, const clang::ASTContext &C);

  std::set<ConstraintVariable *> &
  getReturnVars() { return returnVars; }

  size_t numParams() { return paramVars.size(); }

  bool hasProtoType() { return Hasproto; }
  bool hasBody() { return Hasbody; }
  void setHasBody(bool hbody) { this->Hasbody = hbody; }

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == FunctionVariable;
  }

  void brainTransplant(ConstraintVariable *From);
  void mergeDeclaration(ConstraintVariable *FromCV);

  std::set<ConstraintVariable *> &
  getParamVar(unsigned i) {
    assert(i < paramVars.size());
    return paramVars.at(i);
  }

  bool hasItype();
  bool solutionEqualTo(Constraints &CS, ConstraintVariable *CV);

  std::string mkString(EnvironmentMap &E, bool EmitName =true,
                       bool ForItype =false);
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dump_json(llvm::raw_ostream &O) const;
  void constrainToWild(Constraints &CS);
  void constrainToWild(Constraints &CS, std::string &Rsn);
  void constrainToWild(Constraints &CS, std::string &Rsn,
                       PersistentSourceLoc *PL);
  bool anyChanges(EnvironmentMap &E);
  bool hasWild(EnvironmentMap &E);
  bool hasArr(EnvironmentMap &E);
  bool hasNtArr(EnvironmentMap &E);

  void equateArgumentConstraints(ProgramInfo &P);

  ConstraintVariable *getCopy(Constraints &CS);

  // An FVConstraint is empty if every constraint associated is empty.
  bool isEmpty(void) const {

    if (returnVars.size() > 0)
      return false;

    for (const auto &u : paramVars)
      for (const auto &v : u)
        if (!v->isEmpty())
          return false;

    return true;
  }

  virtual ~FunctionVariableConstraint() {};
};

typedef FunctionVariableConstraint FVConstraint;

#endif //_CONSTRAINTVARIABLES_H

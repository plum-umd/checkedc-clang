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

#ifndef LLVM_CLANG_3C_CONSTRAINTVARIABLES_H
#define LLVM_CLANG_3C_CONSTRAINTVARIABLES_H

#include "clang/3C/Constraints.h"
#include "clang/3C/ProgramVar.h"
#include "clang/AST/ASTContext.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringSwitch.h"

using namespace clang;

class ProgramInfo;

// Holds integers representing constraint variables, with semantics as
// defined in the text above
typedef std::set<ConstraintKey> CVars;
// Holds Atoms, one for each of the pointer (*) declared in the program.
typedef std::vector<Atom *> CAtoms;

// Base class for ConstraintVariables. A ConstraintVariable can either be a
// PointerVariableConstraint or a FunctionVariableConstraint. The difference
// is that FunctionVariableConstraints have constraints on the return value
// and on each parameter.
class ConstraintVariable {
public:
  enum ConstraintVariableKind { PointerVariable, FunctionVariable };

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
  // Flag to indicate if this Constraint Variable has a bounds key.
  bool ValidBoundsKey;
  // Bounds key of this Constraint Variable.
  BoundsKey BKey;
  // Is this Constraint Variable for a declaration?
  bool IsForDecl;

  // Only subclasses should call this
  ConstraintVariable(ConstraintVariableKind K, std::string T, std::string N)
      : Kind(K), OriginalType(T), Name(N), HasEqArgumentConstraints(false),
        ValidBoundsKey(false), IsForDecl(false) {}

public:
  // Create a "for-rewriting" representation of this ConstraintVariable.
  // The 'emitName' parameter is true when the generated string should include
  // the name of the variable, false for just the type.
  // The 'forIType' parameter is true when the generated string is expected
  // to be used inside an itype.
  virtual std::string mkString(const EnvironmentMap &E, bool EmitName = true,
                               bool ForItype = false, bool EmitPointee = false,
                               bool UnmaskTypedef = false) const = 0;

  // Debug printing of the constraint variable.
  virtual void print(llvm::raw_ostream &O) const = 0;
  virtual void dump() const = 0;
  virtual void dumpJson(llvm::raw_ostream &O) const = 0;

  virtual bool srcHasItype() const = 0;
  virtual bool srcHasBounds() const = 0;

  bool hasBoundsKey() const { return ValidBoundsKey; }
  BoundsKey getBoundsKey() const {
    assert(ValidBoundsKey && "No valid Bkey");
    return BKey;
  }
  void setBoundsKey(BoundsKey NK) {
    ValidBoundsKey = true;
    BKey = NK;
  }

  virtual bool solutionEqualTo(Constraints &, const ConstraintVariable *,
                               bool ComparePtyp = true) const = 0;

  virtual void constrainToWild(Constraints &CS,
                               const std::string &Rsn) const = 0;
  virtual void constrainToWild(Constraints &CS, const std::string &Rsn,
                               PersistentSourceLoc *PL) const = 0;

  // Returns true if any of the constraint variables 'within' this instance
  // have a binding in E other than top. E should be the EnvironmentMap that
  // results from running unification on the set of constraints and the
  // environment.
  bool isChecked(const EnvironmentMap &E) const;

  // Returns true if this constraint variable has a different checked type after
  // running unification. Note that if the constraint variable had a checked
  // type in the input program, it will have the same checked type after solving
  // so, the type will not have changed. To test if the type is checked, use
  // isChecked instead.
  virtual bool anyChanges(const EnvironmentMap &E) const = 0;

  // Here, AIdx is the pointer level which needs to be checked.
  // By default, we check for all pointer levels (or VarAtoms)
  virtual bool hasWild(const EnvironmentMap &E, int AIdx = -1) const = 0;
  virtual bool hasArr(const EnvironmentMap &E, int AIdx = -1) const = 0;
  virtual bool hasNtArr(const EnvironmentMap &E, int AIdx = -1) const = 0;

  // Force use of equality constraints in function calls for this CV
  virtual void equateArgumentConstraints(ProgramInfo &I) = 0;

  // Internally combine the constraints and other data from the first parameter
  // with this constraint variable. Used with redeclarations, especially of
  // functions declared in multiple files.
  virtual void mergeDeclaration(ConstraintVariable *, ProgramInfo &,
                                std::string &ReasonFailed) = 0;

  std::string getOriginalTy() const { return OriginalType; }
  // Get the original type string that can be directly
  // used for rewriting.
  std::string getRewritableOriginalTy() const;
  std::string getName() const { return Name; }

  void setValidDecl() { IsForDecl = true; }
  bool isForValidDecl() const { return IsForDecl; }

  virtual ConstraintVariable *getCopy(Constraints &CS) = 0;

  virtual ~ConstraintVariable(){};

  virtual bool getIsOriginallyChecked() const = 0;
};

typedef std::set<ConstraintVariable *> CVarSet;
typedef Option<ConstraintVariable> CVarOption;

enum ConsAction { Safe_to_Wild, Wild_to_Safe, Same_to_Same };

void constrainConsVarGeq(const std::set<ConstraintVariable *> &LHS,
                         const std::set<ConstraintVariable *> &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey = true);
void constrainConsVarGeq(ConstraintVariable *LHS, const CVarSet &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey = true);
void constrainConsVarGeq(ConstraintVariable *LHS, ConstraintVariable *RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey = true);

// True if [C] is a PVConstraint that contains at least one Atom (i.e.,
//   it represents a C pointer)
bool isAValidPVConstraint(const ConstraintVariable *C);

class PointerVariableConstraint;
class FunctionVariableConstraint;

// We need to store the level inside the type AST at which the first
// typedef occurs. This allows us to stop rewriting once we hit the
// first typedef. (All subsequent typedefs will not be rewritten, as
// rewriting will stop.)
struct InternalTypedefInfo {
  bool HasTypedef;
  int TypedefLevel;
  std::string TypedefName;
};

// Represents an individual constraint on a pointer variable.
// This could contain a reference to a FunctionVariableConstraint
// in the case of a function pointer declaration.
class PointerVariableConstraint : public ConstraintVariable {
public:
  enum Qualification {
    ConstQualification,
    VolatileQualification,
    RestrictQualification
  };

  static PointerVariableConstraint *
  getWildPVConstraint(Constraints &CS, const std::string &Rsn,
                      PersistentSourceLoc *PSL = nullptr);
  static PointerVariableConstraint *getPtrPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getNonPtrPVConstraint(Constraints &CS);
  static PointerVariableConstraint *getNamedNonPtrPVConstraint(StringRef Name,
                                                               Constraints &CS);

private:
  std::string BaseType;
  CAtoms Vars;
  FunctionVariableConstraint *FV;
  std::map<uint32_t, std::set<Qualification>> QualMap;
  enum OriginalArrType { O_Pointer, O_SizedArray, O_UnSizedArray };
  // Map from pointer idx to original type and size.
  // If the original variable U was:
  //  * A pointer, then U -> (a,b) , a = O_Pointer, b has no meaning.
  //  * A sized array, then U -> (a,b) , a = O_SizedArray, b is static size.
  //  * An unsized array, then U -(a,b) , a = O_UnSizedArray, b has no meaning.
  std::map<uint32_t, std::pair<OriginalArrType, uint64_t>> ArrSizes;
  // If for all U in arrSizes, any U -> (a,b) where a = O_SizedArray or
  // O_UnSizedArray, arrPresent is true.
  bool ArrPresent;

  // True if this variable has an itype in the original source code.
  bool SrcHasItype;
  // The string representation of the itype of in the original source. This
  // string is empty if the variable did not have an itype OR if the itype was
  // implicitly declared by a bounds declaration on an unchecked pointer.
  std::string ItypeStr;

  // Get the qualifier string (e.g., const, etc) for the provided
  // pointer type into the provided string stream (ss).
  void getQualString(uint32_t TypeIdx, std::ostringstream &Ss) const;
  void insertQualType(uint32_t TypeIdx, QualType &QTy);
  // This function tries to emit an array size for the variable.
  // and returns true if the variable is an array and a size is emitted.
  bool emitArraySize(std::stack<std::string> &CheckedArrs, uint32_t TypeIdx,
                     bool &AllArray, bool &ArrayRun, bool Nt) const;
  void addArrayAnnotations(std::stack<std::string> &CheckedArrs,
                           std::deque<std::string> &EndStrs) const;

  // Utility used by the constructor to obtain a string representation of a
  // declaration's base type. To preserve macros, this we first try to take
  // the type directly from source code. Where that is not possible, the type
  // is regenerated from the type in the clang AST.
  static std::string extractBaseType(DeclaratorDecl *D, QualType QT,
                                     const Type *Ty, const ASTContext &C);

  // Try to extract string representation of the base type for a declaration
  // from the source code. If the base type cannot be extracted from source, an
  // empty string is returned instead.
  static std::string tryExtractBaseType(DeclaratorDecl *D, QualType QT,
                                        const Type *Ty, const ASTContext &C);

  // Flag to indicate that this constraint is a part of function prototype
  // e.g., Parameters or Return.
  bool PartOfFuncPrototype;
  // For the function parameters and returns,
  // this set contains the constraint variable of
  // the values used as arguments.
  std::set<ConstraintVariable *> ArgumentConstraints;
  // Get solution for the atom of a pointer.
  const ConstAtom *getSolution(const Atom *A, const EnvironmentMap &E) const;

  PointerVariableConstraint(PointerVariableConstraint *Ot, Constraints &CS);
  PointerVariableConstraint *Parent;
  // String representing declared bounds expression.
  std::string BoundsAnnotationStr;

  // Does this variable represent a generic type? Which one (or -1 for none)?
  // Generic types can be used with fewer restrictions, so this field is used
  // stop assignments with generic variables from forcing constraint variables
  // to be wild.
  int GenericIndex;

  // Empty array pointers are represented the same as standard pointers. This
  // lets pointers be passed to functions expecting a zero width array. This
  // flag is used to discriminate between standard pointer and zero width array
  // pointers.
  bool IsZeroWidthArray;

  bool IsTypedef = false;
  TypedefNameDecl *TDT;
  std::string TypedefString;
  // Does the type internally contain a typedef, and if so: at what level and
  // what is it's name?
  struct InternalTypedefInfo TypedefLevelInfo;

  // Is this a pointer to void? Possibly with multiple levels of indirection.
  bool IsVoidPtr;

public:
  // Constructor for when we know a CVars and a type string.
  PointerVariableConstraint(CAtoms V, std::string T, std::string Name,
                            FunctionVariableConstraint *F, bool IsArr,
                            std::string Is, int Generic = -1)
      : ConstraintVariable(PointerVariable, "" /*not used*/, Name), BaseType(T),
        Vars(V), FV(F), ArrPresent(IsArr), SrcHasItype(!Is.empty()),
        ItypeStr(Is), PartOfFuncPrototype(false), Parent(nullptr),
        BoundsAnnotationStr(""), GenericIndex(Generic), IsZeroWidthArray(false),
        IsVoidPtr(false) {}

  std::string getTy() const { return BaseType; }
  bool getArrPresent() const { return ArrPresent; }
  // Check if the outermost pointer is an unsized array.
  bool isTopCvarUnsizedArr() const;
  // Check if any of the pointers is either a sized or unsized arr.
  bool hasSomeSizedArr() const;

  bool isTypedef(void);
  void setTypedef(TypedefNameDecl *T, std::string S);

  // Return true if this constraint had an itype in the original source code.
  bool srcHasItype() const override {
    assert(!SrcHasItype || !ItypeStr.empty() || !BoundsAnnotationStr.empty());
    return SrcHasItype;
  }

  // Return the string representation of the itype for this constraint if an
  // itype was present in the original source code. Returns empty string
  // otherwise.
  std::string getItype() const { return ItypeStr; }
  // Check if this variable has bounds annotation.
  bool srcHasBounds() const override { return !BoundsAnnotationStr.empty(); }
  // Get bounds annotation.
  std::string getBoundsStr() const { return BoundsAnnotationStr; }

  bool getIsGeneric() const { return GenericIndex >= 0; }
  int getGenericIndex() const { return GenericIndex; }

  // Was this variable a checked pointer in the input program?
  // This is important for two reasons: (1) externs that are checked should be
  // kept that way during solving, (2) nothing that was originally checked
  // should be modified during rewriting.
  bool getIsOriginallyChecked() const override {
    return llvm::any_of(Vars, [](Atom *A) { return isa<ConstAtom>(A); });
  }

  bool isVoidPtr() const { return IsVoidPtr; }

  bool solutionEqualTo(Constraints &CS, const ConstraintVariable *CV,
                       bool ComparePtyp = true) const override;

  // Construct a PVConstraint when the variable is generated by a specific
  // declaration (D). This constructor calls the next constructor below with
  // QT and N instantiated with information in D.
  PointerVariableConstraint(clang::DeclaratorDecl *D, ProgramInfo &I,
                            const clang::ASTContext &C);

  // QT: Defines the type for the constraint variable. One atom is added for
  //     each level of pointer (or array) indirection in the type.
  // D: If this constraint is generated because of a variable declaration, this
  //    should be a pointer to the the declaration AST node. May be null if the
  //    constraint is not generated by a declaration.
  // N: Name for the constraint variable. This may be chosen arbitrarily, as it
  //    it is only used for labeling graph nodes. Equality is based on generated
  //    unique ids.
  // I and C: Context objects required for this construction. It is expected
  //          that all constructor calls will take the same global objects here.
  // inFunc: If this variable is part of a function prototype, this string is
  //         the name of the function. nullptr otherwise.
  // ForceGenericIndex: CheckedC supports generic types (_Itype_for_any) which
  //                    need less restrictive constraints. Set >= 0 to indicate
  //                    that this variable should be considered generic.
  PointerVariableConstraint(const clang::QualType &QT, clang::DeclaratorDecl *D,
                            std::string N, ProgramInfo &I,
                            const clang::ASTContext &C,
                            std::string *InFunc = nullptr,
                            int ForceGenericIndex = -1,
                            bool VarAtomForChecked = false);

  const CAtoms &getCvars() const { return Vars; }

  // Include new ConstAtoms, supplemental info, and merge function pointers
  void mergeDeclaration(ConstraintVariable *From, ProgramInfo &I,
                        std::string &ReasonFailed) override;

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == PointerVariable;
  }

  std::string gatherQualStrings(void) const;

  std::string mkString(const EnvironmentMap &E, bool EmitName = true,
                       bool ForItype = false, bool EmitPointee = false,
                       bool UnmaskTypedef = false) const override;

  FunctionVariableConstraint *getFV() const { return FV; }

  void print(llvm::raw_ostream &O) const override;
  void dump() const override { print(llvm::errs()); }
  void dumpJson(llvm::raw_ostream &O) const override;

  void constrainToWild(Constraints &CS, const std::string &Rsn) const override;
  void constrainToWild(Constraints &CS, const std::string &Rsn,
                       PersistentSourceLoc *PL) const override;
  void constrainOuterTo(Constraints &CS, ConstAtom *C, bool DoLB = false);
  bool anyChanges(const EnvironmentMap &E) const override;
  bool anyArgumentIsWild(const EnvironmentMap &E);
  bool hasWild(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasArr(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasNtArr(const EnvironmentMap &E, int AIdx = -1) const override;

  void equateArgumentConstraints(ProgramInfo &I) override;

  bool isPartOfFunctionPrototype() const { return PartOfFuncPrototype; }
  // Add the provided constraint variable as an argument constraint.
  bool addArgumentConstraint(ConstraintVariable *DstCons, ProgramInfo &Info);
  // Get the set of constraint variables corresponding to the arguments.
  const std::set<ConstraintVariable *> &getArgumentConstraints() const;

  PointerVariableConstraint *getCopy(Constraints &CS) override;

  // Retrieve the atom at the specified index. This function includes special
  // handling for generic constraint variables to create deeper pointers as
  // they are needed.
  Atom *getAtom(unsigned int AtomIdx, Constraints &CS);

  ~PointerVariableConstraint() override{};
};

typedef PointerVariableConstraint PVConstraint;
// Name for function return, for debugging
#define RETVAR "$ret"

typedef struct {
  PersistentSourceLoc PL;
  std::vector<CVarSet> PS;
} ParamDeferment;

// This class contains a pair of PVConstraints that represent an internal and
// external view of a variable for use as the parameter and return constraints
// of FunctionVariableConstraints. The internal constraint represents how the
// variable is used inside the function. The external constraint represents
// how it can be used by callers to the function. For example, when a variable
// is used unsafely inside the function, the internal constraint variable will
// solve to an unchecked type, but the external constraint variable will still
// be checked. The rewriting of the function then gives it an itype, allowing
// callers to use the external checked type.
class FVComponentVariable {
private:
  friend class FunctionVariableConstraint;
  PVConstraint *InternalConstraint;
  PVConstraint *ExternalConstraint;

public:
  FVComponentVariable()
      : InternalConstraint(nullptr), ExternalConstraint(nullptr) {}

  FVComponentVariable(FVComponentVariable *Ot, Constraints &CS);
  FVComponentVariable(const clang::QualType &QT, clang::DeclaratorDecl *D,
                      std::string N, ProgramInfo &I, const clang::ASTContext &C,
                      std::string *InFunc, bool HasItype);

  void mergeDeclaration(FVComponentVariable *From, ProgramInfo &I,
                        std::string &ReasonFailed);
  std::string mkItypeStr(const EnvironmentMap &E) const;
  std::string mkTypeStr(const EnvironmentMap &E) const;
  std::string mkString(const EnvironmentMap &E) const;
};

// Constraints on a function type. Also contains a 'name' parameter for
// when a re-write of a function pointer is needed.
class FunctionVariableConstraint : public ConstraintVariable {
private:
  FunctionVariableConstraint(FunctionVariableConstraint *Ot, Constraints &CS);

  // N constraints on the return value of the function.
  FVComponentVariable ReturnVar;
  // A vector of K sets of N constraints on the parameter values, for
  // K parameters accepted by the function.
  std::vector<FVComponentVariable> ParamVars;

  // Storing of parameters in the case of untyped prototypes
  std::vector<ParamDeferment> DeferredParams;
  // File name in which this declaration is found.
  std::string FileName;
  bool Hasproto;
  bool Hasbody;
  bool IsStatic;
  FunctionVariableConstraint *Parent;
  // Flag to indicate whether this is a function pointer or not.
  bool IsFunctionPtr;

  // Count of type parameters from `_Itype_for_any(...)`.
  int TypeParams;

  void equateFVConstraintVars(ConstraintVariable *CV, ProgramInfo &Info) const;

public:
  FunctionVariableConstraint()
      : ConstraintVariable(FunctionVariable, "", ""), FileName(""),
        Hasproto(false), Hasbody(false), IsStatic(false), Parent(nullptr),
        IsFunctionPtr(false) {}

  FunctionVariableConstraint(clang::DeclaratorDecl *D, ProgramInfo &I,
                             const clang::ASTContext &C);
  FunctionVariableConstraint(const clang::Type *Ty, clang::DeclaratorDecl *D,
                             std::string N, ProgramInfo &I,
                             const clang::ASTContext &C);

  PVConstraint *getExternalReturn() const {
    return ReturnVar.ExternalConstraint;
  }

  PVConstraint *getInternalReturn() const {
    return ReturnVar.InternalConstraint;
  }

  const std::vector<ParamDeferment> &getDeferredParams() const {
    return DeferredParams;
  }

  void addDeferredParams(PersistentSourceLoc PL, std::vector<CVarSet> Ps);

  size_t numParams() const { return ParamVars.size(); }

  bool hasProtoType() const { return Hasproto; }
  bool hasBody() const { return Hasbody; }
  void setHasBody(bool Hbody) { this->Hasbody = Hbody; }

  static bool classof(const ConstraintVariable *S) {
    return S->getKind() == FunctionVariable;
  }

  // Merge return value and all params
  void mergeDeclaration(ConstraintVariable *FromCV, ProgramInfo &I,
                        std::string &ReasonFailed) override;

  PVConstraint *getExternalParam(unsigned I) const {
    assert(I < ParamVars.size());
    return ParamVars.at(I).ExternalConstraint;
  }

  PVConstraint *getInternalParam(unsigned I) const {
    assert(I < ParamVars.size());
    return ParamVars.at(I).InternalConstraint;
  }

  bool srcHasItype() const override;
  bool srcHasBounds() const override;

  int getGenericIndex() const {
    return ReturnVar.ExternalConstraint->getGenericIndex();
  }

  bool solutionEqualTo(Constraints &CS, const ConstraintVariable *CV,
                       bool ComparePtyp = true) const override;

  std::string mkString(const EnvironmentMap &E, bool EmitName = true,
                       bool ForItype = false, bool EmitPointee = false,
                       bool UnmaskTypedef = false) const override;
  void print(llvm::raw_ostream &O) const override;
  void dump() const override { print(llvm::errs()); }
  void dumpJson(llvm::raw_ostream &O) const override;

  void constrainToWild(Constraints &CS, const std::string &Rsn) const override;
  void constrainToWild(Constraints &CS, const std::string &Rsn,
                       PersistentSourceLoc *PL) const override;
  bool anyChanges(const EnvironmentMap &E) const override;
  bool hasWild(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasArr(const EnvironmentMap &E, int AIdx = -1) const override;
  bool hasNtArr(const EnvironmentMap &E, int AIdx = -1) const override;

  void equateArgumentConstraints(ProgramInfo &P) override;

  FunctionVariableConstraint *getCopy(Constraints &CS) override;

  bool getIsOriginallyChecked() const override;

  ~FunctionVariableConstraint() override {}
};

typedef FunctionVariableConstraint FVConstraint;

#endif // LLVM_CLANG_3C_CONSTRAINTVARIABLES_H

//=--ConstraintVariables.cpp--------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of ConstraintVariables methods.
//
//===----------------------------------------------------------------------===//

#include "clang/3C/ConstraintVariables.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ProgramInfo.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/CommandLine.h"
#include <sstream>

using namespace clang;
// Macro for boolean implication
#define IMPLIES(a,b) ((a) ? (b) : true)

static llvm::cl::OptionCategory OptimizationCategory("Optimization category");
static llvm::cl::opt<bool>
    DisableRDs("disable-rds",
               llvm::cl::desc("Disable reverse edges for Checked Constraints."),
               llvm::cl::init(false), llvm::cl::cat(OptimizationCategory));

std::string ConstraintVariable::getRewritableOriginalTy() const {
  std::string OrigTyString = getOriginalTy();
  std::string SpaceStr = " ";
  std::string AsterixStr = "*";
  // If the type does not end with " " or *
  // we need to add space.
  if (!std::equal(SpaceStr.rbegin(), SpaceStr.rend(), OrigTyString.rbegin()) &&
      !std::equal(AsterixStr.rbegin(), AsterixStr.rend(),
                  OrigTyString.rbegin())) {
    OrigTyString += " ";
  }
  return OrigTyString;
}
bool ConstraintVariable::isChecked(const EnvironmentMap &E) const {
  return getIsOriginallyChecked() || anyChanges(E);
}

PointerVariableConstraint *PointerVariableConstraint::getWildPVConstraint(
    Constraints &CS, const std::string &Rsn, PersistentSourceLoc *PSL) {
  VarAtom *VA =
      CS.createFreshGEQ("wildvar", VarAtom::V_Other, CS.getWild(), Rsn, PSL);
  CAtoms NewAtoms = {VA};
  PVConstraint *WildPVC = new PVConstraint(NewAtoms, "unsigned", "wildvar",
                                           nullptr, false, false, "");
  return WildPVC;
}

PointerVariableConstraint *
PointerVariableConstraint::getPtrPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalPtrPV = nullptr;
  if (GlobalPtrPV == nullptr) {
    CAtoms NewVA;
    NewVA.push_back(CS.getPtr());
    GlobalPtrPV = new PVConstraint(NewVA, "unsigned", "ptrvar", nullptr, false,
                                   false, "");
  }
  return GlobalPtrPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getNonPtrPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalNonPtrPV = nullptr;
  if (GlobalNonPtrPV == nullptr) {
    CAtoms NewVA; // empty -- represents a base type
    GlobalNonPtrPV = new PVConstraint(NewVA, "unsigned", "basevar", nullptr,
                                      false, false, "");
  }
  return GlobalNonPtrPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getNamedNonPtrPVConstraint(StringRef Name,
                                                      Constraints &CS) {
  CAtoms NewVA; // empty -- represents a base type
  return new PVConstraint(NewVA, "unsigned", Name, nullptr, false, false, "");
}

PointerVariableConstraint::PointerVariableConstraint(
    PointerVariableConstraint *Ot, Constraints &CS)
    : ConstraintVariable(ConstraintVariable::PointerVariable, Ot->BaseType,
                         Ot->Name),
      FV(nullptr), PartOfFuncPrototype(Ot->PartOfFuncPrototype) {
  this->ArrSizes = Ot->ArrSizes;
  this->ArrPresent = Ot->ArrPresent;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->ValidBoundsKey = Ot->ValidBoundsKey;
  this->BKey = Ot->BKey;
  // Make copy of the vars only for VarAtoms.
  for (auto *CV : Ot->Vars) {
    if (ConstAtom *CA = dyn_cast<ConstAtom>(CV)) {
      this->Vars.push_back(CA);
    }
    if (VarAtom *VA = dyn_cast<VarAtom>(CV)) {
      this->Vars.push_back(CS.getFreshVar(VA->getName(), VA->getVarKind()));
    }
  }
  if (Ot->FV != nullptr) {
    this->FV = dyn_cast<FVConstraint>(Ot->FV->getCopy(CS));
  }
  this->Parent = Ot;
  this->IsGeneric = Ot->IsGeneric;
  this->IsZeroWidthArray = Ot->IsZeroWidthArray;
  // We need not initialize other members.
}

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
                                                     ProgramInfo &I,
                                                     const ASTContext &C) :
        PointerVariableConstraint(D->getType(), D, D->getName(),
                                  I, C) { }


// Simple recursive visitor for determining if a type contains a typedef
// entrypoint is find()
class TypedefLevelFinder : public RecursiveASTVisitor<TypedefLevelFinder> {
  public:

    static struct InternalTypedefInfo find(const QualType &QT) {
      TypedefLevelFinder TLF;
      QualType tosearch;
      // If the type is currently a typedef, desugar that.
      // This is so we can see if the type _contains_ a typedef
      if (auto TDT = dyn_cast<TypedefType>(QT))
        tosearch = TDT->desugar();
      else
        tosearch = QT;
      TLF.TraverseType(tosearch);
      // If we found a typedef the we need to have filled out the name field
      assert(IMPLIES(TLF.hastypedef, TLF.TDname != ""));
      struct InternalTypedefInfo info = 
      	{ TLF.hastypedef, TLF.typedeflevel, TLF.TDname };
      return info;
    }

    bool VisitTypedefType(TypedefType *TT) {
      hastypedef = true;
      auto TDT = TT->getDecl();
      TDname = TDT->getNameAsString();
      return false;
    }

    bool VisitPointerType(PointerType *PT) {
      typedeflevel++;
      return true;
    }

    bool VisitArrayType(ArrayType *AT) {
      typedeflevel++;
      return true;
    }


  private:
    int typedeflevel = 0;
    std::string TDname = "";
    bool hastypedef = false;

};


PointerVariableConstraint::PointerVariableConstraint(
    const QualType &QT, DeclaratorDecl *D, std::string N, ProgramInfo &I,
    const ASTContext &C, std::string *InFunc, bool Generic)
    : ConstraintVariable(ConstraintVariable::PointerVariable,
                         tyToStr(QT.getTypePtr()), N),
      FV(nullptr), PartOfFuncPrototype(InFunc != nullptr), Parent(nullptr),
      IsGeneric(Generic) {
  QualType QTy = QT;
  const Type *Ty = QTy.getTypePtr();
  auto &CS = I.getConstraints();
  // If the type is a decayed type, then maybe this is the result of
  // decaying an array to a pointer. If the original type is some
  // kind of array type, we want to use that instead.
  if (const DecayedType *DC = dyn_cast<DecayedType>(Ty)) {
    QualType QTytmp = DC->getOriginalType();
    if (QTytmp->isArrayType() || QTytmp->isIncompleteArrayType()) {
      QTy = QTytmp;
      Ty = QTy.getTypePtr();
    }
  }

  bool IsTypedef = false;
  if (Ty->getAs<TypedefType>())
    IsTypedef = true;

  ArrPresent = false;

  bool IsDeclTy = false;

  auto &ABInfo = I.getABoundsInfo();
  if (D != nullptr) {
    if (ABInfo.tryGetVariable(D, BKey)) {
      ValidBoundsKey = true;
    }
    if (D->hasBoundsAnnotations()) {
      BoundsAnnotations BA = D->getBoundsAnnotations();
      BoundsExpr *BExpr = BA.getBoundsExpr();
      if (BExpr != nullptr) {
        SourceRange R = BExpr->getSourceRange();
        if (R.isValid()) {
          BoundsAnnotationStr = getSourceText(R, C);
        }
        if (D->hasBoundsAnnotations() && ABInfo.isValidBoundVariable(D)) {
          assert(ABInfo.tryGetVariable(D, BKey) &&
                 "Is expected to have valid Bounds key");

          ABounds *NewB = ABounds::getBoundsInfo(&ABInfo, BExpr, C);
          ABInfo.insertDeclaredBounds(D, NewB);
        }
      }
    }

    IsDeclTy = D->getType() == QT; // If false, then QT may be D's return type
    if (InteropTypeExpr *ITE = D->getInteropTypeExpr()) {
      // External variables can also have itype.
      // Check if the provided declaration is an external
      // variable.
      // For functions, check to see that if we are analyzing
      // function return types.
      bool AnalyzeITypeExpr = IsDeclTy;
      if (!AnalyzeITypeExpr) {
        const Type *OrigType = Ty;
        if (isa<FunctionDecl>(D)) {
          FunctionDecl *FD = dyn_cast<FunctionDecl>(D);
          OrigType = FD->getType().getTypePtr();
        }
        if (OrigType->isFunctionProtoType()) {
          const FunctionProtoType *FPT = dyn_cast<FunctionProtoType>(OrigType);
          AnalyzeITypeExpr = (FPT->getReturnType() == QT);
        }
      }
      if (AnalyzeITypeExpr) {
        QualType InteropType = ITE->getTypeAsWritten();
        QTy = InteropType;
        Ty = QTy.getTypePtr();

        SourceRange R = ITE->getSourceRange();
        if (R.isValid()) {
          ItypeStr = getSourceText(R, C);
          assert(ItypeStr.size() > 0);
        }
      }
    }
  }

  typedeflevelinfo = TypedefLevelFinder::find(QTy);

  bool VarCreated = false;
  bool IsArr = false;
  bool IsIncompleteArr = false;
  bool IsTopMost = true;
  uint32_t TypeIdx = 0;
  std::string Npre = InFunc ? ((*InFunc) + ":") : "";
  VarAtom::VarKind VK =
      InFunc ? (N == RETVAR ? VarAtom::V_Return : VarAtom::V_Param)
             : VarAtom::V_Other;

  while (Ty->isPointerType() || Ty->isArrayType()) {
    // Is this a VarArg type?
    std::string TyName = tyToStr(Ty);
    if (isVarArgType(TyName)) {
      // Variable number of arguments. Make it WILD.
      std::string Rsn = "Variable number of arguments.";
      VarAtom *WildVA = CS.createFreshGEQ(Npre + N, VK, CS.getWild(), Rsn);
      Vars.push_back(WildVA);
      VarCreated = true;
      break;
    }

    if (Ty->isCheckedPointerType() || Ty->isCheckedArrayType()) {
      ConstAtom *CAtom = nullptr;
      if (Ty->isCheckedPointerNtArrayType() || Ty->isNtCheckedArrayType()) {
        // This is an NT array type.
        CAtom = CS.getNTArr();
      } else if (Ty->isCheckedPointerArrayType() || Ty->isCheckedArrayType()) {
        // This is an array type.
        CAtom = CS.getArr();
      } else if (Ty->isCheckedPointerPtrType()) {
        // This is a regular checked pointer.
        CAtom = CS.getPtr();
      }
      VarCreated = true;
      assert(CAtom != nullptr && "Unable to find the type "
                                 "of the checked pointer.");
      Vars.push_back(CAtom);
    }

    if (Ty->isArrayType() || Ty->isIncompleteArrayType()) {
      ArrPresent = IsArr = true;
      IsIncompleteArr = Ty->isIncompleteArrayType();

      // Boil off the typedefs in the array case.
      // TODO this will need to change to properly account for typedefs
      bool Boiling = true;
      while (Boiling) {
        if (const TypedefType *TydTy = dyn_cast<TypedefType>(Ty)) {
          QTy = TydTy->desugar();
          Ty = QTy.getTypePtr();
        } else if (const ParenType *ParenTy = dyn_cast<ParenType>(Ty)) {
          QTy = ParenTy->desugar();
          Ty = QTy.getTypePtr();
        } else {
          Boiling = false;
        }
      }

      // See if there is a constant size to this array type at this position.
      if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(Ty)) {
        ArrSizes[TypeIdx] = std::pair<OriginalArrType, uint64_t>(
            O_SizedArray, CAT->getSize().getZExtValue());

        // If this is the top-most pointer variable?
        if (hasBoundsKey() && IsTopMost) {
          BoundsKey CBKey = ABInfo.getConstKey(CAT->getSize().getZExtValue());
          ABounds *NB = new CountBound(CBKey);
          ABInfo.insertDeclaredBounds(D, NB);
        }
      } else {
        ArrSizes[TypeIdx] =
            std::pair<OriginalArrType, uint64_t>(O_UnSizedArray, 0);
      }

      // Iterate.
      if (const ArrayType *ArrTy = dyn_cast<ArrayType>(Ty)) {
        QTy = ArrTy->getElementType();
        Ty = QTy.getTypePtr();
      } else {
        llvm_unreachable("unknown array type");
      }
    } else {

      // Save here if QTy is qualified or not into a map that
      // indexes K to the qualification of QTy, if any.
      insertQualType(TypeIdx, QTy);

      ArrSizes[TypeIdx] = std::pair<OriginalArrType, uint64_t>(O_Pointer, 0);

      // Iterate.
      QTy = QTy.getSingleStepDesugaredType(C);
      QTy = QTy.getTypePtr()->getPointeeType();
      Ty = QTy.getTypePtr();
    }

    // This type is not a constant atom. We need to create a VarAtom for this.

    if (!VarCreated) {
      VarAtom *VA = CS.getFreshVar(Npre + N, VK);
      Vars.push_back(VA);

      // Incomplete arrays are lower bounded to ARR because the transformation
      // int[] -> _Ptr<int> is permitted while int[1] -> _Ptr<int> is not.
      if (IsIncompleteArr)
        CS.addConstraint(CS.createGeq(VA, CS.getArr(), false));
      else if (IsArr)
        CS.addConstraint(CS.createGeq(CS.getArr(), VA, false));
    }

    // Prepare for next level of pointer
    VarCreated = false;
    IsArr = false;
    TypeIdx++;
    Npre = Npre + "*";
    VK = VarAtom::
        V_Other; // only the outermost pointer considered a param/return
    IsTopMost = false;
  }
  insertQualType(TypeIdx, QTy);

  // In CheckedC, a pointer can be freely converted to a size 0 array pointer,
  // but our constraint system does not allow this. To enable converting calls
  // to functions with types similar to free, size 0 array pointers are made PTR
  // instead of ARR.
  IsZeroWidthArray = false;
  if (D && D->hasBoundsExpr() && !Vars.empty() && Vars[0] == CS.getArr())
    if (BoundsExpr *BE = D->getBoundsExpr())
      if (isZeroBoundsExpr(BE, C)) {
        IsZeroWidthArray = true;
        Vars[0] = CS.getPtr();
      }

  // If, after boiling off the pointer-ness from this type, we hit a
  // function, then create a base-level FVConstraint that we carry
  // around too.
  if (Ty->isFunctionType())
    // C function-pointer type declarator syntax embeds the variable
    // name within the function-like syntax. For example:
    //    void (*fname)(int, int) = ...;
    // If a typedef'ed type name is used, the name can be omitted
    // because it is not embedded like that. Instead, it has the form
    //    tn fname = ...,
    // where tn is the typedef'ed type name.
    // There is possibly something more elegant to do in the code here.
    FV = new FVConstraint(Ty, IsDeclTy ? D : nullptr, IsTypedef ? "" : N, I, C);

  // Get a string representing the type without pointer and array indirection.
  BaseType = extractBaseType(D, QT, Ty, C);

  bool IsWild = !IsGeneric && (isVarArgType(BaseType) || isTypeHasVoid(QT));
  if (IsWild) {
    std::string Rsn =
        isTypeHasVoid(QT) ? "Default void* type" : "Default Var arg list type";
    // TODO: Github issue #61: improve handling of types for variable arguments.
    for (const auto &V : Vars)
      if (VarAtom *VA = dyn_cast<VarAtom>(V))
        CS.addConstraint(CS.createGeq(VA, CS.getWild(), Rsn));
  }

  // Add qualifiers.
  std::ostringstream QualStr;
  getQualString(TypeIdx, QualStr);
  BaseType = QualStr.str() + BaseType;

  // Here lets add implication that if outer pointer is WILD
  // then make the inner pointers WILD too.
  if (Vars.size() > 1) {
    bool UsedPrGeq = false;
    for (auto VI = Vars.begin(), VE = Vars.end(); VI != VE; VI++) {
      if (VarAtom *VIVar = dyn_cast<VarAtom>(*VI)) {
        // Premise.
        Geq *PrGeq = new Geq(VIVar, CS.getWild());
        UsedPrGeq = false;
        for (auto VJ = (VI + 1); VJ != VE; VJ++) {
          if (VarAtom *VJVar = dyn_cast<VarAtom>(*VJ)) {
            // Conclusion.
            Geq *CoGeq = new Geq(VJVar, CS.getWild());
            CS.addConstraint(CS.createImplies(PrGeq, CoGeq));
            UsedPrGeq = true;
          }
        }
        // Delete unused constraint.
        if (!UsedPrGeq) {
          delete (PrGeq);
        }
      }
    }
  }
}

std::string PointerVariableConstraint::extractBaseType(DeclaratorDecl *D,
                                                       QualType QT,
                                                       const Type *Ty,
                                                       const ASTContext &C) {
  std::string BaseTypeStr;
  bool FoundBaseTypeInSrc = false;
  if (!QT->isOrContainsCheckedType() && !Ty->getAs<TypedefType>() && D &&
      D->getTypeSourceInfo()) {
    // Try to extract the type from original source to preserve defines
    TypeLoc TL = D->getTypeSourceInfo()->getTypeLoc();
    if (isa<FunctionDecl>(D)) {
      FoundBaseTypeInSrc = D->getAsFunction()->getReturnType() == QT;
      TL = getBaseTypeLoc(TL).getAs<FunctionTypeLoc>();
      // FunctionDecl that doesn't have function type? weird
      if (TL.isNull())
        FoundBaseTypeInSrc = false;
      else
        TL = TL.getAs<clang::FunctionTypeLoc>().getReturnLoc();
    } else {
      FoundBaseTypeInSrc = D->getType() == QT;
    }
    TypeLoc BaseLoc = getBaseTypeLoc(TL);
    if (!BaseLoc.getAs<TypedefTypeLoc>().isNull()) {
      FoundBaseTypeInSrc = false;
    } else {
      // Only use this type if the type passed as a parameter to this
      // constructor agrees with the actual type of the declaration.
      SourceRange SR = BaseLoc.getSourceRange();
      if (FoundBaseTypeInSrc && SR.isValid()) {
        BaseTypeStr = getSourceText(SR, C);

        // getSourceText returns the empty string when there's a pointer level
        // inside a macro. Not sure how to handle this, so fall back to tyToStr.
        if (BaseTypeStr.empty())
          FoundBaseTypeInSrc = false;
      } else
        FoundBaseTypeInSrc = false;
    }
  }
  // Fall back to rebuilding the base type based on type passed to constructor
  if (!FoundBaseTypeInSrc)
    BaseTypeStr = tyToStr(Ty);

  return BaseTypeStr;
}

void PointerVariableConstraint::print(raw_ostream &O) const {
  O << "{ ";
  for (const auto &I : Vars) {
    I->print(O);
    O << " ";
  }
  O << " }";

  if (FV) {
    O << "(";
    FV->print(O);
    O << ")";
  }
}

void PointerVariableConstraint::dumpJson(llvm::raw_ostream &O) const {
  O << "{\"PointerVar\":{";
  O << "\"Vars\":[";
  bool AddComma = false;
  for (const auto &I : Vars) {
    if (AddComma) {
      O << ",";
    }
    I->dumpJson(O);

    AddComma = true;
  }
  O << "], \"name\":\"" << getName() << "\"";
  if (FV) {
    O << ", \"FunctionVariable\":";
    FV->dumpJson(O);
  }
  O << "}}";
}

void PointerVariableConstraint::getQualString(uint32_t TypeIdx,
                                              std::ostringstream &Ss) const {
  auto QIter = QualMap.find(TypeIdx);
  if (QIter != QualMap.end()) {
    for (Qualification Q : QIter->second) {
      switch (Q) {
      case ConstQualification:
        Ss << "const ";
        break;
      case VolatileQualification:
        Ss << "volatile ";
        break;
      case RestrictQualification:
        Ss << "restrict ";
        break;
      }
    }
  }
}

void PointerVariableConstraint::insertQualType(uint32_t TypeIdx,
                                               QualType &QTy) {
  if (QTy.isConstQualified())
    QualMap[TypeIdx].insert(ConstQualification);
  if (QTy.isVolatileQualified())
    QualMap[TypeIdx].insert(VolatileQualification);
  if (QTy.isRestrictQualified())
    QualMap[TypeIdx].insert(RestrictQualification);
}

//   emitArraySize
//   Take an array or nt_array variable, determines if it is
//   a constant array, and if so emits the apprioate syntax for a
//   stack-based array. This functions also updates various flags.
bool PointerVariableConstraint::emitArraySize(
    std::stack<std::string> &CheckedArrs, uint32_t TypeIdx,
    // Is the type only an array
    bool &AllArrays,
    // Are we processing an array
    bool &ArrayRun, bool Nt) const {
  bool Ret = false;
  if (ArrPresent) {
    auto I = ArrSizes.find(TypeIdx);
    assert(I != ArrSizes.end());
    OriginalArrType Oat = I->second.first;
    uint64_t Oas = I->second.second;

    std::ostringstream SizeStr;

    if (Oat == O_SizedArray) {
      SizeStr << (Nt ? " _Nt_checked" : " _Checked");
      SizeStr << "[" << Oas << "]";
      CheckedArrs.push(SizeStr.str());
      ArrayRun = true;
      Ret = true;
    } else {
      AllArrays = ArrayRun = false;
    }

    return Ret;
  }
  return Ret;
}

/*  addArrayAnnotiations
 *  This function takes all the stacked annotations for constant arrays
 *  and pops them onto the EndStrs, this ensures the right order of annotations
 *   */
void PointerVariableConstraint::addArrayAnnotations(
    std::stack<std::string> &CheckedArrs,
    std::deque<std::string> &EndStrs) const {
  while (!CheckedArrs.empty()) {
    auto NextStr = CheckedArrs.top();
    CheckedArrs.pop();
    EndStrs.push_front(NextStr);
  }
  assert(CheckedArrs.empty());
}


bool PointerVariableConstraint::isTypedef(void) {
  return IsTypedef;
}

void PointerVariableConstraint::setTypedef(TypedefNameDecl* T, std::string s) {
  IsTypedef = true;
  TDT = T;
  typedefString = s;
}


// Mesh resolved constraints with the PointerVariableConstraints set of
// variables and potentially nested function pointer declaration. Produces a
// string that can be replaced in the source code.

std::string PointerVariableConstraint::mkString(const EnvironmentMap &E,
                                                bool EmitName, bool ForItype,
                                                bool EmitPointee, bool UnmaskTypedef) const {
  if (IsTypedef && !UnmaskTypedef) {
    return typedefString + (EmitName && getName() != RETVAR ? (" " + getName()) : " ");
  }

  std::ostringstream Ss;
  // This deque will store all the type strings that need to pushed
  // to the end of the type string. This is typically things like
  // closing delimiters.
  std::deque<std::string> EndStrs;
  // This will store stacked array decls to ensure correct order
  // We encounter constant arrays variables in the reverse order they
  // need to appear in, so the LIFO structure reverses these annotations
  std::stack<std::string> CheckedArrs;
  // Have we emitted the string for the base type
  bool EmittedBase = false;
  // Have we emitted the name of the variable yet?
  bool EmittedName = false;
  // Was the last variable an Array?
  bool PrevArr = false;
  // Is the entire type so far an array?
  bool AllArrays = true;
  // Are we in a sequence of arrays
  bool ArrayRun = false;
  if (!EmitName || getName() == RETVAR)
    EmittedName = true;
  uint32_t TypeIdx = 0;

  auto It = Vars.begin();
  auto i = 0;
  // Skip over first pointer level if only emitting pointee string.
  // This is needed when inserting type arguments.
  if (EmitPointee)
    ++It;
  // Interate through the vars(), but if we have an internal typedef, then stop once you reach the
  // typedef's level
  for (; It != Vars.end() && IMPLIES(typedeflevelinfo.hasTypedef, i < typedeflevelinfo.typedefLevel); ++It, i++) {
    const auto &V = *It;
    ConstAtom *C = nullptr;
    if (ConstAtom *CA = dyn_cast<ConstAtom>(V)) {
      C = CA;
    } else {
      VarAtom *VA = dyn_cast<VarAtom>(V);
      assert(VA != nullptr && "Constraint variable can "
                              "be either constant or VarAtom.");
      C = E.at(VA).first;
    }
    assert(C != nullptr);

    Atom::AtomKind K = C->getKind();

    // If this is not an itype
    // make this wild as it can hold any pointer type.
    if (!ForItype && BaseType == "void")
      K = Atom::A_Wild;

    if (PrevArr && ArrSizes.at(TypeIdx).first != O_SizedArray && !EmittedName) {
      EmittedName = true;
      addArrayAnnotations(CheckedArrs, EndStrs);
      EndStrs.push_front(" " + getName());
    }
    PrevArr = ((K == Atom::A_Arr || K == Atom::A_NTArr) && ArrPresent &&
               ArrSizes.at(TypeIdx).first == O_SizedArray);

    switch (K) {
    case Atom::A_Ptr:
      getQualString(TypeIdx, Ss);

      // We need to check and see if this level of variable
      // is constrained by a bounds safe interface. If it is,
      // then we shouldn't re-write it.
      AllArrays = false;
      EmittedBase = false;
      Ss << "_Ptr<";
      ArrayRun = false;
      EndStrs.push_front(">");
      break;
    case Atom::A_Arr:
      // If this is an array.
      getQualString(TypeIdx, Ss);
      // If it's an Arr, then the character we substitute should
      // be [] instead of *, IF, the original type was an array.
      // And, if the original type was a sized array of size K.
      // we should substitute [K].
      if (emitArraySize(CheckedArrs, TypeIdx, AllArrays, ArrayRun, false))
        break;
      // We need to check and see if this level of variable
      // is constrained by a bounds safe interface. If it is,
      // then we shouldn't re-write it.
      EmittedBase = false;
      Ss << "_Array_ptr<";
      EndStrs.push_front(">");
      break;
    case Atom::A_NTArr:

      if (emitArraySize(CheckedArrs, TypeIdx, AllArrays, ArrayRun, true))
        break;
      // This additional check is to prevent fall-through from the array.
      if (K == Atom::A_NTArr) {
        // If this is an NTArray.
        getQualString(TypeIdx, Ss);

        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        EmittedBase = false;
        Ss << "_Nt_array_ptr<";
        EndStrs.push_front(">");
        break;
      }
      LLVM_FALLTHROUGH;
    // If there is no array in the original program, then we fall through to
    // the case where we write a pointer value.
    case Atom::A_Wild:
      AllArrays = false;
      if (ArrayRun)
        addArrayAnnotations(CheckedArrs, EndStrs);
      ArrayRun = false;
      if (EmittedBase) {
        Ss << "*";
      } else {
        assert(BaseType.size() > 0);
        EmittedBase = true;
        if (FV) {
          Ss << FV->mkString(E);
        } else {
          Ss << BaseType << " *";
        }
      }

      getQualString(TypeIdx, Ss);
      break;
    case Atom::A_Const:
    case Atom::A_Var:
      llvm_unreachable("impossible");
      break;
    }
    TypeIdx++;
  }

  // If the previous variable was an array or
  // if we are leaving an array run, we need to emit the
  // annotation for a stack-array
  if ((PrevArr || ArrayRun) && !CheckedArrs.empty())
    addArrayAnnotations(CheckedArrs, EndStrs);

  // If the whole type is an array so far, and we haven't emitted
  // a name yet, then emit the name so that it appears before
  // the the stack array type.
  if (PrevArr && !EmittedName && AllArrays) {
    EmittedName = true;
    EndStrs.push_front(" " + getName());
  }

  if (EmittedBase == false) {
    // If we have a FV pointer, then our "base" type is a function pointer.
    // type.
    if (FV) {
      Ss << FV->mkString(E);
    } else if (typedeflevelinfo.hasTypedef) {
      auto name = typedeflevelinfo.typedefName;
      Ss << name;
    } else {
      Ss << BaseType;
    }
  }

  // Add closing elements to type
  for (std::string Str : EndStrs) {
    Ss << Str;
  }

  // No space after itype.
  if (!EmittedName)
    Ss << " " << getName();

  // Final array dropping
  if (!CheckedArrs.empty())
    addArrayAnnotations(CheckedArrs, EndStrs);

  //TODO remove comparison to RETVAR
  if (getName() == RETVAR && !ForItype)
    Ss << " ";

  return Ss.str();
}

bool PVConstraint::addArgumentConstraint(ConstraintVariable *DstCons,
                                         ProgramInfo &Info) {
  if (this->Parent == nullptr) {
    bool RetVal = false;
    if (isPartOfFunctionPrototype()) {
      RetVal = ArgumentConstraints.insert(DstCons).second;
      if (RetVal && this->HasEqArgumentConstraints) {
        constrainConsVarGeq(DstCons, this, Info.getConstraints(), nullptr,
                            Same_to_Same, true, &Info);
      }
    }
    return RetVal;
  }
  return this->Parent->addArgumentConstraint(DstCons, Info);
}

const CVarSet &PVConstraint::getArgumentConstraints() const {
  return ArgumentConstraints;
}

FunctionVariableConstraint::FunctionVariableConstraint(
    FunctionVariableConstraint *Ot, Constraints &CS)
    : ConstraintVariable(ConstraintVariable::FunctionVariable, Ot->OriginalType,
                         Ot->getName()) {
  this->IsStatic = Ot->IsStatic;
  this->FileName = Ot->FileName;
  this->Hasbody = Ot->Hasbody;
  this->Hasproto = Ot->Hasproto;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->IsFunctionPtr = Ot->IsFunctionPtr;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->ReturnVar = Ot->ReturnVar;
  // Make copy of ParameterCVs too.
  for (auto &ParmPv : Ot->ParamVars)
    this->ParamVars.push_back(ParmPv->getCopy(CS));
  this->Parent = Ot;
}

// This describes a function, either a function pointer or a function
// declaration itself. Require constraint variables for each argument and
// return, even those that aren't pointer types, since we may need to
// re-emit the function signature as a type.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
                                                       ProgramInfo &I,
                                                       const ASTContext &C)
    : FunctionVariableConstraint(
          D->getType().getTypePtr(), D,
          (D->getDeclName().isIdentifier() ? D->getName() : ""), I, C) {}

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
                                                       DeclaratorDecl *D,
                                                       std::string N,
                                                       ProgramInfo &I,
                                                       const ASTContext &Ctx)
    : ConstraintVariable(ConstraintVariable::FunctionVariable, tyToStr(Ty), N),
      Parent(nullptr) {
  QualType RT;
  Hasproto = false;
  Hasbody = false;
  FileName = "";
  HasEqArgumentConstraints = false;
  IsFunctionPtr = true;

  // Metadata about function
  FunctionDecl *FD = nullptr;
  if (D)
    FD = dyn_cast<FunctionDecl>(D);
  if (FD) {
    // FunctionDecl::hasBody will return true if *any* declaration in the
    // declaration chain has a body, which is not what we want to record.
    // We want to record if *this* declaration has a body. To do that,
    // we'll check if the declaration that has the body is different
    // from the current declaration.
    const FunctionDecl *OFd = nullptr;
    if (FD->hasBody(OFd) && OFd == FD)
      Hasbody = true;
    IsStatic = !(FD->isGlobal());
    ASTContext *TmpCtx = const_cast<ASTContext *>(&Ctx);
    auto PSL = PersistentSourceLoc::mkPSL(D, *TmpCtx);
    FileName = PSL.getFileName();
    IsFunctionPtr = false;
  }

  // ConstraintVariables for the parameters
  if (Ty->isFunctionPointerType()) {
    // Is this a function pointer definition?
    llvm_unreachable("should not hit this case");
  } else if (Ty->isFunctionProtoType()) {
    // Is this a function?
    const FunctionProtoType *FT = Ty->getAs<FunctionProtoType>();
    assert(FT != nullptr);
    RT = FT->getReturnType();

    // Extract the types for the parameters to this function. If the parameter
    // has a bounds expression associated with it, substitute the type of that
    // bounds expression for the other type.
    for (unsigned J = 0; J < FT->getNumParams(); J++) {
      QualType QT = FT->getParamType(J);

      std::string PName = "";
      DeclaratorDecl *ParmVD = nullptr;
      if (FD && J < FD->getNumParams()) {
        ParmVarDecl *PVD = FD->getParamDecl(J);
        if (PVD) {
          ParmVD = PVD;
          PName = PVD->getName();
        }
      }
      bool IsGeneric =
          ParmVD != nullptr && getTypeVariableType(ParmVD) != nullptr;
      ParamVars.push_back(
          new PVConstraint(QT, ParmVD, PName, I, Ctx, &N, IsGeneric));
    }

    Hasproto = true;
  } else if (Ty->isFunctionNoProtoType()) {
    const FunctionNoProtoType *FT = Ty->getAs<FunctionNoProtoType>();
    assert(FT != nullptr);
    RT = FT->getReturnType();
  } else {
    llvm_unreachable("don't know what to do");
  }

  // ConstraintVariable for the return
  bool IsGeneric = FD != nullptr && getTypeVariableType(FD) != nullptr;
  ReturnVar = new PVConstraint(RT, D, RETVAR, I, Ctx, &N, IsGeneric);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 const std::string &Rsn) const {
  constrainToWild(CS, Rsn, nullptr);
}

void FunctionVariableConstraint::constrainToWild(
    Constraints &CS, const std::string &Rsn, PersistentSourceLoc *PL) const {
  ReturnVar->constrainToWild(CS, Rsn, PL);

  for (const auto &V : ParamVars)
    V->constrainToWild(CS, Rsn, PL);
}

bool FunctionVariableConstraint::anyChanges(const EnvironmentMap &E) const {
  return ReturnVar->anyChanges(E) ||
         llvm::any_of(ParamVars, [&E](ConstraintVariable *CV) {
           return CV->anyChanges(E);
         });
}

bool FunctionVariableConstraint::hasWild(const EnvironmentMap &E,
                                         int AIdx) const {
  return ReturnVar->hasWild(E, AIdx);
}

bool FunctionVariableConstraint::hasArr(const EnvironmentMap &E,
                                        int AIdx) const {
  return ReturnVar->hasArr(E, AIdx);
}

bool FunctionVariableConstraint::hasNtArr(const EnvironmentMap &E,
                                          int AIdx) const {
  return ReturnVar->hasNtArr(E, AIdx);
}

FVConstraint *FunctionVariableConstraint::getCopy(Constraints &CS) {
  return new FVConstraint(this, CS);
}

void PVConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }
  HasEqArgumentConstraints = true;
  constrainConsVarGeq(this, this->ArgumentConstraints, Info.getConstraints(),
                      nullptr, Same_to_Same, true, &Info);

  if (this->FV != nullptr) {
    this->FV->equateArgumentConstraints(Info);
  }
}

void FunctionVariableConstraint::equateFVConstraintVars(
    ConstraintVariable *CV, ProgramInfo &Info) const {
  if (FVConstraint *FVCons = dyn_cast<FVConstraint>(CV)) {
    for (auto &PCon : FVCons->ParamVars)
      PCon->equateArgumentConstraints(Info);
    FVCons->ReturnVar->equateArgumentConstraints(Info);
  }
}

void FunctionVariableConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }

  HasEqArgumentConstraints = true;

  // Equate arguments and parameters vars.
  this->equateFVConstraintVars(this, Info);

  // Is this not a function pointer?
  if (!IsFunctionPtr) {
    FVConstraint *DefnCons = nullptr;

    // Get appropriate constraints based on whether the function is static or
    // not.
    if (IsStatic) {
      DefnCons = Info.getStaticFuncConstraint(Name, FileName);
    } else {
      DefnCons = Info.getExtFuncDefnConstraint(Name);
    }
    assert(DefnCons != nullptr);

    // Equate arguments and parameters vars.
    this->equateFVConstraintVars(DefnCons, Info);
  }
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                const std::string &Rsn) const {
  constrainToWild(CS, Rsn, nullptr);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                const std::string &Rsn,
                                                PersistentSourceLoc *PL) const {
  // Find the first VarAtom. All atoms before this are ConstAtoms, so
  // constraining them isn't useful;
  VarAtom *FirstVA = nullptr;
  for (Atom *A : Vars)
    if (isa<VarAtom>(A)) {
      FirstVA = cast<VarAtom>(A);
      break;
    }

  // Constrains the outer VarAtom to WILD. Inner pointer levels are
  // implicitly WILD because of implication constraints.
  if (FirstVA)
    CS.addConstraint(CS.createGeq(FirstVA, CS.getWild(), Rsn, PL, true));

  if (FV)
    FV->constrainToWild(CS, Rsn, PL);
}

void PointerVariableConstraint::constrainOuterTo(Constraints &CS, ConstAtom *C,
                                                 bool DoLB) {
  assert(C == CS.getPtr() || C == CS.getArr() || C == CS.getNTArr());

  if (Vars.size() > 0) {
    Atom *A = *Vars.begin();
    if (VarAtom *VA = dyn_cast<VarAtom>(A)) {
      if (DoLB)
        CS.addConstraint(CS.createGeq(VA, C, false));
      else
        CS.addConstraint(CS.createGeq(C, VA, false));
    } else if (ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
      if (DoLB) {
        if (*CA < *C) {
          llvm::errs() << "Warning: " << CA->getStr() << " not less than "
                       << C->getStr() << "\n";
          assert(CA == CS.getWild()); // definitely bogus if not
        }
      } else if (*C < *CA) {
        llvm::errs() << "Warning: " << C->getStr() << " not less than "
                     << CA->getStr() << "\n";
        assert(CA == CS.getWild()); // definitely bogus if not
      }
    }
  }
}

bool PointerVariableConstraint::anyArgumentIsWild(const EnvironmentMap &E) {
  for (auto *ArgVal : ArgumentConstraints) {
    if (!ArgVal->isChecked(E)) {
      return true;
    }
  }
  return false;
}

bool PointerVariableConstraint::anyChanges(const EnvironmentMap &E) const {
  // If it was not checked in the input, then it has changed if it now has a
  // checked type.
  bool PtrChanged = false;

  // Are there any non-WILD pointers?
  for (unsigned I = 0; I < Vars.size(); I++) {
    const Atom *VA = Vars[I];
    const ConstAtom *CA = getSolution(VA, E);
    assert(CA != nullptr && "Atom should be either const or var");
    bool OriginallyChecked = isa<ConstAtom>(VA);

    // Atom has changed if it was not originally checked, and it did not solve
    // to WILD. The pointer has changed if the atom has changed.
    bool AtomChanged = !OriginallyChecked && !isa<WildAtom>(CA);
    PtrChanged |= AtomChanged;
  }

  if (FV)
    PtrChanged |= FV->anyChanges(E);

  return PtrChanged;
}

PVConstraint *PointerVariableConstraint::getCopy(Constraints &CS) {
  return new PointerVariableConstraint(this, CS);
}

const ConstAtom *
PointerVariableConstraint::getSolution(const Atom *A,
                                       const EnvironmentMap &E) const {
  const ConstAtom *CS = nullptr;
  if (const ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
    CS = CA;
  } else if (const VarAtom *VA = dyn_cast<VarAtom>(A)) {
    // If this is a VarAtom?, we need to fetch from solution i.e., environment.
    CS = E.at(const_cast<VarAtom *>(VA)).first;
  }
  assert(CS != nullptr && "Atom should be either const or var");
  return CS;
}

bool PointerVariableConstraint::hasWild(const EnvironmentMap &E,
                                        int AIdx) const {
  int VarIdx = 0;
  for (const auto &C : Vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<WildAtom>(CS))
      return true;
    if (VarIdx == AIdx)
      break;
    VarIdx++;
  }

  if (FV)
    return FV->hasWild(E, AIdx);

  return false;
}

bool PointerVariableConstraint::hasArr(const EnvironmentMap &E,
                                       int AIdx) const {
  int VarIdx = 0;
  for (const auto &C : Vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<ArrAtom>(CS))
      return true;
    if (VarIdx == AIdx)
      break;
    VarIdx++;
  }

  if (FV)
    return FV->hasArr(E, AIdx);

  return false;
}

bool PointerVariableConstraint::hasNtArr(const EnvironmentMap &E,
                                         int AIdx) const {
  int VarIdx = 0;
  for (const auto &C : Vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<NTArrAtom>(CS))
      return true;
    if (VarIdx == AIdx)
      break;
    VarIdx++;
  }

  if (FV)
    return FV->hasNtArr(E, AIdx);

  return false;
}

bool PointerVariableConstraint::isTopCvarUnsizedArr() const {
  if (ArrSizes.find(0) != ArrSizes.end()) {
    return ArrSizes.at(0).first != O_SizedArray;
  }
  return true;
}

bool PointerVariableConstraint::hasSomeSizedArr() const {
  for (auto &AS : ArrSizes) {
    if (AS.second.first == O_SizedArray || AS.second.second == O_UnSizedArray) {
      return true;
    }
  }
  return false;
}

bool PointerVariableConstraint::solutionEqualTo(
    Constraints &CS, const ConstraintVariable *CV) const {
  bool Ret = false;
  if (CV != nullptr) {
    if (const auto *PV = dyn_cast<PVConstraint>(CV)) {
      auto &OthCVars = PV->Vars;
      if (IsGeneric || PV->IsGeneric || Vars.size() == OthCVars.size()) {
        Ret = true;

        auto I = Vars.begin();
        auto J = OthCVars.begin();
        // Special handling for zero width arrays so they can compare equal to
        // ARR or PTR.
        if (IsZeroWidthArray) {
          assert(I != Vars.end() && "Zero width array cannot be base type.");
          assert("Zero width arrays should be encoded as PTR." &&
                 CS.getAssignment(*I) == CS.getPtr());
          ConstAtom *JAtom = CS.getAssignment(*J);
          // Zero width array can compare as either ARR or PTR
          if (JAtom != CS.getArr() && JAtom != CS.getPtr())
            Ret = false;
          ++I;
          ++J;
        }
        // Compare Vars to see if they are same.
        while (Ret && I != Vars.end() && J != OthCVars.end()) {
          if (CS.getAssignment(*I) != CS.getAssignment(*J)) {
            Ret = false;
            break;
          }
          ++I;
          ++J;
        }

        if (Ret) {
          FVConstraint *OtherFV = PV->getFV();
          if (FV != nullptr && OtherFV != nullptr) {
            Ret = FV->solutionEqualTo(CS, OtherFV);
          } else if (FV != nullptr || OtherFV != nullptr) {
            // One of them has FV null.
            Ret = false;
          }
        }
      }
    }
  }
  return Ret;
}

void FunctionVariableConstraint::print(raw_ostream &O) const {
  O << "( ";
  ReturnVar->print(O);
  O << " )";
  O << " " << Name << " ";
  for (const auto &I : ParamVars) {
    O << "( ";
    I->print(O);
    O << " )";
  }
}

void FunctionVariableConstraint::dumpJson(raw_ostream &O) const {
  O << "{\"FunctionVar\":{\"ReturnVar\":[";
  bool AddComma = false;
  ReturnVar->dumpJson(O);
  O << "], \"name\":\"" << Name << "\", ";
  O << "\"Parameters\":[";
  AddComma = false;
  for (const auto &I : ParamVars) {
    if (AddComma) {
      O << ",\n";
    }
    O << "[";
    I->dumpJson(O);
    O << "]";
    AddComma = true;
  }
  O << "]";
  O << "}}";
}

bool FunctionVariableConstraint::hasItype() const {
  return ReturnVar->hasItype();
}

bool FunctionVariableConstraint::solutionEqualTo(
    Constraints &CS, const ConstraintVariable *CV) const {
  bool Ret = false;
  if (CV != nullptr) {
    if (const auto *OtherFV = dyn_cast<FVConstraint>(CV)) {
      Ret = (numParams() == OtherFV->numParams());
      Ret = Ret && ReturnVar->solutionEqualTo(CS, OtherFV->ReturnVar);
      for (unsigned I = 0; I < numParams(); I++) {
        Ret =
            Ret && getParamVar(I)->solutionEqualTo(CS, OtherFV->getParamVar(I));
      }
    }
  }
  return Ret;
}

std::string FunctionVariableConstraint::mkString(const EnvironmentMap &E,
                                                 bool EmitName, bool ForItype,
                                                 bool EmitPointee, bool UnmaskTypedef) const {
  std::string Ret = "";
  Ret = ReturnVar->mkString(E);
  Ret = Ret + "(";
  std::vector<std::string> ParmStrs;
  for (const auto &I : this->ParamVars) {
    std::string ParmStr = I->getRewritableOriginalTy() + I->getName();
    if (I->anyChanges(E))
      ParmStr = I->mkString(E);
    ParmStrs.push_back(ParmStr);
  }

  if (ParmStrs.size() > 0) {
    std::ostringstream Ss;

    std::copy(ParmStrs.begin(), ParmStrs.end() - 1,
              std::ostream_iterator<std::string>(Ss, ", "));
    Ss << ParmStrs.back();

    Ret = Ret + Ss.str() + ")";
  } else {
    Ret = Ret + "void)";
  }

  return Ret;
}

// Reverses the direction of CA for function subtyping
//   TODO: function pointers forced to be equal right now
//static ConsAction neg(ConsAction CA) {
//  switch (CA) {
//  case Safe_to_Wild: return Wild_to_Safe;
//  case Wild_to_Safe: return Safe_to_Wild;
//  case Same_to_Same: return Same_to_Same;
//  }
//  // Silencing the compiler.
//  assert(false && "Can never reach here.");
//  return Same_to_Same;
//}

// CA |- R <: L
// Action depends on the kind of constraint (checked, ptyp),
//   which is inferred from the atom type
static void createAtomGeq(Constraints &CS, Atom *L, Atom *R, std::string &Rsn,
                          PersistentSourceLoc *PSL, ConsAction CAct,
                          bool DoEqType) {
  ConstAtom *CAL, *CAR;
  VarAtom *VAL, *VAR;
  ConstAtom *Wild = CS.getWild();

  CAL = clang::dyn_cast<ConstAtom>(L);
  CAR = clang::dyn_cast<ConstAtom>(R);
  VAL = clang::dyn_cast<VarAtom>(L);
  VAR = clang::dyn_cast<VarAtom>(R);

  // Check constant atom relationships hold
  if (CAR != nullptr && CAL != nullptr) {
    if (DoEqType) { // check equality, no matter the atom
      assert(*CAR == *CAL && "Invalid: RHS ConstAtom != LHS ConstAtom");
    } else {
      if (CAL != Wild && CAR != Wild) { // pType atom, disregard CAct
        assert(!(*CAL < *CAR) && "Invalid: LHS ConstAtom < RHS ConstAtom");
      } else { // checked atom (Wild/Ptr); respect CAct
        switch (CAct) {
        case Same_to_Same:
          assert(*CAR == *CAL && "Invalid: RHS ConstAtom != LHS ConstAtom");
          break;
        case Safe_to_Wild:
          assert(!(*CAL < *CAR) && "LHS ConstAtom < RHS ConstAtom");
          break;
        case Wild_to_Safe:
          assert(!(*CAR < *CAL) && "RHS ConstAtom < LHS ConstAtom");
          break;
        }
      }
    }
  } else if (VAL != nullptr && VAR != nullptr) {
    switch (CAct) {
    case Same_to_Same:
      // Equality for checked.
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      // Not for ptyp.
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      // Unless indicated.
      if (DoEqType)
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      break;
    case Safe_to_Wild:
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (DoEqType) {
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    case Wild_to_Safe:
      if (!DisableRDs) {
        // Note: reversal.
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      } else {
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      }
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (DoEqType) {
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    }
  } else {
    // This should be a checked/unchecked constraint.
    if (CAL == Wild || CAR == Wild) {
      switch (CAct) {
      case Same_to_Same:
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        break;
      case Safe_to_Wild:
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        if (DoEqType)
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        break;
      case Wild_to_Safe:
        if (!DisableRDs) {
          // Note: reversal.
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        } else {
          CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        }
        if (DoEqType)
          CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        break;
      }
    } else {
      // This should be a pointer-type constraint.
      switch (CAct) {
      case Same_to_Same:
      case Safe_to_Wild:
      case Wild_to_Safe:
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
        if (DoEqType)
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
        break;
      }
    }
  }
}

// Generate constraints according to CA |- RHS <: LHS.
// If doEqType is true, then also do CA |- LHS <: RHS.
void constrainConsVarGeq(ConstraintVariable *LHS, ConstraintVariable *RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey) {

  // If one of the constraint is NULL, make the other constraint WILD.
  // This can happen when a non-function pointer gets assigned to
  // a function pointer.
  if (LHS == nullptr || RHS == nullptr) {
    std::string Rsn = "Assignment a non-pointer to a pointer";
    if (LHS != nullptr) {
      LHS->constrainToWild(CS, Rsn, PL);
    }
    if (RHS != nullptr) {
      RHS->constrainToWild(CS, Rsn, PL);
    }
    return;
  }

  if (RHS->getKind() == LHS->getKind()) {
    if (FVConstraint *FCLHS = dyn_cast<FVConstraint>(LHS)) {
      if (FVConstraint *FCRHS = dyn_cast<FVConstraint>(RHS)) {

        // This is an assignment between function pointer and
        // function pointer or a function.
        // Force past/future callers of function to use equality constraints.
        FCLHS->equateArgumentConstraints(*Info);
        FCRHS->equateArgumentConstraints(*Info);

        // Constrain the return values covariantly.
        // FIXME: Make neg(CA) here? Function pointers equated
        constrainConsVarGeq(FCLHS->getReturnVar(), FCRHS->getReturnVar(), CS,
                            PL, Same_to_Same, DoEqType, Info, HandleBoundsKey);

        // Constrain the parameters contravariantly.
        if (FCLHS->numParams() == FCRHS->numParams()) {
          for (unsigned I = 0; I < FCLHS->numParams(); I++) {
            ConstraintVariable *LHSV = FCLHS->getParamVar(I);
            ConstraintVariable *RHSV = FCRHS->getParamVar(I);
            // FIXME: Make neg(CA) here? Now: Function pointers equated
            constrainConsVarGeq(RHSV, LHSV, CS, PL, Same_to_Same, DoEqType,
                                Info, HandleBoundsKey);
          }
        } else {
          // Constrain both to be top.
          std::string Rsn =
              "Assigning from:" + FCRHS->getName() + " to " + FCLHS->getName();
          RHS->constrainToWild(CS, Rsn, PL);
          LHS->constrainToWild(CS, Rsn, PL);
        }
      } else {
        llvm_unreachable("impossible");
      }
    } else if (PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS)) {
      if (PVConstraint *PCRHS = dyn_cast<PVConstraint>(RHS)) {

        // Add assignment to bounds info graph.
        if (HandleBoundsKey && PCLHS->hasBoundsKey() && PCRHS->hasBoundsKey()) {
          Info->getABoundsInfo().addAssignment(PCLHS->getBoundsKey(),
                                               PCRHS->getBoundsKey());
        }

        std::string Rsn = "";
        // This is to handle function subtyping. Try to add LHS and RHS
        // to each others argument constraints.
        PCLHS->addArgumentConstraint(PCRHS, *Info);
        PCRHS->addArgumentConstraint(PCLHS, *Info);
        // Element-wise constrain PCLHS and PCRHS to be equal.
        CAtoms CLHS = PCLHS->getCvars();
        CAtoms CRHS = PCRHS->getCvars();

        // Only generate constraint if LHS is not a base type.
        if (CLHS.size() != 0) {
          if (CLHS.size() == CRHS.size() || PCLHS->getIsGeneric() ||
              PCRHS->getIsGeneric()) {
            unsigned Max = std::max(CLHS.size(), CRHS.size());
            for (unsigned N = 0; N < Max; N++) {
              Atom *IAtom = PCLHS->getAtom(N, CS);
              Atom *JAtom = PCRHS->getAtom(N, CS);
              if (IAtom == nullptr || JAtom == nullptr)
                break;

              // Get outermost pointer first, using current ConsAction.
              if (N == 0)
                createAtomGeq(CS, IAtom, JAtom, Rsn, PL, CA, DoEqType);
              else {
                // Now constrain the inner ones as equal.
                createAtomGeq(CS, IAtom, JAtom, Rsn, PL, CA, true);
              }
            }
            // Unequal sizes means casting from (say) T** to T*; not safe.
            // unless assigning to a generic type.
          } else {
            // Constrain both to be top.
            std::string Rsn = "Assigning from:" + std::to_string(CRHS.size()) +
                              " depth pointer to " +
                              std::to_string(CLHS.size()) + " depth pointer.";
            PCLHS->constrainToWild(CS, Rsn, PL);
            PCRHS->constrainToWild(CS, Rsn, PL);
          }
          // Equate the corresponding FunctionConstraint.
          constrainConsVarGeq(PCLHS->getFV(), PCRHS->getFV(), CS, PL, CA,
                              DoEqType, Info, HandleBoundsKey);
        }
      } else
        llvm_unreachable("impossible");
    } else
      llvm_unreachable("unknown kind");
  } else {
    // Assigning from a function variable to a pointer variable?
    PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS);
    FVConstraint *FCRHS = dyn_cast<FVConstraint>(RHS);
    if (PCLHS && FCRHS) {
      if (FVConstraint *FCLHS = PCLHS->getFV()) {
        constrainConsVarGeq(FCLHS, FCRHS, CS, PL, CA, DoEqType, Info,
                            HandleBoundsKey);
      } else {
        std::string Rsn = "Function:" + FCRHS->getName() +
                          " assigned to non-function pointer.";
        LHS->constrainToWild(CS, Rsn, PL);
        RHS->constrainToWild(CS, Rsn, PL);
      }
    } else {
      // Constrain everything in both to wild.
      std::string Rsn = "Assignment to functions from variables";
      LHS->constrainToWild(CS, Rsn, PL);
      RHS->constrainToWild(CS, Rsn, PL);
    }
  }
}

void constrainConsVarGeq(ConstraintVariable *LHS, const CVarSet &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey) {
  for (const auto &J : RHS)
    constrainConsVarGeq(LHS, J, CS, PL, CA, DoEqType, Info, HandleBoundsKey);
}

// Given an RHS and a LHS, constrain them to be equal.
void constrainConsVarGeq(const CVarSet &LHS, const CVarSet &RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool DoEqType, ProgramInfo *Info,
                         bool HandleBoundsKey) {
  for (const auto &I : LHS)
    constrainConsVarGeq(I, RHS, CS, PL, CA, DoEqType, Info, HandleBoundsKey);
}

// True if [C] is a PVConstraint that contains at least one Atom (i.e.,
//   it represents a C pointer).
bool isAValidPVConstraint(const ConstraintVariable *C) {
  if (const auto *PV = dyn_cast_or_null<PVConstraint>(C))
    return !PV->getCvars().empty();
  return false;
}

// Replace CVars and ArgumentConstraints with those in [FromCV].
void PointerVariableConstraint::brainTransplant(ConstraintVariable *FromCV,
                                                ProgramInfo &I) {
  PVConstraint *From = dyn_cast<PVConstraint>(FromCV);
  assert(From != nullptr);
  CAtoms CFrom = From->getCvars();
  assert(Vars.size() == CFrom.size());
  if (From->hasBoundsKey()) {
    // If this has bounds key!? Then do brain transplant of
    // bound keys as well.
    if (hasBoundsKey())
      I.getABoundsInfo().brainTransplant(getBoundsKey(), From->getBoundsKey());

    ValidBoundsKey = From->hasBoundsKey();
    BKey = From->getBoundsKey();
  }
  Vars = CFrom; // FIXME: structural copy? By reference?
  ArgumentConstraints = From->getArgumentConstraints();
  if (FV) {
    assert(From->FV);
    FV->brainTransplant(From->FV, I);
  }
}

void PointerVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV,
                                                 ProgramInfo &Info,
                                                 std::string &ReasonFailed) {
  PVConstraint *From = dyn_cast<PVConstraint>(FromCV);
  std::vector<Atom *> NewVatoms;
  CAtoms CFrom = From->getCvars();
  CAtoms::iterator I = Vars.begin();
  CAtoms::iterator J = CFrom.begin();
  if (CFrom.size() != Vars.size()) {
    ReasonFailed = "conflicting types ";
    return;
  }
  while (I != Vars.end()) {
    Atom *IAt = *I;
    Atom *JAt = *J;
    ConstAtom *ICAt = dyn_cast<ConstAtom>(IAt);
    ConstAtom *JCAt = dyn_cast<ConstAtom>(JAt);
    if (JCAt && !ICAt) {
      NewVatoms.push_back(JAt);
    } else {
      NewVatoms.push_back(IAt);
    }
    if (ICAt && JCAt) {
      // Both are ConstAtoms, no need to equate them.

      // Sanity: If both are ConstAtoms and they are not same,
      // Make sure that current ConstAtom is WILD. This ensure that
      // we are moving towards checked types.
      if (ICAt != JCAt) {
        if (!dyn_cast<WildAtom>(ICAt)) {
          assert(false && "Should be same checked types");
        }
      }
    }
    ++I;
    ++J;
  }
  assert(Vars.size() == NewVatoms.size() && "Merging Failed");
  Vars = NewVatoms;
  if (!From->ItypeStr.empty())
    ItypeStr = From->ItypeStr;
  if (FV) {
    assert(From->FV);
    FV->mergeDeclaration(From->FV, Info, ReasonFailed);
  }
}

Atom *PointerVariableConstraint::getAtom(unsigned AtomIdx, Constraints &CS) {
  if (AtomIdx < Vars.size()) {
    // If index is in bounds, just return the atom.
    return Vars[AtomIdx];
  }
  if (IsGeneric && AtomIdx == Vars.size()) {
    // Polymorphic types don't know how "deep" their pointers are beforehand so,
    // we need to create new atoms for new pointer levels on the fly.
    std::string Stars(Vars.size(), '*');
    Atom *A = CS.getFreshVar(Name + Stars, VarAtom::V_Other);
    Vars.push_back(A);
    return A;
  }
  return nullptr;
}

// Brain Transplant params and returns in [FromCV], recursively.
void FunctionVariableConstraint::brainTransplant(ConstraintVariable *FromCV,
                                                 ProgramInfo &I) {
  FVConstraint *From = dyn_cast<FVConstraint>(FromCV);
  assert(From != nullptr);
  // Transplant returns.
  ReturnVar->brainTransplant(From->ReturnVar, I);
  // Transplant params.
  if (numParams() == From->numParams()) {
    for (unsigned J = 0; J < From->numParams(); J++) {
      ConstraintVariable *FromVar = From->getParamVar(J);
      ConstraintVariable *Var = getParamVar(J);
      Var->brainTransplant(FromVar, I);
    }
  } else if (numParams() != 0 && From->numParams() == 0) {
    auto &CS = I.getConstraints();
    const std::vector<ParamDeferment> &Defers = From->getDeferredParams();
    assert(getDeferredParams().size() == 0);
    for (auto Deferred : Defers) {
      assert(numParams() == Deferred.PS.size());
      for (unsigned J = 0; J < Deferred.PS.size(); J++) {
        ConstraintVariable *ParamDC = getParamVar(J);
        CVarSet ArgDC = Deferred.PS[J];
        constrainConsVarGeq(ParamDC, ArgDC, CS, &(Deferred.PL), Wild_to_Safe,
                            false, &I);
      }
    }
  } else {
    llvm_unreachable("Brain Transplant on empty params");
  }
}

void FunctionVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV,
                                                  ProgramInfo &I,
                                                  std::string &ReasonFailed) {
  // `this`: is the declaration the tool saw first
  // `FromCV`: is the declaration seen second, it cannot have defered
  // constraints
  FVConstraint *From = dyn_cast<FVConstraint>(FromCV);
  assert(From != nullptr);
  assert(From->getDeferredParams().size() == 0);
  // Transplant returns.
  // Our first sanity check is to ensure the function's returns type-check
  ReturnVar->mergeDeclaration(From->ReturnVar, I, ReasonFailed);
  if (ReasonFailed != "") {
    ReasonFailed += "for return value";
    return;
  }

  if (From->numParams() == 0) {
    // From is an untyped declaration, and adds no information
    return;
  }
  if (this->numParams() == 0) {
    // This is an untyped declaration, we need to perform a transplant
    From->brainTransplant(this, I);
  } else {
    // Standard merge
    if (this->numParams() != From->numParams()) {
      ReasonFailed = "differing number of arguments";
      return;
    }
    for (unsigned J = 0; J < From->numParams(); J++) {
      auto *FromVar = From->getParamVar(J);
      auto *Var = getParamVar(J);
      Var->mergeDeclaration(FromVar, I, ReasonFailed);
      if (ReasonFailed != "") {
        ReasonFailed += "for parameter " + std::to_string(J);
        return;
      }
    }
  }
}

void FunctionVariableConstraint::addDeferredParams(PersistentSourceLoc PL,
                                                   std::vector<CVarSet> Ps) {
  ParamDeferment P = {PL, Ps};
  DeferredParams.push_back(P);
}

bool FunctionVariableConstraint::getIsOriginallyChecked() const {
  return ReturnVar->getIsOriginallyChecked();
}

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

#include "llvm/ADT/StringSwitch.h"
#include "clang/Lex/Lexer.h"
#include <sstream>

#include "clang/CConv/ConstraintVariables.h"
#include "clang/CConv/ProgramInfo.h"
#include "clang/CConv/CCGlobalOptions.h"

using namespace clang;

std::string ConstraintVariable::getRewritableOriginalTy() {
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

PointerVariableConstraint *
PointerVariableConstraint::getWildPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalWildPV = nullptr;
  if (GlobalWildPV == nullptr) {
    CAtoms NewVA;
    NewVA.push_back(CS.getWild());
    GlobalWildPV =
        new PVConstraint(NewVA, "unsigned", "wildvar", nullptr, false, false, "");
  }
  return GlobalWildPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getPtrPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalPtrPV = nullptr;
  if (GlobalPtrPV == nullptr) {
    CAtoms NewVA;
    NewVA.push_back(CS.getPtr());
    GlobalPtrPV =
        new PVConstraint(NewVA, "unsigned", "ptrvar", nullptr, false, false, "");
  }
  return GlobalPtrPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getNonPtrPVConstraint(Constraints &CS) {
  static PointerVariableConstraint *GlobalNonPtrPV = nullptr;
  if (GlobalNonPtrPV == nullptr) {
    CAtoms NewVA; // empty -- represents a base type
    GlobalNonPtrPV =
        new PVConstraint(NewVA, "unsigned", "basevar", nullptr, false, false, "");
  }
  return GlobalNonPtrPV;
}

PointerVariableConstraint *
PointerVariableConstraint::getNamedNonPtrPVConstraint(StringRef name,
                                                      Constraints &CS) {
  CAtoms NewVA; // empty -- represents a base type
  return new PVConstraint(NewVA, "unsigned", name, nullptr, false, false, "");
}

PointerVariableConstraint::
    PointerVariableConstraint(PointerVariableConstraint *Ot,
                              Constraints &CS) :
    ConstraintVariable(ConstraintVariable::PointerVariable,
                       Ot->BaseType, Ot->Name),
    FV(nullptr), partOFFuncPrototype(Ot->partOFFuncPrototype) {
  this->arrSizes = Ot->arrSizes;
  this->ArrPresent = Ot->ArrPresent;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  // Make copy of the vars only for VarAtoms.
  for (auto *CV : Ot->vars) {
    if (ConstAtom *CA = dyn_cast<ConstAtom>(CV)) {
      this->vars.push_back(CA);
    }
    if (VarAtom *VA = dyn_cast<VarAtom>(CV)) {
      this->vars.push_back(CS.getFreshVar(VA->getName(), VA->getVarKind()));
    }
  }
  if (Ot->FV != nullptr) {
    this->FV = dyn_cast<FVConstraint>(Ot->FV->getCopy(CS));
  }
  this->Parent = Ot;
  // We need not initialize other members.
}

PointerVariableConstraint::PointerVariableConstraint(DeclaratorDecl *D,
                                                     Constraints &CS,
                                                     const ASTContext &C) :
        PointerVariableConstraint(D->getType(), D, D->getName(),
                                  CS, C) { }

PointerVariableConstraint::PointerVariableConstraint(const QualType &QT,
                                                     DeclaratorDecl *D,
                                                     std::string N,
                                                     Constraints &CS,
                                                     const ASTContext &C,
                                                     std::string *inFunc) :
        ConstraintVariable(ConstraintVariable::PointerVariable,
                           tyToStr(QT.getTypePtr()),N), FV(nullptr),
        partOFFuncPrototype(inFunc != nullptr), Parent(nullptr)
{
  QualType QTy = QT;
  const Type *Ty = QTy.getTypePtr();
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

  bool isDeclTy = false;
  if (D != nullptr) {
    isDeclTy = D->getType() == QT; // If false, then QT may be D's return type
    if (InteropTypeExpr *ITE = D->getInteropTypeExpr()) {
      // External variables can also have itype.
      // Check if the provided declaration is an external
      // variable.
      // For functions, check to see that if we are analyzing
      // function return types.
      bool AnalyzeITypeExpr = isDeclTy;
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
          auto &SM = C.getSourceManager();
          auto LO = C.getLangOpts();
          llvm::StringRef Srctxt =
              Lexer::getSourceText(CharSourceRange::getTokenRange(R), SM, LO);
          ItypeStr = Srctxt.str();
          assert(ItypeStr.size() > 0);
        }
      }
    }
  }

  bool VarCreated = false;
  bool isArr = false;
  uint32_t TypeIdx = 0;
  std::string Npre = inFunc ? ((*inFunc)+":") : "";
  VarAtom::VarKind VK =
      inFunc ? (N == RETVAR ? VarAtom::V_Return : VarAtom::V_Param)
             : VarAtom::V_Other;

  while (Ty->isPointerType() || Ty->isArrayType()) {
    // Is this a VarArg type?
    std::string TyName = tyToStr(Ty);
    if (isVarArgType(TyName)) {
      // Variable number of arguments. Make it WILD.
      vars.push_back(CS.getWild());
      VarCreated = true;
      break;
    }

    if (Ty->isDeclaredCheckedPointerType()) {
      ConstAtom *CAtom = nullptr;
      if (Ty->isDeclaredCheckedPointerNtArrayType()) {
        // This is an NT array type.
        CAtom = CS.getNTArr();
      } else if (Ty->isDeclaredCheckedPointerArrayType()) {
        // This is an array type.
        CAtom = CS.getArr();
      } else if (Ty->isDeclaredCheckedPointerPtrType()) {
        // This is a regular checked pointer.
        CAtom = CS.getPtr();
      }
      VarCreated = true;
      assert(CAtom != nullptr && "Unable to find the type "
                                 "of the checked pointer.");
      vars.push_back(CAtom);
    }

    if (Ty->isArrayType() || Ty->isIncompleteArrayType()) {
      ArrPresent = isArr = true;

      // See if there is a constant size to this array type at this position.
      if (const ConstantArrayType *CAT = dyn_cast<ConstantArrayType>(Ty)) {
        arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(
                O_SizedArray,CAT->getSize().getZExtValue());
      } else {
        arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(
                O_UnSizedArray,0);
      }

      // Boil off the typedefs in the array case.
      while (const TypedefType *TydTy = dyn_cast<TypedefType>(Ty)) {
        QTy = TydTy->desugar();
        Ty = QTy.getTypePtr();
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
      if (QTy.isConstQualified())
        QualMap.insert(
                std::pair<uint32_t, Qualification>(TypeIdx,
                                                    ConstQualification));

      arrSizes[TypeIdx] = std::pair<OriginalArrType,uint64_t>(O_Pointer,0);

      // Iterate.
      QTy = QTy.getSingleStepDesugaredType(C);
      QTy = QTy.getTypePtr()->getPointeeType();
      Ty = QTy.getTypePtr();
    }

    // This type is not a constant atom. We need to create a VarAtom for this.

    if (!VarCreated) {
      VarAtom *VA = CS.getFreshVar(Npre + N, VK);
      vars.push_back(VA);
      if (isArr)
        CS.addConstraint(CS.createGeq(CS.getArr(), VA, false));
    }

    // Prepare for next level of pointer
    VarCreated = false;
    isArr = false;
    TypeIdx++;
    Npre = Npre + "*";
    VK = VarAtom::V_Other; // only the outermost pointer considered a param/return
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
    FV = new FVConstraint(Ty, isDeclTy ? D : nullptr,
                          (IsTypedef ? "" : N), CS, C);

  BaseType = tyToStr(Ty);

  bool IsWild = isVarArgType(BaseType) || isTypeHasVoid(QT);
  if (IsWild) {
    std::string Rsn = "Default Var arg list type.";
    if (D && hasVoidType(D))
      Rsn = "Default void* type";
    // TODO: Github issue #61: improve handling of types for
    // Variable arguments.
    for (const auto &V : vars)
      if (VarAtom *VA = dyn_cast<VarAtom>(V))
        CS.addConstraint(CS.createGeq(VA, CS.getWild(), Rsn));
  }

  // Add qualifiers.
  if (QTy.isConstQualified()) {
    BaseType = "const " + BaseType;
  }

  // Here lets add implication that if outer pointer is WILD
  // then make the inner pointers WILD too.
  if (vars.size() > 1) {
    bool UsedPrGeq = false;
    for (auto VI=vars.begin(), VE=vars.end(); VI != VE; VI++) {
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

void PointerVariableConstraint::print(raw_ostream &O) const {
  O << "{ ";
  for (const auto &I : vars) {
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

void PointerVariableConstraint::dump_json(llvm::raw_ostream &O) const {
  O << "{\"PointerVar\":{";
  O << "\"Vars\":[";
  bool addComma = false;
  for (const auto &I : vars) {
    if (addComma) {
      O << ",";
    }
    I->dump_json(O);

    addComma = true;
  }
  O << "], \"name\":\"" << getName() << "\"";
  if (FV) {
    O << ", \"FunctionVariable\":";
    FV->dump_json(O);
  }
  O << "}}";

}

void PointerVariableConstraint::getQualString(uint32_t TypeIdx,
                                              std::ostringstream &Ss) {
  std::map<ConstraintKey, Qualification>::iterator Q = QualMap.find(TypeIdx);
  if (Q != QualMap.end())
    if (Q->second == ConstQualification)
      Ss << "const ";
}

bool PointerVariableConstraint::emitArraySize(std::ostringstream &Pss,
                                              uint32_t TypeIdx,
                                              bool &EmitName,
                                              bool &EmittedCheckedAnnotation,
                                              bool Nt) {
  bool Ret = false;
  if (ArrPresent) {
    auto i = arrSizes.find(TypeIdx);
    assert(i != arrSizes.end());
    OriginalArrType Oat = i->second.first;
    uint64_t Oas = i->second.second;

    if (EmitName == false) {
      EmitName = true;
      Pss << getName();
    }

    switch (Oat) {
      case O_SizedArray:
        if (!EmittedCheckedAnnotation) {
          Pss << (Nt ? " _Nt_checked" : " _Checked");
          EmittedCheckedAnnotation = true;
        }
        Pss << "[" << Oas << "]";
        Ret = true;
        break;
      case O_UnSizedArray:
        Pss << "[]";
        Ret = true;
        break;
      default: break;
    }
    return Ret;
  }
  return Ret;
}

// Mesh resolved constraints with the PointerVariableConstraints set of
// variables and potentially nested function pointer declaration. Produces a
// string that can be replaced in the source code.
std::string
PointerVariableConstraint::mkString(EnvironmentMap &E,
                                    bool EmitName,
                                    bool ForItype) {
  std::ostringstream Ss;
  std::ostringstream Pss;
  unsigned CaratsToAdd = 0;
  bool EmittedBase = false;
  bool EmittedName = false;
  bool EmittedCheckedAnnotation = false;
  if (EmitName == false && hasItype() == false)
    EmittedName = true;
  uint32_t TypeIdx = 0;
  for (const auto &V : vars) {
    ConstAtom *C = nullptr;
    if (ConstAtom *CA = dyn_cast<ConstAtom>(V)) {
      C = CA;
    } else {
      VarAtom *VA = dyn_cast<VarAtom>(V);
      assert(VA != nullptr && "Constraint variable can "
                              "be either constant or VarAtom.");
      C = E[VA].first;
    }
    assert(C != nullptr);

    Atom::AtomKind K = C->getKind();

    // If this is not an itype
    // make this wild as it can hold any pointer type.
    if (!ForItype && BaseType == "void")
      K = Atom::A_Wild;

    switch (K) {
      case Atom::A_Ptr:
        getQualString(TypeIdx, Ss);

        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (hasItype() == false) {
          EmittedBase = false;
          Ss << "_Ptr<";
          CaratsToAdd++;
          break;
        }
        LLVM_FALLTHROUGH;
    case Atom::A_Arr:
        // If this is an array.
        getQualString(TypeIdx, Ss);
        // If it's an Arr, then the character we substitute should
        // be [] instead of *, IF, the original type was an array.
        // And, if the original type was a sized array of size K.
        // we should substitute [K].
        if (emitArraySize(Pss, TypeIdx, EmittedName,
                          EmittedCheckedAnnotation, false))
          break;
        // We need to check and see if this level of variable
        // is constrained by a bounds safe interface. If it is,
        // then we shouldn't re-write it.
        if (hasItype() == false) {
          EmittedBase = false;
          Ss << "_Array_ptr<";
          CaratsToAdd++;
          break;
        }
        LLVM_FALLTHROUGH;
      case Atom::A_NTArr:

        if (emitArraySize(Pss, TypeIdx, EmittedName,
                          EmittedCheckedAnnotation, true))
          break;
        // This additional check is to prevent fall-through from the array.
        if (K == Atom::A_NTArr) {
          // If this is an NTArray.
          getQualString(TypeIdx, Ss);

          // We need to check and see if this level of variable
          // is constrained by a bounds safe interface. If it is,
          // then we shouldn't re-write it.
          if (hasItype() == false) {
            EmittedBase = false;
            Ss << "_Nt_array_ptr<";
            CaratsToAdd++;
            break;
          }
        }
        LLVM_FALLTHROUGH;
      // If there is no array in the original program, then we fall through to
      // the case where we write a pointer value.
      case Atom::A_Wild:
        if (EmittedBase) {
          Ss << "*";
        } else {
          assert(BaseType.size() > 0);
          EmittedBase = true;
          if (FV) {
            Ss << FV->mkString(E);
          } else {
            Ss << BaseType << "*";
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

  if (EmittedBase == false) {
    // If we have a FV pointer, then our "base" type is a function pointer.
    // type.
    if (FV) {
      Ss << FV->mkString(E);
    } else {
      Ss << BaseType;
    }
  }

  // Push carats onto the end of the string.
  for (unsigned i = 0; i < CaratsToAdd; i++) {
    Ss << ">";
  }

  // No space after itype.
  if (!ForItype)
    Ss << " ";

  std::string FinalDec;
  if (EmittedName == false) {
    if (getName() != RETVAR)
      Ss << getName();
    FinalDec = Ss.str();
  } else {
    FinalDec = Ss.str() + Pss.str();
  }

  return FinalDec;
}

bool PVConstraint::addArgumentConstraint(ConstraintVariable *DstCons,
                                         ProgramInfo &Info) {
  if (this->Parent == nullptr) {
    bool RetVal = false;
    if (isPartOfFunctionPrototype()) {
      RetVal = argumentConstraints.insert(DstCons).second;
      if (RetVal && this->HasEqArgumentConstraints) {
        constrainConsVarGeq(DstCons, this, Info.getConstraints(), nullptr,
                            Same_to_Same, true, &Info);
      }
    }
    return RetVal;
  }
  return this->Parent->addArgumentConstraint(DstCons, Info);
}

std::set<ConstraintVariable *> &PVConstraint::getArgumentConstraints() {
  return argumentConstraints;
}

FunctionVariableConstraint::
    FunctionVariableConstraint(FunctionVariableConstraint *Ot,
                               Constraints &CS) :
    ConstraintVariable(ConstraintVariable::FunctionVariable,
                       Ot->OriginalType,
                       Ot->getName()) {
  this->IsStatic = Ot->IsStatic;
  this->FileName = Ot->FileName;
  this->Hasbody = Ot->Hasbody;
  this->Hasproto = Ot->Hasproto;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  this->IsFunctionPtr = Ot->IsFunctionPtr;
  this->HasEqArgumentConstraints = Ot->HasEqArgumentConstraints;
  // Copy Return CVs.
  for (auto *Rt : Ot->getReturnVars()) {
    this->returnVars.insert(Rt->getCopy(CS));
  }
  // Make copy of ParameterCVs too.
  for (auto &Pset : Ot->paramVars) {
    std::set<ConstraintVariable *> ParmCVs;
    ParmCVs.clear();
    for (auto *ParmPV : Pset) {
      ParmCVs.insert(ParmPV->getCopy(CS));
    }
    this->paramVars.push_back(ParmCVs);
  }
  this->Parent = Ot;
}

// This describes a function, either a function pointer or a function
// declaration itself. Require constraint variables for each argument and
// return, even those that aren't pointer types, since we may need to
// re-emit the function signature as a type.
FunctionVariableConstraint::FunctionVariableConstraint(DeclaratorDecl *D,
                                                       Constraints &CS,
                                                       const ASTContext &C) :
        FunctionVariableConstraint(D->getType().getTypePtr(), D,
                                   (D->getDeclName().isIdentifier() ?
                                        D->getName() : ""), CS, C)
{ }

FunctionVariableConstraint::FunctionVariableConstraint(const Type *Ty,
                                                       DeclaratorDecl *D,
                                                       std::string N,
                                                       Constraints &CS,
                                                       const ASTContext &Ctx) :
        ConstraintVariable(ConstraintVariable::FunctionVariable,
                           tyToStr(Ty), N), Parent(nullptr)
{
  QualType RT;
  Hasproto = false;
  Hasbody = false;
  FileName = "";
  HasEqArgumentConstraints = false;
  IsFunctionPtr = true;

  // Metadata about function
  FunctionDecl *FD = nullptr;
  if (D) FD = dyn_cast<FunctionDecl>(D);
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
    for (unsigned i = 0; i < FT->getNumParams(); i++) {
      QualType QT = FT->getParamType(i);

      std::string PName = "";
      DeclaratorDecl *ParmVD = nullptr;
      if (FD && i < FD->getNumParams()) {
        ParmVarDecl *PVD = FD->getParamDecl(i);
        if (PVD) {
          ParmVD = PVD;
          PName = PVD->getName();
        }
      }

      std::set<ConstraintVariable *> C;
      C.insert(new PVConstraint(QT, ParmVD, PName, CS, Ctx, &N));
      paramVars.push_back(C);
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
  returnVars.insert(new PVConstraint(RT, D, RETVAR, CS, Ctx, &N));
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 std::string &Rsn) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS, Rsn);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS, Rsn);
}

void FunctionVariableConstraint::constrainToWild(Constraints &CS,
                                                 std::string &Rsn,
                                                 PersistentSourceLoc *PL) {
  for (const auto &V : returnVars)
    V->constrainToWild(CS, Rsn, PL);

  for (const auto &V : paramVars)
    for (const auto &U : V)
      U->constrainToWild(CS, Rsn, PL);
}

bool FunctionVariableConstraint::anyChanges(EnvironmentMap &E) {
  bool f = false;

  for (const auto &C : returnVars)
    f |= C->anyChanges(E);

  return f;
}

bool FunctionVariableConstraint::hasWild(EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasWild(E))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasArr(EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasArr(E))
      return true;

  return false;
}

bool FunctionVariableConstraint::hasNtArr(EnvironmentMap &E)
{
  for (const auto &C : returnVars)
    if (C->hasNtArr(E))
      return true;

  return false;
}

ConstraintVariable *FunctionVariableConstraint::getCopy(Constraints &CS) {
  return new FVConstraint(this, CS);
}

void PVConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }
  HasEqArgumentConstraints = true;
  for (auto *ArgCons : this->argumentConstraints) {
    constrainConsVarGeq(this, ArgCons, Info.getConstraints(), nullptr,
                        Same_to_Same, true, &Info);
  }

  if (this->FV != nullptr) {
    this->FV->equateArgumentConstraints(Info);
  }
}

void
FunctionVariableConstraint::equateFVConstraintVars(
    std::set<ConstraintVariable *> &Cset, ProgramInfo &Info) {
  for (auto *TmpCons : Cset) {
    if (FVConstraint *FVCons = dyn_cast<FVConstraint>(TmpCons)) {
      for (auto &PConSet : FVCons->paramVars) {
        for (auto *PCon : PConSet) {
          PCon->equateArgumentConstraints(Info);
        }
      }
      for (auto *RCon : FVCons->returnVars) {
        RCon->equateArgumentConstraints(Info);
      }
    }
  }
}

void FunctionVariableConstraint::equateArgumentConstraints(ProgramInfo &Info) {
  if (HasEqArgumentConstraints) {
    return;
  }

  HasEqArgumentConstraints = true;
  std::set<ConstraintVariable *> TmpCSet;
  TmpCSet.insert(this);

  // Equate arguments and parameters vars.
  this->equateFVConstraintVars(TmpCSet, Info);

  // Is this not a function pointer?
  if (!IsFunctionPtr) {
    std::set<FVConstraint *> *DefnCons = nullptr;

    // Get appropriate constraints based on whether the function is static or not.
    if (IsStatic) {
      DefnCons = Info.getStaticFuncConstraintSet(Name, FileName);
    } else {
      DefnCons = Info.getExtFuncDefnConstraintSet(Name);
    }
    assert(DefnCons != nullptr);

    // Equate arguments and parameters vars.
    std::set<ConstraintVariable *> TmpDefn;
    TmpDefn.clear();
    TmpDefn.insert(DefnCons->begin(), DefnCons->end());
    this->equateFVConstraintVars(TmpDefn, Info);
  }
}

void PointerVariableConstraint::constrainToWild(Constraints &CS) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, true));
  }

  if (FV)
    FV->constrainToWild(CS);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                std::string &Rsn,
                                                PersistentSourceLoc *PL) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, Rsn, PL, true));
  }

  if (FV)
    FV->constrainToWild(CS, Rsn, PL);
}

void PointerVariableConstraint::constrainToWild(Constraints &CS,
                                                std::string &Rsn) {
  ConstAtom *WA = CS.getWild();
  for (const auto &V : vars) {
    if (VarAtom *VA = dyn_cast<VarAtom>(V))
      CS.addConstraint(CS.createGeq(VA, WA, Rsn, true));
  }

  if (FV)
    FV->constrainToWild(CS, Rsn);
}

// FIXME: Should do some checking here, eventually to make sure
// checked types are respected
void PointerVariableConstraint::constrainOuterTo(Constraints &CS, ConstAtom *C) {
  assert(C == CS.getPtr() || C == CS.getArr() || C == CS.getNTArr());

  if (vars.size() > 0) {
    Atom *A = *vars.begin();
    if (VarAtom *VA = dyn_cast<VarAtom>(A))
      CS.addConstraint(CS.createGeq(C, VA, false));
    else if (ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
      if (*C < *CA) {
        llvm::errs() << "Warning: " << C->getStr() << " not less than " << CA->getStr() <<"\n";
        assert(CA == CS.getWild()); // definitely bogus if not
      }
    }
  }
}

bool PointerVariableConstraint::anyArgumentIsWild(EnvironmentMap &E) {
  for (auto *ArgVal : argumentConstraints) {
    if (!(ArgVal->anyChanges(E))) {
      return true;
    }
  }
  return false;
}

bool PointerVariableConstraint::anyChanges(EnvironmentMap &E) {
  bool Ret = false;

  // Are there any non-WILD pointers?
  for (const auto &C : vars) {
    const ConstAtom *CS = getSolution(C, E);
    assert(CS != nullptr && "Atom should be either const or var");
    Ret |= !(isa<WildAtom>(CS));
  }

  if (FV)
    Ret |= FV->anyChanges(E);

  return Ret;
}

ConstraintVariable *PointerVariableConstraint::getCopy(Constraints &CS) {
  return new PointerVariableConstraint(this, CS);
}

const ConstAtom *
PointerVariableConstraint::getSolution(const Atom *A, EnvironmentMap &E) const {
  const ConstAtom *CS = nullptr;
  if (const ConstAtom *CA = dyn_cast<ConstAtom>(A)) {
    CS = CA;
  } else if (const VarAtom *VA = dyn_cast<VarAtom>(A)) {
    // If this is a VarAtom?, we need ot fetch from solution
    // i.e., environment.
    CS = E[const_cast<VarAtom*>(VA)].first;
  }
  assert(CS != nullptr && "Atom should be either const or var");
  return CS;
}

bool PointerVariableConstraint::hasWild(EnvironmentMap &E)
{
  for (const auto &C : vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<WildAtom>(CS))
      return true;
  }

  if (FV)
    return FV->hasWild(E);

  return false;
}

bool PointerVariableConstraint::hasArr(EnvironmentMap &E)
{
  for (const auto &C : vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<ArrAtom>(CS))
      return true;
  }

  if (FV)
    return FV->hasArr(E);

  return false;
}

bool PointerVariableConstraint::hasNtArr(EnvironmentMap &E)
{
  for (const auto &C : vars) {
    const ConstAtom *CS = getSolution(C, E);
    if (isa<NTArrAtom>(CS))
      return true;
  }

  if (FV)
    return FV->hasNtArr(E);

  return false;
}

bool PointerVariableConstraint::
    solutionEqualTo(Constraints &CS, ConstraintVariable *CV) {
  bool Ret = false;
  if (CV != nullptr) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(CV)) {
      auto &OthCVars = PV->vars;
      if (vars.size() == OthCVars.size()) {
        Ret = true;

        // First compare Vars to see if they are same.
        CAtoms::iterator I = vars.begin();
        CAtoms::iterator J = OthCVars.begin();
        while (I != vars.end()) {
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
  for (const auto &I : returnVars)
    I->print(O);
  O << " )";
  O << " " << Name << " ";
  for (const auto &I : paramVars) {
    O << "( ";
    for (const auto &J : I)
      J->print(O);
    O << " )";
  }
}

void FunctionVariableConstraint::dump_json(raw_ostream &O) const {
  O << "{\"FunctionVar\":{\"ReturnVar\":[";
  bool AddComma = false;
  for (const auto &I : returnVars) {
    if (AddComma) {
      O << ",";
    }
    I->dump_json(O);
  }
  O << "], \"name\":\"" << Name << "\", ";
  O << "\"Parameters\":[";
  AddComma = false;
  for (const auto &I : paramVars) {
    if (I.size() > 0) {
      if (AddComma) {
        O << ",\n";
      }
      O << "[";
      bool InnerComma = false;
      for (const auto &J : I) {
        if (InnerComma) {
          O << ",";
        }
        J->dump_json(O);
        InnerComma = true;
      }
      O << "]";
      AddComma = true;
    }
  }
  O << "]";
  O << "}}";
}

bool FunctionVariableConstraint::hasItype() {
  for (auto &RV : getReturnVars()) {
    if (RV->hasItype()) {
      return true;
    }
  }
  return false;
}

static bool cvSetsSolutionEqualTo(Constraints &CS,
                                std::set<ConstraintVariable *> &CVS1,
                                std::set<ConstraintVariable *> &CVS2) {
  bool Ret = false;
  if (CVS1.size() == CVS2.size()) {
    Ret = CVS1.size() <= 1;
    if (CVS1.size() == 1) {
     auto *CV1 = getOnly(CVS1);
     auto *CV2 = getOnly(CVS2);
     Ret = CV1->solutionEqualTo(CS, CV2);
    }
  }
  return Ret;
}

bool FunctionVariableConstraint::
    solutionEqualTo(Constraints &CS, ConstraintVariable *CV) {
  bool Ret = false;
  if (CV != nullptr) {
    if (FVConstraint *OtherFV = dyn_cast<FVConstraint>(CV)) {
      Ret = (numParams() == OtherFV->numParams());
      Ret = Ret && cvSetsSolutionEqualTo(CS, getReturnVars(),
                                       OtherFV->getReturnVars());
      for (unsigned i=0; i < numParams(); i++) {
        Ret = Ret && cvSetsSolutionEqualTo(CS, getParamVar(i),
                                         OtherFV->getParamVar(i));
      }
    }
  }
  return Ret;
}

std::string
FunctionVariableConstraint::mkString(EnvironmentMap &E,
                                     bool EmitName, bool ForItype) {
  std::string Ret = "";
  // TODO punting on what to do here. The right thing to do is to figure out
  // The LUB of all of the V in returnVars.
  assert(returnVars.size() > 0);
  ConstraintVariable *V = *returnVars.begin();
  assert(V != nullptr);
  Ret = V->mkString(E);
  Ret = Ret + "(";
  std::vector<std::string> ParmStrs;
  for (const auto &I : this->paramVars) {
    // TODO likewise punting here.
    assert(I.size() > 0);
    ConstraintVariable *U = *(I.begin());
    assert(U != nullptr);
    std::string ParmStr = U->getRewritableOriginalTy() + U->getName();
    if (U->anyChanges(E))
      ParmStr = U->mkString(E);
    ParmStrs.push_back(ParmStr);
  }

  if (ParmStrs.size() > 0) {
    std::ostringstream Ss;

    std::copy(ParmStrs.begin(), ParmStrs.end() - 1,
              std::ostream_iterator<std::string>(Ss, ", "));
    Ss << ParmStrs.back();

    Ret = Ret + Ss.str() + ")";
  } else {
    Ret = Ret + ")";
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
                          bool doEqType) {
  ConstAtom *CAL, *CAR;
  VarAtom *VAL, *VAR;
  ConstAtom *Wild = CS.getWild();

  CAL = clang::dyn_cast<ConstAtom>(L);
  CAR = clang::dyn_cast<ConstAtom>(R);
  VAL = clang::dyn_cast<VarAtom>(L);
  VAR = clang::dyn_cast<VarAtom>(R);

  // Check constant atom relationships hold
  if (CAR != nullptr && CAL != nullptr) {
    if (doEqType) { // check equality, no matter the atom
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
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true)); // Equality for checked
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false)); // Not for ptyp ...
      if (doEqType)
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false)); // .... Unless indicated
      break;
    case Safe_to_Wild:
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (doEqType) {
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    case Wild_to_Safe:
      CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true)); // note reversal!
      CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
      if (doEqType) {
        CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
      }
      break;
    }
  } else {
    if (CAL == Wild || CAR == Wild) { // This should be a checked/unchecked constraint
      switch (CAct) {
      case Same_to_Same:
	CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
	CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
	break;
      case Safe_to_Wild:
	CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        if (doEqType)
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true));
        break;
      case Wild_to_Safe:
	CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, true)); // note reversal!
        if (doEqType)
          CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, true));
        break;
      }
    } else { // This should be a pointer-type constraint
      switch (CAct) {
      case Same_to_Same:
      case Safe_to_Wild:
      case Wild_to_Safe:
	CS.addConstraint(CS.createGeq(L, R, Rsn, PSL, false));
        if (doEqType)
          CS.addConstraint(CS.createGeq(R, L, Rsn, PSL, false));
	break;
      }
    }
  }
}

// Generate constraints according to CA |- RHS <: LHS.
// If doEqType is true, then also do CA |- LHS <: RHS
void constrainConsVarGeq(ConstraintVariable *LHS, ConstraintVariable *RHS,
                         Constraints &CS, PersistentSourceLoc *PL,
                         ConsAction CA, bool doEqType, ProgramInfo *Info) {

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
        constrainConsVarGeq(FCLHS->getReturnVars(), FCRHS->getReturnVars(), CS,
                            PL, Same_to_Same, doEqType, Info);

        // Constrain the parameters contravariantly
        if (FCLHS->numParams() == FCRHS->numParams()) {
          for (unsigned i = 0; i < FCLHS->numParams(); i++) {
            std::set<ConstraintVariable *> &LHSV =
                FCLHS->getParamVar(i);
            std::set<ConstraintVariable *> &RHSV =
                FCRHS->getParamVar(i);
            // FIXME: Make neg(CA) here? Now: Function pointers equated
            constrainConsVarGeq(RHSV, LHSV, CS, PL, Same_to_Same, doEqType,
                                Info);
          }
        } else {
          // Constrain both to be top.
          std::string Rsn = "Assigning from:" + FCRHS->getName() +
                            " to " + FCLHS->getName();
          RHS->constrainToWild(CS, Rsn, PL);
          LHS->constrainToWild(CS, Rsn, PL);
        }
      } else {
        llvm_unreachable("impossible");
      }
    }
    else if (PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS)) {
      if (PVConstraint *PCRHS = dyn_cast<PVConstraint>(RHS)) {
        std::string Rsn = "";
        // This is to handle function subtyping. Try to add LHS and RHS
        // to each others argument constraints.
        PCLHS->addArgumentConstraint(PCRHS, *Info);
        PCRHS->addArgumentConstraint(PCLHS, *Info);
        // Element-wise constrain PCLHS and PCRHS to be equal
        CAtoms CLHS = PCLHS->getCvars();
        CAtoms CRHS = PCRHS->getCvars();

        // Only generate constraint if LHS is not a base type
        if (CLHS.size() != 0) {
          if (CLHS.size() == CRHS.size()) {
            int n = 0;
            CAtoms::iterator I = CLHS.begin();
            CAtoms::iterator J = CRHS.begin();
            while (I != CLHS.end()) {
              // Get outermost pointer first, using current ConsAction
              if (n == 0)
                createAtomGeq(CS, *I, *J, Rsn, PL, CA, doEqType);
              else {
                // Now constrain the inner ones as equal
                createAtomGeq(CS, *I, *J, Rsn, PL, CA, true);
              }
              ++I;
              ++J;
              n++;
            }
          // Unequal sizes means casting from (say) T** to T*; not safe
          } else {
            // Constrain both to be top.
            std::string Rsn = "Assigning from:" + PCRHS->getName() + " to " +
                              PCLHS->getName();
            PCLHS->constrainToWild(CS, Rsn, PL);
            PCRHS->constrainToWild(CS, Rsn, PL);
          }
          // Equate the corresponding FunctionContraint.
          constrainConsVarGeq(PCLHS->getFV(), PCRHS->getFV(), CS, PL, CA,
                              doEqType, Info);
        }
      } else
        llvm_unreachable("impossible");
    } else
      llvm_unreachable("unknown kind");
  }
  else {
    // Assigning from a function variable to a pointer variable?
    PVConstraint *PCLHS = dyn_cast<PVConstraint>(LHS);
    FVConstraint *FCRHS = dyn_cast<FVConstraint>(RHS);
    if (PCLHS && FCRHS) {
      if (FVConstraint *FCLHS = PCLHS->getFV()) {
        constrainConsVarGeq(FCLHS, FCRHS, CS, PL, CA, doEqType, Info);
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

// Given an RHS and a LHS, constrain them to be equal.
void constrainConsVarGeq(std::set<ConstraintVariable *> &LHS,
                      std::set<ConstraintVariable *> &RHS,
                      Constraints &CS,
                      PersistentSourceLoc *PL,
                      ConsAction CA,
                      bool doEqType,
                      ProgramInfo *Info) {
  for (const auto &I : LHS) {
    for (const auto &J : RHS) {
      constrainConsVarGeq(I, J, CS, PL, CA, doEqType, Info);
    }
  }
}

// True if [C] is a PVConstraint that contains at least one Atom (i.e.,
//   it represents a C pointer)
bool isAValidPVConstraint(ConstraintVariable *C) {
  if (C != nullptr) {
    if (PVConstraint *PV = dyn_cast<PVConstraint>(C))
      return !PV->getCvars().empty();
  }
  return false;
}

// Replace CVars and argumentConstraints with those in [FromCV]
void PointerVariableConstraint::brainTransplant(ConstraintVariable *FromCV) {
  PVConstraint *From = dyn_cast<PVConstraint>(FromCV);
  assert (From != nullptr);
  CAtoms CFrom = From->getCvars();
  assert (vars.size() == CFrom.size());
  vars = CFrom; // FIXME: structural copy? By reference?
  argumentConstraints = From->getArgumentConstraints();
  if (FV) {
    assert(From->FV);
    FV->brainTransplant(From->FV);
  }
}

void PointerVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV) {
  PVConstraint *From = dyn_cast<PVConstraint>(FromCV);
  std::vector<Atom *> NewVatoms;
  CAtoms CFrom = From->getCvars();
  CAtoms::iterator I = vars.begin();
  CAtoms::iterator J = CFrom.begin();
  while (I != vars.end()) {
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
      // we are moving towards checked types
      if (ICAt != JCAt) {
        if (!dyn_cast<WildAtom>(ICAt)) {
          assert(false && "Should be same checked types");
        }
      }
    }
    ++I;
    ++J;
  }
  assert (vars.size() == NewVatoms.size() && "Merging Failed");
  vars = NewVatoms;
  if (!From->ItypeStr.empty())
    ItypeStr = From->ItypeStr;
  if (FV) {
    assert(From->FV);
    FV->mergeDeclaration(From->FV);
  }
}

// Brain Transplant params and returns in [FromCV], recursively
void FunctionVariableConstraint::brainTransplant(ConstraintVariable *FromCV) {
  FVConstraint *From = dyn_cast<FVConstraint>(FromCV);
  assert (From != nullptr);
  // transplant returns
  auto fromRetVar = getOnly(From->getReturnVars());
  auto retVar = getOnly(returnVars);
  retVar->brainTransplant(fromRetVar);
  // transplant params
  assert(From->numParams() == numParams());
  for (unsigned i = 0; i < From->numParams(); i++) {
    std::set<ConstraintVariable *> &FromP = From->getParamVar(i);
    std::set<ConstraintVariable *> &P = getParamVar(i);
    auto FromVar = getOnly(FromP);
    auto Var = getOnly(P);
    Var->brainTransplant(FromVar);
  }
}

void FunctionVariableConstraint::mergeDeclaration(ConstraintVariable *FromCV) {
  FVConstraint *From = dyn_cast<FVConstraint>(FromCV);
  assert (From != nullptr);
  // transplant returns
  auto fromRetVar = getOnly(From->getReturnVars());
  auto retVar = getOnly(returnVars);
  retVar->mergeDeclaration(fromRetVar);
  // transplant params
  assert(From->numParams() == numParams());
  for (unsigned i = 0; i < From->numParams(); i++) {
    std::set<ConstraintVariable *> &FromP = From->getParamVar(i);
    std::set<ConstraintVariable *> &P = getParamVar(i);
    auto FromVar = getOnly(FromP);
    auto Var = getOnly(P);
    Var->mergeDeclaration(FromVar);
  }
}


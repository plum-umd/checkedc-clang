//=--TypeVariableAnalysis.cpp-------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Classes dealing with the processing of type variables for generic functions.
//===----------------------------------------------------------------------===//

#include "clang/3C/TypeVariableAnalysis.h"

using namespace llvm;
using namespace clang;

std::set<ConstraintVariable *> &TypeVariableEntry::getConstraintVariables() {
  return ArgConsVars;
}

void TypeVariableEntry::insertConstraintVariables(
    std::set<ConstraintVariable *> &CVs) {
  ArgConsVars.insert(CVs.begin(), CVs.end());
}

void TypeVariableEntry::setTypeParamConsVar(ConstraintVariable *CV) {
  assert("Accessing constraint variable for inconsistent Type Variable." &&
         IsConsistent);
  assert("Setting constraint variable to null" && CV != nullptr);
  assert("Changing already set constraint variable" &&
         TypeParamConsVar == nullptr);
  TypeParamConsVar = CV;
}

void TypeVariableEntry::updateEntry(QualType Ty, CVarSet &CVs) {
  if (!(Ty->isArrayType() || Ty->isPointerType())) {
    // We need to have a pointer or an array type for an instantiation to make
    // sense. Anything else is treated as inconsistent.
    IsConsistent = false;
  } else {
    // If the type has previously been instantiated as a different type, its use
    // is not consistent. We also make it inconsistent if the type is anonymous
    // since we'll need a name to fill the type arguments during rewriting.
    const clang::Type *PtrTy = Ty->getPointeeOrArrayElementType();
    if (IsConsistent && (isTypeAnonymous(PtrTy) ||
                         getType()->getPointeeOrArrayElementType() != PtrTy))
      IsConsistent = false;
  }
  // Record new constraints for the entry. These are used even when the variable
  // is not consistent.
  insertConstraintVariables(CVs);
}

ConstraintVariable *TypeVariableEntry::getTypeParamConsVar() {
  assert("Accessing constraint variable for inconsistent Type Variable." &&
         IsConsistent);
  assert("Accessing null constraint variable" && TypeParamConsVar != nullptr);
  return TypeParamConsVar;
}

QualType TypeVariableEntry::getType() {
  assert("Accessing Type for inconsistent Type Variable." && IsConsistent);
  return TyVarType;
}

bool TypeVariableEntry::getIsConsistent() const { return IsConsistent; }

// Finds cast expression that contain function call to a generic function. If
// the return type of the function uses a type variable, a binding for the
// return is added to the type variable map.
bool TypeVarVisitor::VisitCastExpr(CastExpr *CE) {
  Expr *SubExpr = CE->getSubExpr();
  if (CHKCBindTemporaryExpr *TempE = dyn_cast<CHKCBindTemporaryExpr>(SubExpr))
    SubExpr = TempE->getSubExpr();

  if (auto *Call = dyn_cast<CallExpr>(SubExpr))
    if (auto *FD = dyn_cast_or_null<FunctionDecl>(Call->getCalleeDecl())) {
      FunctionDecl *FDef = getDefinition(FD);
      if (FDef == nullptr)
        FDef = FD;
      if (auto *FVCon = Info.getFuncConstraint(FDef, Context)) {
        const int TyIdx = FVCon->getGenericIndex();
        if (TyIdx >= 0) {
          clang::QualType Ty = CE->getType();
          std::set<ConstraintVariable *> CVs =
              CR.getExprConstraintVars(SubExpr);
          insertBinding(Call, TyIdx, Ty, CVs);
        }
      }
    }
  return true;
}

bool TypeVarVisitor::VisitCallExpr(CallExpr *CE) {
  if (auto *FD = dyn_cast_or_null<FunctionDecl>(CE->getCalleeDecl())) {
    FunctionDecl *FDef = getDefinition(FD);
    if (FDef == nullptr)
      FDef = FD;
    if (auto *FVCon = Info.getFuncConstraint(FDef, Context)) {
      // if we need to rewrite it but can't (macro, etc), it isn't safe
      bool ForcedInconsistent = !typeArgsProvided(CE)
                                && !Rewriter::isRewritable(CE->getExprLoc());
      // Visit each function argument, and if it use a type variable, insert it
      // into the type variable binding map.
      unsigned int I = 0;
      for (auto *const A : CE->arguments()) {
        // This can happen with varargs.
        if (I >= FVCon->numParams())
          break;
        const int TyIdx = FVCon->getExternalParam(I)->getGenericIndex();
        if (TyIdx >= 0) {
          Expr *Uncast = A->IgnoreImpCasts();
          std::set<ConstraintVariable *> CVs = CR.getExprConstraintVars(Uncast);
          insertBinding(CE, TyIdx, Uncast->getType(),
                        CVs, ForcedInconsistent);
        }
        ++I;
      }
    }

    // For each type variable added above, make a new constraint variable to
    // remember the solved pointer type.
    for (auto &TVEntry : TVMap[CE])
      if (TVEntry.second.getIsConsistent()) {
        std::string Name =
            FD->getNameAsString() + "_tyarg_" + std::to_string(TVEntry.first);
        PVConstraint *P =
            new PVConstraint(TVEntry.second.getType(), nullptr, Name, Info,
                             *Context, nullptr, TVEntry.first);

        // Constrain this variable GEQ the function arguments using the type
        // variable so if any of them are wild, the type argument will also be
        // an unchecked pointer.
        constrainConsVarGeq(P, TVEntry.second.getConstraintVariables(),
                            Info.getConstraints(), nullptr, Safe_to_Wild, false,
                            &Info);

        TVEntry.second.setTypeParamConsVar(P);
      } else {
        // TODO: This might be too cautious.
        CR.constraintAllCVarsToWild(TVEntry.second.getConstraintVariables(),
                                    "Used with inconsistent type variable.");
      }
  }
  return true;
}

// Update the type variable map for a new use of a type variable. For each use
// the exact type variable is identified by the call expression where it is
// used and the index of the type variable type in the function declaration.
void TypeVarVisitor::insertBinding(CallExpr *CE, const int TyIdx,
                                   clang::QualType Ty, CVarSet &CVs,
                                   bool ForceInconsistent) {
  assert(TyIdx >= 0 &&
         "Creating a type variable binding without a type variable.");
  auto &CallTypeVarMap = TVMap[CE];
  if (CallTypeVarMap.find(TyIdx) == CallTypeVarMap.end()) {
    // If the type variable hasn't been seen before, add it to the map.
    TypeVariableEntry TVEntry = TypeVariableEntry(Ty, CVs, ForceInconsistent);
    CallTypeVarMap[TyIdx] = TVEntry;
  } else {
    // Otherwise, update entry with new type and constraints.
    CallTypeVarMap[TyIdx].updateEntry(Ty, CVs);
  }
}

// Lookup the of type parameters for a CallExpr that are used consistently.
// Type parameters are identified by their index in the type parameter list and
// consistent parameters are added to the Types set reference.
void TypeVarVisitor::getConsistentTypeParams(CallExpr *CE,
                                             std::set<unsigned int> &Types) {
  // Gather consistent TypeVariables into output set
  auto &CallTypeVarBindings = TVMap[CE];
  for (const auto &TVEntry : CallTypeVarBindings)
    if (TVEntry.second.getIsConsistent())
      Types.insert(TVEntry.first);
}

// Store type param bindings persistently in ProgramInfo so they are available
// during rewriting.
void TypeVarVisitor::setProgramInfoTypeVars() {
  for (const auto &TVEntry : TVMap) {
    // Add each type variable into the map in ProgramInfo. Inconsistent
    // variables are mapped to null.
    for (auto TVCallEntry : TVEntry.second)
      if (TVCallEntry.second.getIsConsistent())
        Info.setTypeParamBinding(TVEntry.first, TVCallEntry.first,
                                 TVCallEntry.second.getTypeParamConsVar(),
                                 Context);
      else
        Info.setTypeParamBinding(TVEntry.first, TVCallEntry.first, nullptr,
                                 Context);
  }
}

// Check if type arguments have already been provided for this function
// call so that we don't mess with anything already there.
bool typeArgsProvided(CallExpr *Call) {
  Expr *Callee = Call->getCallee()->IgnoreImpCasts();
  if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(Callee)) {
    // ArgInfo is null if there are no type arguments anywhere in the program
    if (auto *ArgInfo = DRE->GetTypeArgumentInfo())
      for (auto Arg : ArgInfo->typeArgumentss()) {
        if (!Arg.typeName->isVoidType()) {
          // Found a non-void type argument. No doubt type args are provided.
          return true;
        }
        if (Arg.sourceInfo->getTypeLoc().getSourceRange().isValid()) {
          // The type argument is void, but with a valid source range. This
          // means an explict void type argument was provided.
          return true;
        }
        // A void type argument without a source location. The type argument
        // is implicit so, we're good to insert a new one.
      }
    return false;
  }
  // We only handle direct calls, so there must be a DeclRefExpr.
  llvm_unreachable("Callee of function call is not DeclRefExpr.");
}

//=--ABounds.cpp--------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains the implementation of the methods in ABounds.h.
//
//===----------------------------------------------------------------------===//

#include "clang/3C/ABounds.h"
#include "clang/3C/AVarBoundsInfo.h"

ABounds *ABounds::getBoundsInfo(AVarBoundsInfo *ABInfo, BoundsExpr *BExpr,
                                const ASTContext &C) {
  ABounds *Ret = nullptr;
  CountBoundsExpr *CBE = dyn_cast<CountBoundsExpr>(BExpr->IgnoreParenCasts());
  BoundsKey VK;
  if (CBE && !CBE->isCompilerGenerated()) {
    if (ABInfo->tryGetVariable(CBE->getCountExpr()->IgnoreParenCasts(), C,
                               VK)) {
      ProgramVar *PV = ABInfo->getProgramVar(VK);
      if (PV->isNumConstant() && PV->getConstantVal() == 0) {
        // Invalid bounds. This is for functions like free.
        // Where the bounds is 0.
        Ret = nullptr;
      } else if (BExpr->isElementCount()) {
        Ret = new CountBound(VK);
      } else if (BExpr->isByteCount()) {
        Ret = new ByteBound(VK);
      }
    }
  }
  // Disabling Range bounds as they are not currently supported!
  /*
  RangeBoundsExpr *RBE = dyn_cast<RangeBoundsExpr>(BExpr->IgnoreParenCasts());
  if (BExpr->isRange() && RBE) {
    Expr *LHS = RBE->getLowerExpr()->IgnoreParenCasts();
    Expr *RHS = RBE->getUpperExpr()->IgnoreParenImpCasts();
    BoundsKey LV, RV;
    if (ABInfo->tryGetVariable(LHS, C, LV) &&
        ABInfo->tryGetVariable(RHS, C, RV)) {
      Ret = new RangeBound(LV, RV);
    }
  }*/
  return Ret;
}

std::string ABounds::getBoundsKeyStr(BoundsKey BK, AVarBoundsInfo *ABI,
                                     Decl *D) {
  ProgramVar *PV = ABI->getProgramVar(BK);
  assert(PV != nullptr && "No Valid program var");
  std::string BKStr = PV->getVarName();
  unsigned PIdx = 0;
  auto *PVD = dyn_cast_or_null<ParmVarDecl>(D);
  // Does this belong to a function parameter?
  if (PVD && ABI->isFuncParamBoundsKey(BK, PIdx)) {
    // Then get the corresponding parameter in context of the given
    // Function and get its name.
    const FunctionDecl *FD = dyn_cast<FunctionDecl>(PVD->getDeclContext());
    if (FD->getNumParams() > PIdx) {
      auto *NewPVD = FD->getParamDecl(PIdx);
      BKStr = NewPVD->getNameAsString();
      // If the parameter in the new declaration does not have a name?
      // then use the old name.
      if (BKStr.empty())
        BKStr = PV->getVarName();
    }
  }
  return BKStr;
}

std::string CountBound::mkString(AVarBoundsInfo *ABI, clang::Decl *D) {
  return "count(" + ABounds::getBoundsKeyStr(LenVar, ABI, D) + ")";
}

std::string CountBound::mkRangeString(AVarBoundsInfo *ABI, clang::Decl *D,
                                      std::string BasePtr) {
  // Assume that BasePtr is the same pointer type as this pointer this bound
  // acts on, so pointer arithmetic works as expected.
  return "bounds(" + BasePtr + ", " + BasePtr + " + " +
         ABounds::getBoundsKeyStr(LenVar, ABI, D) + ")";
}

bool CountBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (O != nullptr) {
    if (CountBound *OT = dyn_cast<CountBound>(O))
      return ABI->areSameProgramVar(this->LenVar, OT->LenVar);
  }
  return false;
}

ABounds *CountBound::makeCopy(BoundsKey NK) { return new CountBound(NK); }

std::string CountPlusOneBound::mkString(AVarBoundsInfo *ABI, clang::Decl *D) {
  std::string CVar = ABounds::getBoundsKeyStr(LenVar, ABI, D);
  return "count(" + CVar + " + 1)";
}

std::string CountPlusOneBound::mkRangeString(AVarBoundsInfo *ABI,
                                             clang::Decl *D,
                                             std::string BasePtr) {
  return "bounds(" + BasePtr + ", " + BasePtr + " + " +
         ABounds::getBoundsKeyStr(LenVar, ABI, D) + "+ 1)";
}

bool CountPlusOneBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (CountPlusOneBound *OT = dyn_cast_or_null<CountPlusOneBound>(O))
    return ABI->areSameProgramVar(this->LenVar, OT->LenVar);
  return false;
}

std::string ByteBound::mkString(AVarBoundsInfo *ABI, clang::Decl *D) {
  return "byte_count(" + ABounds::getBoundsKeyStr(LenVar, ABI, D) + ")";
}

std::string ByteBound::mkRangeString(AVarBoundsInfo *ABI, clang::Decl *D,
                                     std::string BasePtr) {
  // BasePtr will be a pointer to some type that is not necessarily char, so
  // pointer arithmetic will not give the behavior needed for a byte count.
  // First cast the pointer to a char pointer, and then add byte count.
  return "bounds(((_Array_ptr<char>)" + BasePtr + "), ((_Array_ptr<char>)" +
         BasePtr + ") + " + ABounds::getBoundsKeyStr(LenVar, ABI, D) + ")";
}

bool ByteBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (ByteBound *BB = dyn_cast_or_null<ByteBound>(O))
    return ABI->areSameProgramVar(this->LenVar, BB->LenVar);
  return false;
}

ABounds *ByteBound::makeCopy(BoundsKey NK) { return new ByteBound(NK); }

#if OLD_RANGE_BOUNDS
std::string RangeBound::mkString(AVarBoundsInfo *ABI, clang::Decl *D) {
  std::string LBStr = ABounds::getBoundsKeyStr(LB, ABI, D);
  std::string UBStr = ABounds::getBoundsKeyStr(UB, ABI, D);
  return "bounds(" + LBStr + ", " + UBStr + ")";
}

std::string RangeBound::mkRangeString(AVarBoundsInfo *ABI, clang::Decl *D,
                                      std::string BasePtr) {
  return mkString(ABI, D);
}

bool RangeBound::areSame(ABounds *O, AVarBoundsInfo *ABI) {
  if (RangeBound *RB = dyn_cast_or_null<RangeBound>(O))
    return ABI->areSameProgramVar(this->LB, RB->LB) &&
           ABI->areSameProgramVar(this->UB, RB->UB);
  return false;
}
#endif

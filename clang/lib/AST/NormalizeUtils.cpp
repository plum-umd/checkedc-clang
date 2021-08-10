//===--------- NormalizeUtils.cpp: Functions for normalizing expressions --===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements functions for normalizing expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ExprUtils.h"
#include "clang/AST/NormalizeUtils.h"

using namespace clang;

// Input form:  E1 - E2
// Output form: E1 + -E2
//
// This transformation is applied to E1 and E2 as well.
Expr *NormalizeUtil::TransformAdditiveOp(Sema &S, Expr *E) {
  // Check that E is of the form E1 +/- E2.
  BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!BO)
    return nullptr;
  if (!BinaryOperator::isAdditiveOp(BO->getOpcode()))
    return nullptr;

  // If E1 is of the form E3 - E4, transform E1 to E3 + -E4.
  Expr *E1 = TransformSingleAdditiveOp(S, BO->getLHS());

  // If E2 is of the form E5 - E6, transform E2 to E5 + -E6.
  Expr *E2 = TransformSingleAdditiveOp(S, BO->getRHS());

  // Negate E2 if E is of the form E1 - E2.
  if (BO->getOpcode() == BinaryOperatorKind::BO_Sub)
    E2 = ExprCreatorUtil::CreateUnaryOperator(S, E2,
                             UnaryOperatorKind::UO_Minus);

  // If no change was made to E1 or E2, there is no need to create
  // a new expression.
  if (E1 == BO->getLHS() && E2 == BO->getRHS())
    return E;

  return AddExprs(S, E1, E2);
}

// Input form:  E1 + (E2 +/- E3)
// Output form: (E1 + E2) +/- E3
// Requirements:
// 1. E1 has pointer type
// 2. E2 has integer type
// 3. E3 has integer type
Expr *NormalizeUtil::TransformAssocLeft(Sema &S, Expr *E) {
  // E must be of the form LHS +/- RHS.
  BinaryOperator *RootBinOp = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!RootBinOp)
    return nullptr;
  if (!RootBinOp->isAdditiveOp())
    return nullptr;

  Expr *E1, *E2, *E3;

  // Check if E is already of the form (E1 + E2) +/- E3.
  if (BinaryOperator *LHSBinOp = dyn_cast<BinaryOperator>(RootBinOp->getLHS()->IgnoreParens())) {
    if (LHSBinOp->getOpcode() == BinaryOperatorKind::BO_Add) {
      E1 = LHSBinOp->getLHS();
      E2 = LHSBinOp->getRHS();
      E3 = RootBinOp->getRHS();

      // Check that E1 has pointer type, and that E2 and E3 have integer type.
      if (E1->getType()->isPointerType() && E2->getType()->isIntegerType() &&
          E3->getType()->isIntegerType())
        return E;
    }
  }

  // E must be an addition operator.
  if (RootBinOp->getOpcode() != BinaryOperatorKind::BO_Add)
    return nullptr;

  // E1 must have pointer type.
  E1 = RootBinOp->getLHS();
  if (!E1->getType()->isPointerType())
    return nullptr;

  // E must be of the form E1 + (E2 +/- E3).
  BinaryOperator *RHSBinOp = dyn_cast<BinaryOperator>(RootBinOp->getRHS()->IgnoreParens());
  if (!RHSBinOp)
    return nullptr;
  if (!RHSBinOp->isAdditiveOp())
    return nullptr;

  // E2 and E3 must have integer type.
  E2 = RHSBinOp->getLHS();
  E3 = RHSBinOp->getRHS();
  if (!E2->getType()->isIntegerType() || !E3->getType()->isIntegerType())
    return nullptr;

  // If E is of the form E1 + (E2 + E3), output expression is (E1 + E2) + E3.
  if (RHSBinOp->getOpcode() == BinaryOperatorKind::BO_Add)
    return AddExprs(S, AddExprs(S, E1, E2), E3);

  // If E is of the form E1 + (E2 - E3), output expression is (E1 + E2) - E3.
  else if (RHSBinOp->getOpcode() == BinaryOperatorKind::BO_Sub)
    return ExprCreatorUtil::CreateBinaryOperator(S, AddExprs(S, E1, E2), E3,
                                                 BinaryOperatorKind::BO_Sub);

  return nullptr;
}

bool NormalizeUtil::GetVariableAndConstant(Sema &S, Expr *E, Expr *&Variable,
                                           llvm::APSInt &Constant) {
  // While possible, split E into a PointerExpr and an integer constant C
  // and add C to Constant.
  Expr *PointerExpr = E;
  llvm::APSInt C;
  bool GotPointerAndConst = QueryPointerAdditiveConstant(S, PointerExpr,
                                                         PointerExpr, C);
  while (GotPointerAndConst) {
    if (!AddConstants(Constant, C))
      goto exit;
    GotPointerAndConst = QueryPointerAdditiveConstant(S, PointerExpr,
                                                      PointerExpr, C);
  }

  if (!PointerExpr)
    goto exit;

  // If PointerExpr is of the form p + (i +/- j) (where p is a pointer and
  // i and j are integers), rewrite PointerExpr as (p + i) +/- j.
  if (Expr *AssocLeft = TransformAssocLeft(S, PointerExpr))
    PointerExpr = AssocLeft;

  // After transforming subtraction operators and applying left-associativity,
  // we might be able to get another integer constant from PointerExpr.
  // If so, add it to Constant if the addition does not overflow.
  if (QueryPointerAdditiveConstant(S, PointerExpr, PointerExpr, C))
    if (!AddConstants(Constant, C))
      goto exit;

  Variable = PointerExpr;
  return true;

  exit:
    // Return (E, 0).
    Variable = E;
    uint64_t PointerWidth = S.Context.getTargetInfo().getPointerWidth(0);
    Constant = llvm::APSInt(PointerWidth, false);
    return false;
}

// Input form: (E1 +/- A) +/- B.
// Outputs: Variable: E1, Constant: (+/-)A + (+/-)B.
bool NormalizeUtil::ConstantFold(Sema &S, Expr *E, QualType T, Expr *&Variable,
                                 llvm::APSInt &Constant) {
  llvm::APSInt LHSConst;
  llvm::APSInt RHSConst;
  BinaryOperator *LHSBinOp = nullptr;

  // E must be of the form LHS +/- RHS.
  BinaryOperator *RootBinOp = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!RootBinOp)
    goto exit;
  if (!RootBinOp->isAdditiveOp())
    goto exit;

  // E must be of the form (E1 +/- E2) +/- RHS.
  LHSBinOp = dyn_cast<BinaryOperator>(RootBinOp->getLHS()->IgnoreParens());
  if (!LHSBinOp)
    goto exit;
  if (!LHSBinOp->isAdditiveOp())
    goto exit;

  // E must be of the form (E1 +/- E2) +/- B, where B is a constant.
  if (!GetRHSConstant(S, RootBinOp, T, RHSConst))
    goto exit;

  // E must be of the form (E1 +/- A) +/- B, where A is a constant.
  if (!GetRHSConstant(S, LHSBinOp, T, LHSConst))
    goto exit;

  Variable = LHSBinOp->getLHS();

  bool Overflow;
  ExprUtil::EnsureEqualBitWidths(LHSConst, RHSConst);
  Constant = LHSConst.sadd_ov(RHSConst, Overflow);
  if (Overflow)
    goto exit;
  return true;

  exit:
    // Return (E, 0).
    Variable = E;
    uint64_t PointerWidth = S.Context.getTargetInfo().getPointerWidth(0);
    Constant = llvm::APSInt(PointerWidth, false);
    return false;
}

// Input form:  E1 - E2
// Output form: E1 + -E2
Expr *NormalizeUtil::TransformSingleAdditiveOp(Sema &S, Expr *E) {
  BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!BO)
    return E;
  if (BO->getOpcode() != BinaryOperatorKind::BO_Sub)
    return E;

  Expr *LHS = BO->getLHS();
  Expr *RHS = BO->getRHS();
  Expr *Minus = ExprCreatorUtil::CreateUnaryOperator(S, RHS,
                                   UnaryOperatorKind::UO_Minus);
  return AddExprs(S, LHS, Minus);
}

Expr *NormalizeUtil::AddExprs(Sema &S, Expr *LHS, Expr *RHS) {
  return ExprCreatorUtil::CreateBinaryOperator(S, LHS, RHS,
                            BinaryOperatorKind::BO_Add);
}

bool NormalizeUtil::AddConstants(llvm::APSInt &C1, llvm::APSInt C2) {
  ExprUtil::EnsureEqualBitWidths(C1, C2);
  bool Overflow;
  C1 = C1.sadd_ov(C2, Overflow);
  return !Overflow;
}

bool NormalizeUtil::GetRHSConstant(Sema &S, BinaryOperator *E, QualType T,
                                   llvm::APSInt &Constant) {
  if (!E->isAdditiveOp())
    return false;
  Optional<llvm::APSInt> OptConstant =
                         E->getRHS()->getIntegerConstantExpr(S.Context);
  if (!OptConstant)
    return false;

  Constant = *OptConstant;
  bool Overflow;
  Constant = ExprUtil::ConvertToSignedPointerWidth(S.Context, Constant, Overflow);
  if (Overflow)
    return false;
  // Normalize the operation by negating the offset if necessary.
  if (E->getOpcode() == BO_Sub) {
    uint64_t PointerWidth = S.Context.getTargetInfo().getPointerWidth(0);
    Constant = llvm::APSInt(PointerWidth, false).ssub_ov(Constant, Overflow);
    if (Overflow)
      return false;
  }
  llvm::APSInt ElemSize;
  if (!ExprUtil::getReferentSizeInChars(S.Context, T, ElemSize))
    return false;
  Constant = Constant.smul_ov(ElemSize, Overflow);
  if (Overflow)
    return false;

  return true;
}

bool NormalizeUtil::QueryPointerAdditiveConstant(Sema &S, Expr *E,
                                                 Expr *&PointerExpr,
                                                 llvm::APSInt &Constant) {
  BinaryOperator *BO = dyn_cast<BinaryOperator>(E->IgnoreParens());
  if (!BO)
    return false;
  if (!BO->isAdditiveOp())
    return false;

  Optional<llvm::APSInt> OptConstant;
  // E must be of the form PointerExpr +/- Constant or Constant + PointerExpr,
  // where PointerExpr has pointer type and Constant is an integer constant.
  // Note that E cannot be of the form Constant - PointerExpr, since a pointer
  // cannot appear on the right-hand side of a subtraction operator.
  if (BO->getLHS()->getType()->isPointerType() &&
      (OptConstant = BO->getRHS()->getIntegerConstantExpr(S.Context)))
    PointerExpr = BO->getLHS();
  else if (BO->getOpcode() == BinaryOperatorKind::BO_Add &&
              BO->getRHS()->getType()->isPointerType() &&
              (OptConstant = BO->getLHS()->getIntegerConstantExpr(S.Context)))
    PointerExpr = BO->getRHS();
  else
    return false;

  Constant = *OptConstant;
  bool Overflow;
  Constant = ExprUtil::ConvertToSignedPointerWidth(S.Context, Constant, Overflow);
  if (Overflow)
    return false;
  // Normalize the operation by negating the offset if necessary.
  if (BO->getOpcode() == BO_Sub) {
    uint64_t PointerWidth = S.Context.getTargetInfo().getPointerWidth(0);
    Constant = llvm::APSInt(PointerWidth, false).ssub_ov(Constant, Overflow);
    if (Overflow)
      return false;
  }
  llvm::APSInt ElemSize;
  if (!ExprUtil::getReferentSizeInChars(S.Context, E->getType(), ElemSize))
    return false;
  Constant = Constant.smul_ov(ElemSize, Overflow);
  if (Overflow)
    return false;

  return true;
}

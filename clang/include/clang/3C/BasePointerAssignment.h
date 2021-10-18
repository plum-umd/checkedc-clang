//=--PointerArithmeticAssignment.h--------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Contains classes for detection and rewriting of assignment expression that
// would invalidate the bounds of pointers rewritten to use range bounds.
// For pointers using a range bound `bounds(__3c_tmp_p, __3c_tmp_p + n)`, an
// assignment `p = q` effectively changes the "base pointer" of the range
// bounds, so that the new bounds of `p` are `bounds(q, q + n)`
// (assuming `q` has the same size as `p`). For this to not invalidate the
// bound, `__3c_tmp_p` must also be updated to be equal to `q`.
//===----------------------------------------------------------------------===//

#ifndef LLVM_BASEPOINTERASSIGNMENT_H
#define LLVM_BASEPOINTERASSIGNMENT_H

#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/3C/ConstraintResolver.h"

// Return true if an assignment LHS=RHS is the value of RHS is not derived form
// LHS. For example, an assignment `p = q` will return true (we assume `q`
// doesn't alias `p`), while `p = p + 1` will return false.
bool isBasePointerAssignment(clang::Expr *LHS, clang::Expr *RHS);

// A class to visit all base pointer assignment expression as detected by
// isBasePointerAssignment. This class should be extended with
// visitBasePointerAssignment overridden.
class BasePointerAssignmentVisitor
  : public RecursiveASTVisitor<BasePointerAssignmentVisitor> {
public:
  explicit BasePointerAssignmentVisitor() {}

  bool VisitBinaryOperator(BinaryOperator *O);

  // Override this method to define the operation that should be performed on
  // each assignment. The LHS and RHS of the assignment expression are passed
  // through.
  virtual void visitBasePointerAssignment(Expr *LHS, Expr *RHS) = 0;
};

// Visit each base pointer assignment expression and, if the LHS is a pointer
// variable that was rewritten to use range bounds, rewrite the assignment so
// that it doesn't not invalidate the bounds. e.g.:
//     q = p;
// becomes
//     __3c_tmp_q = p, q = __3c_tmp_q;
class BasePointerAssignmentUpdater : public BasePointerAssignmentVisitor {
public:
  explicit BasePointerAssignmentUpdater(ASTContext *C, ProgramInfo &I,
                                        Rewriter &R) : ABInfo(
    I.getABoundsInfo()), CR(I, C), R(R) {}

  void visitBasePointerAssignment(Expr *LHS, Expr *RHS) override;

private:
  AVarBoundsInfo &ABInfo;
  ConstraintResolver CR;
  Rewriter &R;
};

// Visit each base pointer assignment expression and, if it is inside a macro,
// mark the LHS pointer as ineligible for range bounds. This is required
// because, if the pointer is given range bounds, then the assignment expression
// would need to be rewritten. The expression is a macro, so it cannot be
// rewritten.
class BasePointerAssignmentFinder : public BasePointerAssignmentVisitor {
public:
  explicit BasePointerAssignmentFinder(ASTContext *C, ProgramInfo &I) : ABInfo(
    I.getABoundsInfo()), CR(I, C), C(C) {}

  void visitBasePointerAssignment(Expr *LHS, Expr *RHS) override;

private:
  AVarBoundsInfo &ABInfo;
  ConstraintResolver CR;
  ASTContext *C;
};
#endif //LLVM_BASEPOINTERASSIGNMENT_H

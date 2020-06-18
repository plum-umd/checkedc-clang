//=--ConstraintResolver.h-----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Class that helps in resolving constraints for various expressions.
//===----------------------------------------------------------------------===//

#ifndef _CONSTRAINTRESOLVER_H
#define _CONSTRAINTRESOLVER_H

#include "clang/AST/ASTConsumer.h"

#include "ProgramInfo.h"

using namespace llvm;
using namespace clang;

// Class that handles building constraints from various AST artifacts.
class ConstraintResolver {

public:
  ConstraintResolver(ProgramInfo &I, ASTContext *C) : Info(I), Context(C) {
    ExprTmpConstraints.clear();
  }

  virtual ~ConstraintResolver();

  void constraintAllCVarsToWild(std::set<ConstraintVariable*> &CSet,
                                std::string rsn,
                                Expr *AtExpr = nullptr);

  // Returns a set of ConstraintVariables which represent the result of
  // evaluating the expression E. Will explore E recursively, but will
  // ignore parts of it that do not contribute to the final result
  std::set<ConstraintVariable *> getExprConstraintVars(Expr *E);

  // Handle assignment of RHS expression to LHS expression using the
  // given action.
  void constrainLocalAssign(Stmt *TSt, Expr *LHS, Expr *RHS,
                            ConsAction CAction);

  // Handle the assignment of RHS to the given declaration.
  void constrainLocalAssign(Stmt *TSt, DeclaratorDecl *D, Expr *RHS,
                            ConsAction CAction = Same_to_Same);

private:
  ProgramInfo &Info;
  ASTContext *Context;
  // These are temporary constraints, that will be created to handle various
  // expressions
  static std::set<ConstraintVariable *> TempConstraintVars;
  // Map that stores temporary constraint variable copies created for the
  // corresponding expression and constraint variable
  std::map<std::pair<clang::Expr *, ConstraintVariable *>,
           ConstraintVariable *> ExprTmpConstraints;

  ConstraintVariable *getTemporaryConstraintVariable(clang::Expr *E,
                                                     ConstraintVariable *CV);

  std::set<ConstraintVariable *> handleDeref(std::set<ConstraintVariable *> T);

  // Update a PVConstraint with one additional level of indirection
  PVConstraint *addAtom(PVConstraint *PVC, Atom *NewA, Constraints &CS);
  std::set<ConstraintVariable *> addAtomAll(std::set<ConstraintVariable *> CVS,
                                            Atom *PtrTyp, Constraints &CS);
  std::set<ConstraintVariable *> getWildPVConstraint();
  std::set<ConstraintVariable *> PVConstraintFromType(QualType TypE);

  std::set<ConstraintVariable *>
      getAllSubExprConstraintVars(std::vector<Expr *> &Exprs);
  std::set<ConstraintVariable *> getBaseVarPVConstraint(DeclRefExpr *Decl);
};

#endif // _CONSTRAINTRESOLVER_H

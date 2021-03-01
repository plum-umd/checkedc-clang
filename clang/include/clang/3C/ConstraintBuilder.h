//=--ConstraintBuilder.h------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_CONSTRAINTBUILDER_H
#define LLVM_CLANG_3C_CONSTRAINTBUILDER_H

#include "clang/3C/ProgramInfo.h"
#include "clang/3C/TypeVariableAnalysis.h"
#include "clang/AST/ASTConsumer.h"

class VariableAdderConsumer : public clang::ASTConsumer {
public:
  explicit VariableAdderConsumer(ProgramInfo &I, clang::ASTContext *C)
      : Info(I) {}

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
};

class ConstraintBuilderConsumer : public clang::ASTConsumer {
public:
  explicit ConstraintBuilderConsumer(ProgramInfo &I, clang::ASTContext *C)
      : Info(I) {}

  virtual void HandleTranslationUnit(clang::ASTContext &);

private:
  ProgramInfo &Info;
};

#endif

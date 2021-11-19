//=--MappingVisitor.h---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// The MappingVisitor is used to traverse an AST and re-define a mapping from
// PersistentSourceLocations to "live" AST objects. This is needed to support
// multi-compilation unit analyses, where after each compilation unit is
// analyzed, the state of the analysis is "shelved" and all references to AST
// data structures are replaced with data structures that survive the clang
// constructed AST.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_MAPPINGVISITOR_H
#define LLVM_CLANG_3C_MAPPINGVISITOR_H

#include "clang/3C/PersistentSourceLoc.h"
#include "clang/3C/Utils.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"

typedef std::map<PersistentSourceLoc, clang::Decl *> SourceToDeclMapType;

class MappingVisitor : public clang::RecursiveASTVisitor<MappingVisitor> {
public:
  MappingVisitor(std::set<PersistentSourceLoc> S, clang::ASTContext &C)
      : SourceLocs(S), Context(C) {}

  bool VisitDecl(clang::Decl *D);

  const SourceToDeclMapType &getResults() { return PSLtoSDT; }

private:
  // A map from a PersistentSourceLoc to a Decl at that location.
  SourceToDeclMapType PSLtoSDT;
  // The set of PersistentSourceLoc's this instance of MappingVisitor is tasked
  // with re-instantiating as a Decl.
  std::set<PersistentSourceLoc> SourceLocs;
  // The ASTContext for the particular AST that the MappingVisitor is
  // traversing.
  clang::ASTContext &Context;
};

#endif

//=--ABounds.h----------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains bounds information of constraint variables.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_ABOUNDS_H
#define LLVM_CLANG_3C_ABOUNDS_H

#include "clang/3C/ProgramVar.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"

using namespace clang;

class AVarBoundsInfo;
// Class that represents bounds information of a program variable.
class ABounds {
public:
  enum BoundsKind {
    // Invalid bounds.
    InvalidKind,
    // Bounds that represent number of items.
    CountBoundKind,
    // Count bounds but plus one, i.e., count(i+1)
    CountPlusOneBoundKind,
    // Bounds that represent number of bytes.
    ByteBoundKind,
    // Bounds that represent range.
    RangeBoundKind,
  };
  BoundsKind getKind() const { return Kind; }

protected:
  BoundsKind Kind;

protected:
  ABounds(BoundsKind K) : Kind(K) {}
  void addBoundsUsedKey(BoundsKey);
  // Get the variable name of the the given bounds key that corresponds
  // to the given declaration.
  static std::string getBoundsKeyStr(BoundsKey, AVarBoundsInfo *,
                                     clang::Decl *);

public:
  virtual ~ABounds() {}

  virtual std::string mkString(AVarBoundsInfo *, clang::Decl *D = nullptr) = 0;
  virtual bool areSame(ABounds *, AVarBoundsInfo *) = 0;
  virtual BoundsKey getBKey() = 0;
  virtual ABounds *makeCopy(BoundsKey NK) = 0;

  // Set that maintains all the bound keys that are used inin
  // TODO: Is this still needed?
  static std::set<BoundsKey> KeysUsedInBounds;
  static bool isKeyUsedInBounds(BoundsKey ToCheck);

  static ABounds *getBoundsInfo(AVarBoundsInfo *AVBInfo, BoundsExpr *BExpr,
                                const ASTContext &C);
};

class CountBound : public ABounds {
public:
  CountBound(BoundsKey Var) : ABounds(CountBoundKind), CountVar(Var) {
    addBoundsUsedKey(Var);
  }

  ~CountBound() override {}

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  BoundsKey getBKey() override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == CountBoundKind;
  }

  BoundsKey getCountVar() { return CountVar; }

protected:
  BoundsKey CountVar;
};

class CountPlusOneBound : public CountBound {
public:
  CountPlusOneBound(BoundsKey Var) : CountBound(Var) {
    this->Kind = CountPlusOneBoundKind;
  }

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == CountPlusOneBoundKind;
  }
};

class ByteBound : public ABounds {
public:
  ByteBound(BoundsKey Var) : ABounds(ByteBoundKind), ByteVar(Var) {
    addBoundsUsedKey(Var);
  }

  ~ByteBound() override {}

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  BoundsKey getBKey() override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == ByteBoundKind;
  }
  BoundsKey getByteVar() { return ByteVar; }

private:
  BoundsKey ByteVar;
};

class RangeBound : public ABounds {
public:
  RangeBound(BoundsKey L, BoundsKey R) : ABounds(RangeBoundKind), LB(L), UB(R) {
    addBoundsUsedKey(L);
    addBoundsUsedKey(R);
  }

  ~RangeBound() override {}

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;

  BoundsKey getBKey() override {
    assert(false && "Not implemented.");
    return 0;
  }

  ABounds *makeCopy(BoundsKey NK) override {
    assert(false && "Not Implemented");
    return nullptr;
  }

  static bool classof(const ABounds *S) {
    return S->getKind() == RangeBoundKind;
  }

private:
  BoundsKey LB;
  BoundsKey UB;
};
#endif // LLVM_CLANG_3C_ABOUNDS_H

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
  ABounds(BoundsKind K, BoundsKey L) : Kind(K), LenVar(L) {}

  BoundsKind Kind;

  // Bounds key representing the length of the bounds from the base pointer of
  // the range. The exact interpretation of this field varies by subclass.
  BoundsKey LenVar;

  // Get the variable name of the the given bounds key that corresponds
  // to the given declaration.
  static std::string getBoundsKeyStr(BoundsKey, AVarBoundsInfo *,
                                     clang::Decl *);

public:
  virtual ~ABounds() {}
  virtual std::string mkString(AVarBoundsInfo *, clang::Decl *D = nullptr) = 0;
  virtual std::string mkRangeString(AVarBoundsInfo *, clang::Decl *D,
                                    std::string BasePtr) = 0;
  virtual bool areSame(ABounds *, AVarBoundsInfo *) = 0;
  virtual ABounds *makeCopy(BoundsKey NK) = 0;

  BoundsKey getLengthKey() const { return LenVar; }

  static ABounds *getBoundsInfo(AVarBoundsInfo *AVBInfo, BoundsExpr *BExpr,
                                const ASTContext &C);
};

class CountBound : public ABounds {
public:
  CountBound(BoundsKey L) : ABounds(CountBoundKind, L) {}

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  std::string mkRangeString(AVarBoundsInfo *, clang::Decl *D,
                            std::string BasePtr) override;

  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == CountBoundKind;
  }
};

class CountPlusOneBound : public CountBound {
public:
  CountPlusOneBound(BoundsKey L) : CountBound(L) {
    this->Kind = CountPlusOneBoundKind;
  }

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  std::string mkRangeString(AVarBoundsInfo *, clang::Decl *D,
                            std::string BasePtr) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == CountPlusOneBoundKind;
  }
};

class ByteBound : public ABounds {
public:
  ByteBound(BoundsKey L) : ABounds(ByteBoundKind, L) {}

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  std::string mkRangeString(AVarBoundsInfo *, clang::Decl *D,
                            std::string BasePtr) override;
  bool areSame(ABounds *O, AVarBoundsInfo *ABI) override;
  ABounds *makeCopy(BoundsKey NK) override;

  static bool classof(const ABounds *S) {
    return S->getKind() == ByteBoundKind;
  }
};

#define OLD_RANGE_BOUNDS 0
#if OLD_RANGE_BOUNDS
class RangeBound : public ABounds {
public:
  RangeBound(BoundsKey L, BoundsKey R) : ABounds(RangeBoundKind), LB(L),
                                         UB(R) {}

  ~RangeBound() override {}

  std::string mkString(AVarBoundsInfo *ABI, clang::Decl *D = nullptr) override;
  std::string mkRangeString(AVarBoundsInfo *, clang::Decl *D,
                            std::string BasePtr) override;
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
#endif
#endif // LLVM_CLANG_3C_ABOUNDS_H

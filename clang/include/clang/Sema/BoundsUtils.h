//===----------- BoundsUtils.h: Utility functions for bounds ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===------------------------------------------------------------------===//
//
//  This file defines the utility functions for bounds expressions.
//
//===------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BOUNDSUTILS_H
#define LLVM_CLANG_BOUNDSUTILS_H

#include "clang/AST/Expr.h"
#include "clang/Sema/Sema.h"

namespace clang {

class BoundsUtil {
public:
  // IsStandardForm returns true if the bounds expression BE is:
  // 1. bounds(any), or:
  // 2. bounds(unknown), or:
  // 3. bounds(e1, e2), or:
  // 4. Invalid bounds.
  // It returns false if BE is of the form count(e) or byte_count(e).
  static bool IsStandardForm(const BoundsExpr *BE);

  // CreateBoundsUnknown returns bounds(unknown).
  static BoundsExpr *CreateBoundsUnknown(Sema &S);

  // If Bounds uses the value of LValue and an original value is provided,
  // ReplaceLValueInBounds will return a bounds expression where the uses
  // of LValue are replaced with the original value.
  // If Bounds uses the value of LValue and no original value is provided,
  // ReplaceLValueInBounds will return bounds(unknown).
  static BoundsExpr *ReplaceLValueInBounds(Sema &S, BoundsExpr *Bounds,
                                           Expr *LValue, Expr *OriginalValue,
                                           CheckedScopeSpecifier CSS);

  // If an original value is provided, ReplaceLValue returns an expression
  // that replaces all uses of the lvalue expression LValue in E with the
  // original value.  If no original value is provided and E uses LValue,
  // ReplaceLValue returns nullptr.
  static Expr *ReplaceLValue(Sema &S, Expr *E, Expr *LValue,
                             Expr *OriginalValue,
                             CheckedScopeSpecifier CSS);
};

} // end namespace clang

#endif

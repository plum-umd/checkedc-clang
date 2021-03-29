//=--CastPlacement.cpp--------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class contains implementation of the functions and
// classes of CastPlacement.h
//===----------------------------------------------------------------------===//

#include "clang/3C/CastPlacement.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/Utils.h"
#include "clang/Tooling/Transformer/SourceCode.h"

using namespace clang;

bool CastPlacementVisitor::VisitCallExpr(CallExpr *CE) {
  // Get the constraint variable for the function.
  Decl *CalleeDecl = CE->getCalleeDecl();
  FunctionDecl *FD = dyn_cast_or_null<FunctionDecl>(CalleeDecl);

  FVConstraint *FV = getCallFVConstraint(CE);
  if (FV == nullptr)
    return true;

  // Note: I'm not entirely sure that this will always hold. The previous
  // implementation just returned early if FV was null, but I don't think that
  // can ever actually happen.
  // assert("Could not find function constraint variable!" && FV != nullptr);

  // Now we need to check the type of the arguments and corresponding
  // parameters to see if any explicit casting is needed.
  ProgramInfo::CallTypeParamBindingsT TypeVars;
  if (Info.hasTypeParamBindings(CE, Context))
    TypeVars = Info.getTypeParamBindings(CE, Context);

  // Cast on arguments.
  unsigned PIdx = 0;
  for (const auto &A : CE->arguments()) {
    if (PIdx < FV->numParams()) {
      // Avoid adding incorrect casts to generic function arguments by
      // removing implicit casts when on arguments with a consistently
      // used generic type.
      Expr *ArgExpr = A;
      if (FD && PIdx < FD->getNumParams()) {
        const int TyVarIdx = FV->getExternalParam(PIdx)->getGenericIndex();
        if (TypeVars.find(TyVarIdx) != TypeVars.end() &&
            TypeVars[TyVarIdx] != nullptr)
          ArgExpr = ArgExpr->IgnoreImpCasts();
      }

      CVarSet ArgConstraints = CR.getExprConstraintVarsSet(ArgExpr);
      for (auto *ArgC : ArgConstraints) {
        CastNeeded CastKind = needCasting(
            ArgC, ArgC, FV->getInternalParam(PIdx), FV->getExternalParam(PIdx));
        if (CastKind != NO_CAST) {
          surroundByCast(FV->getExternalParam(PIdx), CastKind, A);
          ExprsWithCast.insert(ignoreCheckedCImplicit(A));
          break;
        }
      }
    }
    PIdx++;
  }

  // Cast on return. Be sure not to place casts when the result is not used,
  // otherwise an externally unsafe function whose result is not used would end
  // up with a bounds cast around it. hasPersistentConstraints is used to
  // determine if an expression is used because any expression that is
  // eventually assigned to a variable or passed as a function argument will
  // be cached in the persistent constraint set.
  if (Info.hasPersistentConstraints(CE, Context)) {
    CVarSet DestinationConstraints = CR.getExprConstraintVarsSet(CE);
    for (auto *DstC : DestinationConstraints) {
      // Order of ParameterC and ArgumentC is reversed from when inserting
      // parameter casts because assignment now goes from returned to its
      // local use.
      CastNeeded CastKind = needCasting(FV->getInternalReturn(),
                                        FV->getExternalReturn(), DstC, DstC);
      if (ExprsWithCast.find(CE) == ExprsWithCast.end() &&
          CastKind != NO_CAST) {
        surroundByCast(DstC, CastKind, CE);
        ExprsWithCast.insert(ignoreCheckedCImplicit(CE));
        break;
      }
    }
  }
  return true;
}

CastPlacementVisitor::CastNeeded CastPlacementVisitor::needCasting(
    ConstraintVariable *SrcInt, ConstraintVariable *SrcExt,
    ConstraintVariable *DstInt, ConstraintVariable *DstExt) {
  Constraints &CS = Info.getConstraints();
  // No casting is required if the source exactly matches either the
  // destinations itype or the destinations regular type.
  if (SrcExt->solutionEqualTo(CS, DstExt, false) ||
      SrcExt->solutionEqualTo(CS, DstInt, false) ||
      SrcInt->solutionEqualTo(CS, DstExt, false) ||
      SrcInt->solutionEqualTo(CS, DstInt, false))
    return NO_CAST;

  // As a special case, no casting is required when passing an unchecked pointer
  // to a function with an itype in the original source code. This case is
  // required to avoid adding casts when a function has an itype and is defined
  // in the file. Because the function is defined, the internal type can solve
  // to checked, causing to appear fully checked (without itype). This would
  // cause a bounds cast to be inserted on unchecked calls to the function.
  if (!SrcExt->isChecked(CS.getVariables()) && DstInt->srcHasItype())
    return NO_CAST;

  if (DstInt->isChecked(CS.getVariables()))
    return CAST_TO_CHECKED;

  return CAST_TO_WILD;
}

// Find a FVConstraint for this call. If there is more than one, then they
// will have been unified during constraint generation, so we can use any of
// them.
FVConstraint *CastPlacementVisitor::getCallFVConstraint(CallExpr *CE) {
  for (auto *CV : CR.getCalleeConstraintVars(CE)) {
    FVConstraint *FV = nullptr;
    if (isa<FVConstraint>(CV))
      FV = cast<FVConstraint>(CV);
    else if (isa<PVConstraint>(CV) && cast<PVConstraint>(CV)->getFV())
      FV = cast<PVConstraint>(CV)->getFV();
    if (FV && !isFunctionAllocator(FV->getName()))
      return FV;
  }
  return nullptr;
}

// Get the string representation of the cast required for the call. The return
// is a pair of strings: a prefix and suffix string that form the complete cast
// when placed around the expression being cast.
std::pair<std::string, std::string>
CastPlacementVisitor::getCastString(ConstraintVariable *Dst,
                                    CastNeeded CastKind) {
  switch (CastKind) {
  case CAST_TO_WILD:
    return std::make_pair("((" + Dst->getRewritableOriginalTy() + ")", ")");
  case CAST_TO_CHECKED: {
    std::string Suffix = ")";
    if (const auto *DstPVC = dyn_cast<PVConstraint>(Dst)) {
      assert("Checked cast not to a pointer" && !DstPVC->getCvars().empty());
      ConstAtom *CA =
          Info.getConstraints().getAssignment(DstPVC->getCvars().at(0));

      // Writing an _Assume_bounds_cast to an array type requires inserting
      // the bounds for destination array. These can come from the source
      // code or the infered bounds. If neither source is available, use empty
      // bounds.
      if (isa<ArrAtom>(CA) || isa<NTArrAtom>(CA)) {
        std::string Bounds = "";
        if (DstPVC->srcHasBounds())
          Bounds = DstPVC->getBoundsStr();
        else if (DstPVC->hasBoundsKey())
          Bounds = ABRewriter.getBoundsString(DstPVC, nullptr, true);
        if (Bounds.empty())
          Bounds = "byte_count(0)";

        Suffix = ", " + Bounds + ")";
      }
    }
    return std::make_pair(
      "_Assume_bounds_cast<" + Dst->mkString(Info.getConstraints(), false) +
      ">(", Suffix);
  }
  default:
    llvm_unreachable("No casting needed");
  }
}

void CastPlacementVisitor::surroundByCast(ConstraintVariable *Dst,
                                          CastNeeded CastKind, Expr *E) {
  PersistentSourceLoc PSL = PersistentSourceLoc::mkPSL(E, *Context);
  if (!canWrite(PSL.getFileName())) {
    // 3C has known bugs that can cause attempted cast insertion in
    // unwritable files in common use cases. Until they are fixed, report a
    // warning rather than letting the main "unwritable change" error trigger
    // later.
    clang::DiagnosticsEngine &DE = Writer.getSourceMgr().getDiagnostics();
    unsigned ErrorId = DE.getCustomDiagID(
        DiagnosticsEngine::Warning,
        "3C internal error: tried to insert a cast into an unwritable file "
        "(https://github.com/correctcomputation/checkedc-clang/issues/454)");
    DE.Report(E->getBeginLoc(), ErrorId);
    return;
  }

  auto CastStrs = getCastString(Dst, CastKind);

  // If E is already a cast expression, we will try to rewrite the cast instead
  // of adding a new expression.
  if (isa<CStyleCastExpr>(E->IgnoreParens()) && CastKind == CAST_TO_WILD) {
    auto *CE = cast<CStyleCastExpr>(E->IgnoreParens());
    SourceRange CastTypeRange(CE->getLParenLoc(), CE->getRParenLoc());
    assert("Cast expected to start with '('" && !CastStrs.first.empty() &&
           CastStrs.first[0] == '(');
    std::string CastStr = CastStrs.first.substr(1);
    // FIXME: This rewriting is known to fail on the benchmark programs.
    //        https://github.com/correctcomputation/checkedc-clang/issues/444
    rewriteSourceRange(Writer, CastTypeRange, CastStr, false);
  } else {
    // First try to insert the cast prefix and suffix around the expression in
    // the source code.
    bool FrontRewritable = Writer.isRewritable(E->getBeginLoc());
    bool EndRewritable = Writer.isRewritable(E->getEndLoc());
    if (FrontRewritable && EndRewritable) {
      bool BFail = Writer.InsertTextBefore(E->getBeginLoc(), CastStrs.first);
      bool EFail = Writer.InsertTextAfterToken(E->getEndLoc(), CastStrs.second);
      assert("Locations were rewritable, fail should not be possible." &&
             !BFail && !EFail);
    } else {
      // Sometimes we can't insert the cast around the expression due to macros
      // getting in the way. In these cases, we can sometimes replace the entire
      // expression source with a new string containing the original expression
      // and the cast.
      auto CRA = CharSourceRange::getTokenRange(E->getSourceRange());
      auto NewCRA = clang::Lexer::makeFileCharRange(
          CRA, Context->getSourceManager(), Context->getLangOpts());
      std::string SrcText(clang::tooling::getText(CRA, *Context));
      // This doesn't always work either. We can't rewrite if the cast needs to
      // be placed fully inside a macro rather than around a macro or on an
      // argument to the macro.
      if (!SrcText.empty())
        rewriteSourceRange(Writer, NewCRA,
                           CastStrs.first + SrcText + CastStrs.second);
      else
        reportCastInsertionFailure(E, CastStrs.first + CastStrs.second);
    }
  }
}

void CastPlacementVisitor::reportCastInsertionFailure(
    Expr *E, const std::string &CastStr) {
  // FIXME: This is a warning rather than an error so that a new benchmark
  //        failure is not introduced in Lua.
  //        github.com/correctcomputation/checkedc-clang/issues/439
  clang::DiagnosticsEngine &DE = Context->getDiagnostics();
  unsigned ErrorId = DE.getCustomDiagID(
      DiagnosticsEngine::Warning, "Unable to surround expression with cast.\n"
                                  "Intended cast: \"%0\"");
  auto ErrorBuilder = DE.Report(E->getExprLoc(), ErrorId);
  ErrorBuilder.AddSourceRange(
      Context->getSourceManager().getExpansionRange(E->getSourceRange()));
  ErrorBuilder.AddString(CastStr);
}

bool CastLocatorVisitor::VisitCastExpr(CastExpr *C) {
  ExprsWithCast.insert(C);
  if (!isa<ImplicitCastExpr>(C)) {
    Expr *Sub = ignoreCheckedCImplicit(C->getSubExpr());
    ExprsWithCast.insert(Sub);
  }
  return true;
}

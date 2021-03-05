//=--DeclRewriter.cpp---------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/3C/DeclRewriter.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/MappingVisitor.h"
#include "clang/3C/RewriteUtils.h"
#include "clang/3C/StructInit.h"
#include "clang/3C/Utils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

#ifdef FIVE_C
#include "clang/3C/DeclRewriter_5C.h"
#endif

using namespace llvm;
using namespace clang;

// This function is the public entry point for declaration rewriting.
void DeclRewriter::rewriteDecls(ASTContext &Context, ProgramInfo &Info,
                                Rewriter &R) {
  // Compute the bounds information for all the array variables.
  ArrayBoundsRewriter ABRewriter(Info);

  // Collect function and record declarations that need to be rewritten in a set
  // as well as their rewriten types in a map.
  RSet RewriteThese(DComp(Context.getSourceManager()));

  FunctionDeclBuilder *TRV = nullptr;
#ifdef FIVE_C
  auto TRV5C = FunctionDeclBuilder5C(&Context, Info, RewriteThese, NewFuncSig,
                                     ABRewriter);
  TRV = &TRV5C;
#else
  auto TRV3C =
      FunctionDeclBuilder(&Context, Info, RewriteThese, NewFuncSig, ABRewriter);
  TRV = &TRV3C;
#endif
  StructVariableInitializer SVI =
      StructVariableInitializer(&Context, Info, RewriteThese);
  for (const auto &D : Context.getTranslationUnitDecl()->decls()) {
    TRV->TraverseDecl(D);
    SVI.TraverseDecl(D);
    if (const auto &TD = dyn_cast<TypedefDecl>(D)) {
      auto PSL = PersistentSourceLoc::mkPSL(TD, Context);
      if (!TD->getUnderlyingType()->isBuiltinType()) { // Don't rewrite base types like int
        const auto O = Info.lookupTypedef(PSL);
        if (O.hasValue()) {
          const auto &Var = O.getValue();
          const auto &Env = Info.getConstraints().getVariables();
          if (Var.anyChanges(Env)) {
            std::string newTy =
                  getStorageQualifierString(D) +
                  Var.mkString(Info.getConstraints().getVariables(), true,
                                   false, false, true);
              RewriteThese.insert(
                  new TypedefDeclReplacement(TD, nullptr, newTy));
            }
        }
      }
    }
  }

  // Build a map of all of the PersistentSourceLoc's back to some kind of
  // Stmt, Decl, or Type.
  TranslationUnitDecl *TUD = Context.getTranslationUnitDecl();
  std::set<PersistentSourceLoc> Keys;
  for (const auto &I : Info.getVarMap())
    Keys.insert(I.first);
  MappingVisitor MV(Keys, Context);
  LastRecordDecl = nullptr;
  for (const auto &D : TUD->decls()) {
    MV.TraverseDecl(D);
    detectInlineStruct(D, Context.getSourceManager());
    if (FunctionDecl *FD = dyn_cast<FunctionDecl>(D)) {
      if (FD->hasBody() && FD->isThisDeclarationADefinition()) {
        for (auto &D : FD->decls()) {
          detectInlineStruct(D, Context.getSourceManager());
        }
      }
    }
  }
  SourceToDeclMapType PSLMap;
  VariableDecltoStmtMap VDLToStmtMap;
  std::tie(PSLMap, VDLToStmtMap) = MV.getResults();

  // Add declarations from this map into the rewriting set
  for (const auto &V : Info.getVarMap()) {
    // PLoc specifies the location of the variable whose type it is to
    // re-write, but not where the actual type storage is. To get that, we
    // need to turn PLoc into a Decl and then get the SourceRange for the
    // type of the Decl. Note that what we need to get is the ExpansionLoc
    // of the type specifier, since we want where the text is printed before
    // the variable name, not the typedef or #define that creates the
    // name of the type.
    PersistentSourceLoc PLoc = V.first;
    if (Decl *D = std::get<1>(PSLMap[PLoc])) {
      ConstraintVariable *CV = V.second;
      PVConstraint *PV = dyn_cast<PVConstraint>(CV);
      FVConstraint *FV = dyn_cast<FVConstraint>(CV);

      if (PV && PV->anyChanges(Info.getConstraints().getVariables()) &&
          !PV->isPartOfFunctionPrototype()) {
        // Rewrite a declaration, only if it is not part of function prototype.
        DeclStmt *DS = nullptr;
        if (VDLToStmtMap.find(D) != VDLToStmtMap.end())
          DS = VDLToStmtMap[D];

        std::string NewTy = getStorageQualifierString(D) +
                            PV->mkString(Info.getConstraints().getVariables()) +
                            ABRewriter.getBoundsString(PV, D);
        if (auto *VD = dyn_cast<VarDecl>(D))
          RewriteThese.insert(new VarDeclReplacement(VD, DS, NewTy));
        else if (auto *FD = dyn_cast<FieldDecl>(D))
          RewriteThese.insert(new FieldDeclReplacement(FD, DS, NewTy));
        else if (auto *PD = dyn_cast<ParmVarDecl>(D))
          RewriteThese.insert(new ParmVarDeclReplacement(PD, DS, NewTy));
        else
          llvm_unreachable("Unrecognized declaration type.");
      } else if (FV && NewFuncSig.find(FV->getName()) != NewFuncSig.end() &&
                 !TRV->isFunctionVisited(FV->getName())) {
        auto *FD = cast<FunctionDecl>(D);
        // TODO: I don't think this branch is ever reached. Either remove it or
        //       add a test case that reaches it.
        // If this function already has a modified signature? and it is not
        // visited by our cast placement visitor then rewrite it.
        std::string NewSig = NewFuncSig[FV->getName()];
        RewriteThese.insert(
            new FunctionDeclReplacement(FD, NewSig, true, true));
      }
    }
  }

  // Build sets of variables that are declared in the same statement so we can
  // rewrite things like int x, *y, **z;
  GlobalVariableGroups GVG(R.getSourceMgr());
  for (const auto &D : TUD->decls()) {
    GVG.addGlobalDecl(dyn_cast<VarDecl>(D));
    //Search through the AST for fields that occur on the same line
    FieldFinder::gatherSameLineFields(GVG, D);
  }

  // Do the declaration rewriting
  DeclRewriter DeclR(R, Context, GVG);
  DeclR.rewrite(RewriteThese);

  for (const auto *R : RewriteThese)
    delete R;
}

void DeclRewriter::rewrite(RSet &ToRewrite) {
  for (auto *const N : ToRewrite) {
    assert(N->getDecl() != nullptr);

    if (Verbose) {
      errs() << "Replacing type of decl:\n";
      N->getDecl()->dump();
      errs() << "with " << N->getReplacement() << "\n";
    }

    // Exact rewriting procedure depends on declaration type
    if (auto *PVR = dyn_cast<ParmVarDeclReplacement>(N)) {
      assert(N->getStatement() == nullptr);
      rewriteParmVarDecl(PVR);
    } else if (auto *VR = dyn_cast<VarDeclReplacement>(N)) {
      rewriteFieldOrVarDecl(VR, ToRewrite);
    } else if (auto *FR = dyn_cast<FunctionDeclReplacement>(N)) {
      rewriteFunctionDecl(FR);
    } else if (auto *FdR = dyn_cast<FieldDeclReplacement>(N)) {
      rewriteFieldOrVarDecl(FdR, ToRewrite);
    } else if (auto *TDR = dyn_cast<TypedefDeclReplacement>(N)) {
      rewriteTypedefDecl(TDR, ToRewrite);
    } else {
      assert(false && "Unknown replacement type");
    }
  }
}

void DeclRewriter::rewriteParmVarDecl(ParmVarDeclReplacement *N) {
  // First, find all the declarations of the containing function.
  DeclContext *DF = N->getDecl()->getParentFunctionOrMethod();
  assert(DF != nullptr && "no parent function or method for decl");
  FunctionDecl *FD = cast<FunctionDecl>(DF);

  // For each function, determine which parameter in the declaration
  // matches PV, then, get the type location of that parameter
  // declaration and re-write.
  unsigned int PIdx = getParameterIndex(N->getDecl(), FD);

  for (auto *CurFD = FD; CurFD != nullptr; CurFD = CurFD->getPreviousDecl())
    if (PIdx < CurFD->getNumParams()) {
      ParmVarDecl *Rewrite = CurFD->getParamDecl(PIdx);
      assert(Rewrite != nullptr);
      SourceRange TR = Rewrite->getSourceRange();
      rewriteSourceRange(R, TR, N->getReplacement());
    }
}


void DeclRewriter::rewriteTypedefDecl(TypedefDeclReplacement *TDR, RSet &ToRewrite) {
  rewriteSingleDecl(TDR, ToRewrite);
}

template <typename DRType>
void DeclRewriter::rewriteFieldOrVarDecl(DRType *N, RSet &ToRewrite) {
  static_assert(std::is_same<DRType, FieldDeclReplacement>::value ||
                    std::is_same<DRType, VarDeclReplacement>::value,
                "Method expects variable or field declaration replacement.");

  if (InlineVarDecls.find(N->getDecl()) != InlineVarDecls.end() &&
      VisitedMultiDeclMembers.find(N) == VisitedMultiDeclMembers.end()) {
    std::vector<Decl *> SameLineDecls;
    getDeclsOnSameLine(N, SameLineDecls);
    if (std::find(SameLineDecls.begin(), SameLineDecls.end(),
                  VDToRDMap[N->getDecl()]) == SameLineDecls.end())
      SameLineDecls.insert(SameLineDecls.begin(), VDToRDMap[N->getDecl()]);
    rewriteMultiDecl(N, ToRewrite, SameLineDecls, true);
  } else if (isSingleDeclaration(N)) {
    rewriteSingleDecl(N, ToRewrite);
  } else if (VisitedMultiDeclMembers.find(N) == VisitedMultiDeclMembers.end()) {
    std::vector<Decl *> SameLineDecls;
    getDeclsOnSameLine(N, SameLineDecls);
    rewriteMultiDecl(N, ToRewrite, SameLineDecls, false);
  } else {
    // Anything that reaches this case should be a multi-declaration that has
    // already been rewritten.
    assert("Declaration should have been rewritten." &&
           !isSingleDeclaration(N) &&
           VisitedMultiDeclMembers.find(N) != VisitedMultiDeclMembers.end());
  }
}

void DeclRewriter::rewriteSingleDecl(DeclReplacement *N, RSet &ToRewrite) {
  bool IsSingleDecl =
      dyn_cast<TypedefDecl>(N->getDecl()) || isSingleDeclaration(N);
  assert("Declaration is not a single declaration." && IsSingleDecl);
  // This is the easy case, we can rewrite it locally, at the declaration.
  SourceRange TR = N->getDecl()->getSourceRange();
  doDeclRewrite(TR, N);
}

void DeclRewriter::rewriteMultiDecl(DeclReplacement *N, RSet &ToRewrite,
                                    std::vector<Decl *> SameLineDecls,
                                    bool ContainsInlineStruct) {
  // Rewriting is more difficult when there are multiple variables declared in a
  // single statement. When this happens, we need to find all the declaration
  // replacement for this statement and apply them at the same time. We also
  // need to avoid rewriting any of these declarations twice by updating the
  // Skip set to include the processed declarations.

  // Step 1: get declaration replacement in the same statement
  RSet RewritesForThisDecl(DComp(R.getSourceMgr()));
  auto I = ToRewrite.find(N);
  while (I != ToRewrite.end()) {
    if (areDeclarationsOnSameLine(N, *I)) {
      assert("Unexpected DeclReplacement kind." &&
             (*I)->getKind() == N->getKind());
      RewritesForThisDecl.insert(*I);
    }
    ++I;
  }

  // Step 2: For each decl in the original, build up a new string. If the
  //         original decl was re-written, write that out instead. Existing
  //         initializers are preserved, any declarations that an initializer to
  //         be valid checked-c are given one.

  bool IsFirst = true;
  SourceLocation PrevEnd;
  for (const auto &DL : SameLineDecls) {
    std::string ReplaceText = ";\n";
    // Find the declaration replacement object for the current declaration.
    DeclReplacement *SameLineReplacement;
    bool Found = false;
    for (const auto &NLT : RewritesForThisDecl)
      if (NLT->getDecl() == DL) {
        SameLineReplacement = NLT;
        Found = true;
        break;
      }

    if (IsFirst && ContainsInlineStruct) {
      // If it is an inline struct, the first thing we have to do
      // is separate the RecordDecl from the VarDecl.
      ReplaceText = "};\n";
    } else if (IsFirst) {
      // Rewriting the first declaration is easy. Nothing should change if its
      // type does not to be rewritten. When rewriting is required, it is
      // essentially the same as the single declaration case.
      IsFirst = false;
      if (Found) {
        SourceRange SR(DL->getBeginLoc(), DL->getEndLoc());
        doDeclRewrite(SR, SameLineReplacement);
      }
    } else {
      // The subsequent decls are more complicated because we need to insert a
      // type string even if the variables type hasn't changed.
      if (Found) {
        // If the type has changed, the DeclReplacement object has a replacement
        // string stored in it that should be used.
        SourceRange SR(PrevEnd, DL->getEndLoc());
        doDeclRewrite(SR, SameLineReplacement);
      } else {
        // When the type hasn't changed, we still need to insert the original
        // type for the variable.

        // This is a bit of trickery needed to get a string representation of
        // the declaration without the initializer. We don't want to rewrite to
        // initializer because this causes problems when rewriting casts and
        // generic function calls later on. (issue 267)
        auto *VD = dyn_cast<VarDecl>(DL);
        Expr *Init = nullptr;
        if (VD && VD->hasInit()) {
          Init = VD->getInit();
          VD->setInit(nullptr);
        }

        // Dump the declaration (without the initializer) to a string. Printing
        // the AST node gives the full declaration including the base type which
        // is not present in the multi-decl source code.
        std::string DeclStr = "";
        raw_string_ostream DeclStream(DeclStr);
        DL->print(DeclStream);
        assert("Original decl string empty." && !DeclStream.str().empty());

        // Do the replacement. PrevEnd is setup to be the source location of the
        // comma after the previous declaration in the multi-decl. getEndLoc is
        // either the end of the declaration or just before the initializer if
        // one is present.
        SourceRange SR(PrevEnd, DL->getEndLoc());
        rewriteSourceRange(R, SR, DeclStream.str());

        // Undo prior trickery. This need to happen so that the PSL for the decl
        // is not changed since the PSL is used as a map key in a few places.
        if (VD && Init)
          VD->setInit(Init);
      }
    }

    SourceRange End;
    // In the event that IsFirst was not set to false, that implies we are
    // separating the RecordDecl and VarDecl, so instead of searching for
    // the next comma, we simply specify the end of the RecordDecl.
    if (IsFirst) {
      IsFirst = false;
      End = DL->getEndLoc();
    }
    // Variables in a mutli-decl are delimited by commas. The rewritten decls
    // are separate statements separated by a semicolon and a newline.
    else
      End = getNextCommaOrSemicolon(DL->getEndLoc());
    rewriteSourceRange(R, End, ReplaceText);
    // Offset by one to skip past what we've just added so it isn't overwritten.
    PrevEnd = End.getEnd().getLocWithOffset(1);
  }

  // Step 3: Be sure and skip all of the declarations that we just dealt with by
  //         adding them to the skip set.
  for (const auto &TN : RewritesForThisDecl)
    VisitedMultiDeclMembers.insert(TN);
}

// Common rewriting logic used to replace a single decl either on its own or as
// part of a multi decl. The primary responsibility of this method (aside from
// invoking the rewriter) is to add any required initializer expression.
void DeclRewriter::doDeclRewrite(SourceRange &SR, DeclReplacement *N) {
  std::string Replacement = N->getReplacement();
  if (isa<TypedefDecl>(N->getDecl()))
    Replacement = "typedef " + Replacement;
  if (auto *VD = dyn_cast<VarDecl>(N->getDecl())) {
    if (VD->hasInit()) {
      // Make sure we preserve any existing initializer
      SR.setEnd(VD->getInitializerStartLoc());
      Replacement += " =";
    } else {
      // There is no initializer. Add it if we need one.
      // MWH -- Solves issue 43. Should make it so we insert NULL if stdlib.h or
      // stdlib_checked.h is included
      if (VD->getStorageClass() != StorageClass::SC_Extern) {
        const std::string NullPtrStr = "((void *)0)";
        if (isPointerType(VD)) {
          Replacement += " = " + NullPtrStr;
        } else if (VD->getType()->isArrayType()) {
          const auto *ElemType = VD->getType()->getPointeeOrArrayElementType();
          if (ElemType->isPointerType())
            Replacement += " = {" + NullPtrStr + "}";
        }
      }
    }
  }

  rewriteSourceRange(R, SR, Replacement);
}

void DeclRewriter::rewriteFunctionDecl(FunctionDeclReplacement *N) {
  rewriteSourceRange(R, N->getSourceRange(A.getSourceManager()),
                     N->getReplacement());
}

// A function to detect the presence of inline struct declarations
// by tracking VarDecls and RecordDecls and populating data structures
// later used in rewriting.

// These variables are duplicated in the header file and here because static
// vars need to be initialized in the cpp file where the class is defined.
/*static*/ RecordDecl *DeclRewriter::LastRecordDecl = nullptr;
/*static*/ std::map<Decl *, Decl *> DeclRewriter::VDToRDMap;
/*static*/ std::set<Decl *> DeclRewriter::InlineVarDecls;
void DeclRewriter::detectInlineStruct(Decl *D, SourceManager &SM) {
  RecordDecl *RD = dyn_cast<RecordDecl>(D);
  if (RD != nullptr &&
      // With -fms-extensions (default on Windows), Clang injects an implicit
      // `struct _GUID` with an invalid location, which would cause an assertion
      // failure in SM.isPointWithin below.
      RD->getBeginLoc().isValid()) {
    LastRecordDecl = RD;
  }
  if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
    if (LastRecordDecl != nullptr) {
      auto LastRecordLocation = LastRecordDecl->getBeginLoc();
      auto Begin = VD->getBeginLoc();
      auto End = VD->getEndLoc();
      bool IsInLineStruct = SM.isPointWithin(LastRecordLocation, Begin, End);
      bool IsNamedInLineStruct =
          IsInLineStruct && LastRecordDecl->getNameAsString() != "";
      if (IsNamedInLineStruct) {
        VDToRDMap[VD] = LastRecordDecl;
        InlineVarDecls.insert(VD);
      }
    }
  }
}

// Uses clangs lexer to find the location of the next comma or semicolon after
// the given source location. This is used to find the end of each declaration
// within a multi-declaration.
SourceRange DeclRewriter::getNextCommaOrSemicolon(SourceLocation L) {
  SourceManager &SM = A.getSourceManager();
  auto Tok = Lexer::findNextToken(L, SM, A.getLangOpts());
  while (Tok.hasValue() && !Tok->is(clang::tok::eof)) {
    if (Tok->is(clang::tok::comma) || Tok->is(clang::tok::semi))
      return SourceRange(Tok->getLocation(), Tok->getLocation());
    Tok = Lexer::findNextToken(Tok->getEndLoc(), A.getSourceManager(),
                               A.getLangOpts());
  }
  llvm_unreachable("Unable to find comma or semicolon at source location.");
}

bool DeclRewriter::areDeclarationsOnSameLine(DeclReplacement *N1,
                                             DeclReplacement *N2) {
  Decl *D1 = N1->getDecl();
  Decl *D2 = N2->getDecl();
  if (D1 && D2) {
    // In the event that this is a FieldDecl,
    // these statements will always be null
    DeclStmt *Stmt1 = N1->getStatement();
    DeclStmt *Stmt2 = N2->getStatement();
    if (Stmt1 == nullptr && Stmt2 == nullptr) {
      auto &DGroup = GP.getVarsOnSameLine(D1);
      return llvm::is_contained(DGroup, D2);
    }
    if (Stmt1 == nullptr || Stmt2 == nullptr) {
      return false;
    }
    return Stmt1 == Stmt2;
  }
  return false;
}

bool DeclRewriter::isSingleDeclaration(DeclReplacement *N) {
  DeclStmt *Stmt = N->getStatement();
  if (Stmt == nullptr) {
    auto &VDGroup = GP.getVarsOnSameLine(N->getDecl());
    return VDGroup.size() == 1;
  }
  return Stmt->isSingleDecl();
}

void DeclRewriter::getDeclsOnSameLine(DeclReplacement *N,
                                      std::vector<Decl *> &Decls) {
  if (N->getStatement() != nullptr) {
    Decls.insert(Decls.begin(), N->getStatement()->decls().begin(),
                 N->getStatement()->decls().end());
  } else {
    std::vector<Decl *> GlobalLine = GP.getVarsOnSameLine(N->getDecl());
    Decls.insert(Decls.begin(), GlobalLine.begin(), GlobalLine.end());
  }

  assert("Invalid ordering in same line decls" &&
         std::is_sorted(Decls.begin(), Decls.end(), [&](Decl *D0, Decl *D1) {
           return A.getSourceManager().isBeforeInTranslationUnit(
               D0->getEndLoc(), D1->getEndLoc());
         }));
}

// Note: This is variable declared static in the header file in order to pass
// information between different invocations on different translation units.
std::map<std::string, std::string> DeclRewriter::NewFuncSig;

// This function checks how to re-write a function declaration.
bool FunctionDeclBuilder::VisitFunctionDecl(FunctionDecl *FD) {

  // Get the constraint variable for the function.
  // For the return value and each of the parameters, do the following:
  //   1. Get a constraint variable representing the definition (def) and the
  //      uses ("arguments").
  //   2. If arguments could be wild but def is not, we insert a bounds-safe
  //      interface.
  // If we don't have a definition in scope, we can assert that all of
  // the constraint variables are equal.
  // Finally, we need to note that we've visited this particular function, and
  // that we shouldn't make one of these visits again.

  auto FuncName = FD->getNameAsString();

  // Do we have a definition for this function?
  FunctionDecl *Definition = getDefinition(FD);
  if (Definition == nullptr)
    Definition = FD;

  // Make sure we haven't visited this function name before, and that we
  // only visit it once.
  if (isFunctionVisited(FuncName))
    return true;
  VisitedSet.insert(FuncName);

  FVConstraint *Defnc = Info.getFuncConstraint(Definition, Context);
  if (!Defnc)
    return true;

  // If this is an external function, there is no need to rewrite the
  // declaration. We cannot change the signature of external functions.
  if (!Defnc->hasBody())
    return true;

  // RewriteParams and RewriteReturn track if we will need to rewrite the
  // parameter and return type declarations on this function. They are first
  // set to true if any changes are made to the types of the parameter and
  // return. If a type has changed, then it must be rewritten. There are then
  // some special circumstances which require rewriting the parameter or return
  // even when the type as not changed.
  bool RewriteParams = false;
  bool RewriteReturn = false;

  // Get rewritten parameter variable declarations.
  std::vector<std::string> ParmStrs;
  for (unsigned I = 0; I < Defnc->numParams(); ++I) {
    PVConstraint *ExtCV = Defnc->getExternalParam(I);
    PVConstraint *IntCV = Defnc->getInternalParam(I);
    ParmVarDecl *PVDecl = Definition->getParamDecl(I);
    std::string Type, IType;
    this->buildDeclVar(IntCV, ExtCV, PVDecl, Type, IType, RewriteParams,
                       RewriteReturn);
    ParmStrs.push_back(Type + IType);
  }

  if (Defnc->numParams() == 0) {
    ParmStrs.push_back("void");
    QualType ReturnTy = FD->getReturnType();
    QualType Ty = FD->getType();
    if (!Ty->isFunctionProtoType() && ReturnTy->isPointerType())
      RewriteParams = true;
  }

  // Get rewritten return variable.
  std::string ReturnVar, ItypeStr;
  this->buildDeclVar(Defnc->getInternalReturn(), Defnc->getExternalReturn(), FD,
                     ReturnVar, ItypeStr, RewriteParams, RewriteReturn);

  // If the return is a function pointer, we need to rewrite the whole
  // declaration even if no actual changes were made to the parameters because
  // the parameter for the function pointer type appear later in the source than
  // the parameters for the function declaration. It could probably be done
  // better, but getting the correct source locations is painful.
  if (FD->getReturnType()->isFunctionPointerType() && RewriteReturn)
    RewriteParams = true;

  // If the function is declared using a typedef for the function type, then we
  // need to rewrite parameters and the return if either would have been
  // rewritten. What this does is expand the typedef to the full function type
  // to avoid the problem of rewriting inside the typedef.
  // FIXME: If issue #437 is fixed in way that preserves typedefs on function
  //        declarations, then this conditional should be removed to enable
  //        separate rewriting of return type and parameters on the
  //        corresponding definition.
  //        https://github.com/correctcomputation/checkedc-clang/issues/437
  if ((RewriteReturn || RewriteParams) && hasDeclWithTypedef(FD)) {
    RewriteParams = true;
    RewriteReturn = true;
  }

  // Combine parameter and return variables rewritings into a single rewriting
  // for the entire function declaration.
  std::string NewSig = "";
  if (RewriteReturn)
    NewSig = getStorageQualifierString(Definition) + ReturnVar;

  if (RewriteReturn && RewriteParams)
    NewSig += Defnc->getName();

  if (RewriteParams && !ParmStrs.empty()) {
    // Gather individual parameter strings into a single buffer
    std::ostringstream ConcatParamStr;
    copy(ParmStrs.begin(), ParmStrs.end() - 1,
         std::ostream_iterator<std::string>(ConcatParamStr, ", "));
    ConcatParamStr << ParmStrs.back();

    NewSig += "(" + ConcatParamStr.str();
    // Add varargs.
    if (functionHasVarArgs(Definition))
      NewSig += ", ...";
    NewSig += ")";
  }
  if (!ItypeStr.empty())
    NewSig = NewSig + ItypeStr;

  // Add new declarations to RewriteThese if it has changed
  if (RewriteReturn || RewriteParams) {
    for (auto *const RD : Definition->redecls())
      RewriteThese.insert(new FunctionDeclReplacement(RD, NewSig, RewriteReturn,
                                                      RewriteParams));
    // Save the modified function signature.
    if (FD->isStatic()) {
      auto FileName = PersistentSourceLoc::mkPSL(FD, *Context).getFileName();
      FuncName = FileName + "::" + FuncName;
    }
    ModifiedFuncSignatures[FuncName] = NewSig;
  }

  return true;
}

void FunctionDeclBuilder::buildCheckedDecl(
    PVConstraint *Defn, DeclaratorDecl *Decl, std::string &Type,
    std::string &IType, bool &RewriteParm, bool &RewriteRet) {
  Type = Defn->mkString(Info.getConstraints().getVariables());
  IType = getExistingIType(Defn);
  IType += ABRewriter.getBoundsString(Defn, Decl, !IType.empty());
  RewriteParm |= !IType.empty() || isa<ParmVarDecl>(Decl);
  RewriteRet |= isa<FunctionDecl>(Decl);
  return;
}

void FunctionDeclBuilder::buildItypeDecl(PVConstraint *Defn,
                                         DeclaratorDecl *Decl,
                                         std::string &Type, std::string &IType,
                                         bool &RewriteParm, bool &RewriteRet) {
  Type = Defn->getRewritableOriginalTy();
  if (isa<ParmVarDecl>(Decl))
    Type += Defn->getName();
  IType = " : itype(" +
          Defn->mkString(Info.getConstraints().getVariables(), false, true) +
          ")" + ABRewriter.getBoundsString(Defn, Decl, true);
  RewriteParm = true;
  RewriteRet |= isa<FunctionDecl>(Decl);
  return;
}

// Note: For a parameter, Type + IType will give the full declaration (including
// the name) but the breakdown between Type and IType is not guaranteed. For a
// return, Type will be what goes before the name and IType will be what goes
// after the parentheses.
void FunctionDeclBuilder::buildDeclVar(PVConstraint *IntCV, PVConstraint *ExtCV,
                                       DeclaratorDecl *Decl, std::string &Type,
                                       std::string &IType, bool &RewriteParm,
                                       bool &RewriteRet) {
  const auto &Env = Info.getConstraints().getVariables();
  // If the external constraint variable is checked, then the parameter should
  // be advertised as checked to callers. This requires adding either an itype
  // or a checked type. If the constraint variable type did not change, then
  // the type does not need to be rewritten. The type in the source is correct.
  if (isAValidPVConstraint(ExtCV) && ExtCV->isChecked(Env) &&
      ExtCV->anyChanges(Env)) {
    // If the internal and external constraint variables solve to the same type,
    // then they are both checked and we can use a _Ptr type. Otherwise, an
    // itype is used.
    if (IntCV->solutionEqualTo(Info.getConstraints(), ExtCV))
      buildCheckedDecl(ExtCV, Decl, Type, IType, RewriteParm, RewriteRet);
    else
      buildItypeDecl(ExtCV, Decl, Type, IType, RewriteParm, RewriteRet);
    return;
  }
  // Variables that do not need to be rewritten fall through to here.
  // For parameter variables, we try to extract the declaration from the source
  // code. This preserves macros and other formatting. This isn't possible for
  // return variables because the itype on returns is located after the
  // parameter list. Sometimes we cannot get the original source for a parameter
  // declaration, for example if a function prototype is declared using a
  // typedef or the parameter declaration is inside a macro. For these cases, we
  // just fall back to reconstructing the declaration from the PVConstraint.
  ParmVarDecl *PVD = dyn_cast<ParmVarDecl>(Decl);
  if (PVD) {
    SourceRange Range = PVD->getSourceRange();
    if (Range.isValid()) {
      Type = getSourceText(Range, *Context);
      if (!Type.empty()) {
        // Great, we got the original source including any itype and bounds.
        IType = "";
        return;
      }
    }
    // Otherwise, reconstruct the name and type, and reuse the code below for
    // the itype and bounds.
    // TODO: Do we care about `register` or anything else this doesn't handle?
    Type = qtyToStr(PVD->getOriginalType(), PVD->getNameAsString());
  } else {
    Type = ExtCV->getOriginalTy() + " ";
  }
  IType = getExistingIType(ExtCV);
  IType += ABRewriter.getBoundsString(ExtCV, Decl, !IType.empty());
}

std::string FunctionDeclBuilder::getExistingIType(ConstraintVariable *DeclC) {
  auto *PVC = dyn_cast<PVConstraint>(DeclC);
  if (PVC != nullptr && !PVC->getItype().empty())
    return " : " + PVC->getItype();
  return "";
}

// Check if the function is handled by this visitor.
bool FunctionDeclBuilder::isFunctionVisited(std::string FuncName) {
  return VisitedSet.find(FuncName) != VisitedSet.end();
}

// Given a function declaration figure out if this declaration or any other
// declaration of the same function is declared using a typedefed function type.
bool FunctionDeclBuilder::hasDeclWithTypedef(const FunctionDecl *FD) {
  for (FunctionDecl *FDIter : FD->redecls()) {
    // If the declaration type is TypedefType, then this is definitely declared
    // using a typedef. This only happens when the typedefed declaration is the
    // first declaration of a function.
    if (isa_and_nonnull<TypedefType>(FDIter->getType().getTypePtrOrNull()))
      return true;
    // Next look for a TypeDefTypeLoc. This is present on the typedefed
    // declaration even when it is not the first declaration.
    TypeSourceInfo *TSI = FDIter->getTypeSourceInfo();
    if (TSI) {
      if (!TSI->getTypeLoc().getAs<TypedefTypeLoc>().isNull())
        return true;
    } else {
      // This still could possibly be a typedef type if TSI was NULL.
      // TypeSourceInfo is null for implicit function declarations, so if a
      // implicit declaration uses a typedef, it will be missed. That's fine
      // since an implicit declaration can't be rewritten anyways.
      // There might be other ways it can be null that I'm not aware of.
      if (Verbose) {
        llvm::errs() << "Unable to conclusively determine if a function "
                     << "declaration uses a typedef.\n";
        FDIter->dump();
      }
    }
  }
  return false;
}

bool FieldFinder::VisitFieldDecl(FieldDecl *FD) {
  GVG.addGlobalDecl(FD);
  return true;
}

void FieldFinder::gatherSameLineFields(GlobalVariableGroups &GVG, Decl *D) {
  FieldFinder FF(GVG);
  FF.TraverseDecl(D);
}

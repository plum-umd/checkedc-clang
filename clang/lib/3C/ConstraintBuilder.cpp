//=--ConstraintBuilder.cpp----------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Implementation of visitor methods for the FunctionVisitor class. These
// visitors create constraints based on the AST of the program.
//===----------------------------------------------------------------------===//

#include "clang/3C/ConstraintBuilder.h"
#include "clang/3C/3CGlobalOptions.h"
#include "clang/3C/ArrayBoundsInferenceConsumer.h"
#include "clang/3C/ConstraintResolver.h"
#include "clang/3C/TypeVariableAnalysis.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include <algorithm>

using namespace llvm;
using namespace clang;

// This class is intended to locate inline struct definitions
// in order to mark them wild or signal a warning as appropriate.
class InlineStructDetector {
public:
  explicit InlineStructDetector() : LastRecordDecl(nullptr) {}

  void processRecordDecl(RecordDecl *Declaration, ProgramInfo &Info,
                         ASTContext *Context, ConstraintResolver CB) {
    LastRecordDecl = Declaration;
    if (RecordDecl *Definition = Declaration->getDefinition()) {
      auto LastRecordLocation = Definition->getBeginLoc();
      FullSourceLoc FL = Context->getFullLoc(Definition->getBeginLoc());
      if (FL.isValid()) {
        SourceManager &SM = Context->getSourceManager();
        FileID FID = FL.getFileID();
        const FileEntry *FE = SM.getFileEntryForID(FID);

        // Detect whether this RecordDecl is part of an inline struct.
        bool IsInLineStruct = false;
        Decl *D = Declaration->getNextDeclInContext();
        if (VarDecl *VD = dyn_cast_or_null<VarDecl>(D)) {
          auto VarTy = VD->getType();
          auto BeginLoc = VD->getBeginLoc();
          auto EndLoc = VD->getEndLoc();
          SourceManager &SM = Context->getSourceManager();
          IsInLineStruct =
              !isPtrOrArrayType(VarTy) && !VD->hasInit() &&
              SM.isPointWithin(LastRecordLocation, BeginLoc, EndLoc);
        }

        if (FE && FE->isValid()) {
          // We only want to re-write a record if it contains
          // any pointer types, to include array types.
          for (const auto &F : Definition->fields()) {
            auto FieldTy = F->getType();
            // If the RecordDecl is a union or in a system header
            // and this field is a pointer, we need to mark it wild.
            bool FieldInUnionOrSysHeader =
                (FL.isInSystemHeader() || Definition->isUnion());
            // Mark field wild if the above is true and the field is a pointer.
            if (isPtrOrArrayType(FieldTy) &&
                (FieldInUnionOrSysHeader || IsInLineStruct)) {
              std::string Rsn = "Union or external struct field encountered";
              CVarOption CV = Info.getVariable(F, Context);
              CB.constraintCVarToWild(CV, Rsn);
            }
          }
        }
      }
    }
  }

  void processVarDecl(VarDecl *VD, ProgramInfo &Info, ASTContext *Context,
                      ConstraintResolver CB) {
    // If the last seen RecordDecl is non-null and coincides with the current
    // VarDecl (i.e. via an inline struct), we proceed as follows:
    // If the struct is named, do nothing.
    // If the struct is anonymous:
    //      When alltypes is on, do nothing, but signal a warning to
    //                           the user indicating its presence.
    //      When alltypes is off, mark the VarDecl WILD in order to
    //                           ensure the converted program compiles.
    if (LastRecordDecl != nullptr) {
      auto LastRecordLocation = LastRecordDecl->getBeginLoc();
      auto BeginLoc = VD->getBeginLoc();
      auto EndLoc = VD->getEndLoc();
      auto VarTy = VD->getType();
      SourceManager &SM = Context->getSourceManager();
      bool IsInLineStruct =
          SM.isPointWithin(LastRecordLocation, BeginLoc, EndLoc) &&
          isPtrOrArrayType(VarTy);
      bool IsNamedInLineStruct =
          IsInLineStruct && LastRecordDecl->getNameAsString() != "";
      if (IsInLineStruct && !IsNamedInLineStruct) {
        if (!AllTypes) {
          CVarOption CV = Info.getVariable(VD, Context);
          CB.constraintCVarToWild(CV, "Inline struct encountered.");
        } else {
          clang::DiagnosticsEngine &DE = Context->getDiagnostics();
          unsigned InlineStructWarning =
              DE.getCustomDiagID(DiagnosticsEngine::Warning,
                                 "\n Rewriting failed"
                                 "for %q0 because an inline "
                                 "or anonymous struct instance "
                                 "was detected.\n Consider manually "
                                 "rewriting by inserting the struct "
                                 "definition inside the _Ptr "
                                 "annotation.\n "
                                 "EX. struct {int *a; int *b;} x; "
                                 "_Ptr<struct {int *a; _Ptr<int> b;}>;");
          const auto Pointer = reinterpret_cast<intptr_t>(VD);
          const auto Kind =
              clang::DiagnosticsEngine::ArgumentKind::ak_nameddecl;
          auto DiagBuilder = DE.Report(VD->getLocation(), InlineStructWarning);
          DiagBuilder.AddTaggedVal(Pointer, Kind);
        }
      }
    }
  }

private:
  RecordDecl *LastRecordDecl;
};

// This class visits functions and adds constraints to the
// Constraints instance assigned to it.
// Each VisitXXX method is responsible for looking inside statements
// and imposing constraints on variables it uses
class FunctionVisitor : public RecursiveASTVisitor<FunctionVisitor> {
public:
  explicit FunctionVisitor(ASTContext *C, ProgramInfo &I, FunctionDecl *FD,
                           TypeVarInfo &TVI)
      : Context(C), Info(I), Function(FD), CB(Info, Context), TVInfo(TVI),
        ISD() {}

  // T x = e
  bool VisitDeclStmt(DeclStmt *S) {
    // Introduce variables as needed.
    for (const auto &D : S->decls()) {
      if (RecordDecl *RD = dyn_cast<RecordDecl>(D)) {
        ISD.processRecordDecl(RD, Info, Context, CB);
      }
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        if (VD->isLocalVarDecl()) {
          FullSourceLoc FL = Context->getFullLoc(VD->getBeginLoc());
          SourceRange SR = VD->getSourceRange();
          if (SR.isValid() && FL.isValid() && isPtrOrArrayType(VD->getType())) {
            ISD.processVarDecl(VD, Info, Context, CB);
          }
        }
      }
    }

    // Process inits even for non-pointers because structs and union values
    // can contain pointers
    for (const auto &D : S->decls()) {
      if (VarDecl *VD = dyn_cast<VarDecl>(D)) {
        Expr *InitE = VD->getInit();
        CB.constrainLocalAssign(S, VD, InitE, Same_to_Same);
      }
    }

    return true;
  }

  // (T)e
  bool VisitCStyleCastExpr(CStyleCastExpr *C) {
    // Is cast compatible with LHS type?
    QualType SrcT = C->getSubExpr()->getType();
    QualType DstT = C->getType();
    if (!isCastSafe(DstT, SrcT) && !Info.hasPersistentConstraints(C, Context)) {
      auto CVs = CB.getExprConstraintVarsSet(C->getSubExpr());
      std::string Rsn = "Cast from " + SrcT.getAsString() +  " to " +
                        DstT.getAsString();
      CB.constraintAllCVarsToWild(CVs, Rsn, C);
    }
    return true;
  }

  // x += e
  bool VisitCompoundAssignOperator(CompoundAssignOperator *O) {
    switch (O->getOpcode()) {
    case BO_AddAssign:
    case BO_SubAssign:
      arithBinop(O, true);
      break;
    // rest shouldn't happen on pointers, so we ignore
    default:
      break;
    }
    return true;
  }

  // e(e1,e2,...)
  bool VisitCallExpr(CallExpr *E) {
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(E, *Context);
    auto &CS = Info.getConstraints();
    CVarSet FVCons = CB.getCalleeConstraintVars(E);

    // When multiple function variables are used in the same expression, they
    // must have the same type.
    if (FVCons.size() > 1) {
      PersistentSourceLoc PL =
          PersistentSourceLoc::mkPSL(E->getCallee(), *Context);
      constrainConsVarGeq(FVCons, FVCons, Info.getConstraints(), &PL,
                          Same_to_Same, false, &Info);
    }

    Decl *D = E->getCalleeDecl();
    FunctionDecl *TFD = dyn_cast_or_null<FunctionDecl>(D);
    std::string FuncName = "";
    if (auto *DD = dyn_cast_or_null<DeclaratorDecl>(D))
      FuncName = DD->getNameAsString();

    // Collect type parameters for this function call that are
    // consistently instantiated as single type in this function call.
    std::set<unsigned int> ConsistentTypeParams;
    if (TFD != nullptr)
      TVInfo.getConsistentTypeParams(E, ConsistentTypeParams);

    // Now do the call: Constrain arguments to parameters (but ignore returns)
    if (FVCons.empty()) {
      // Don't know who we are calling; make args WILD
      constraintAllArgumentsToWild(E);
    } else if (!ConstraintResolver::canFunctionBeSkipped(FuncName)) {
      // FIXME: realloc comparison is still required. See issue #176.
      // If we are calling realloc, ignore it, so as not to constrain the first
      // arg. Else, for each function we are calling ...
      for (auto *TmpC : FVCons) {
        if (PVConstraint *PVC = dyn_cast<PVConstraint>(TmpC)) {
          TmpC = PVC->getFV();
          assert(TmpC != nullptr && "Function pointer with null FVConstraint.");
        }
        // and for each arg to the function ...
        if (FVConstraint *TargetFV = dyn_cast<FVConstraint>(TmpC)) {
          unsigned I = 0;
          for (const auto &A : E->arguments()) {
            CSetBkeyPair ArgumentConstraints;
            if (I < TargetFV->numParams()) {
              // Remove casts to void* on polymorphic types that are used
              // consistently.
              const int TyIdx =
                  TargetFV->getExternalParam(I)->getGenericIndex();
              if (ConsistentTypeParams.find(TyIdx) !=
                  ConsistentTypeParams.end())
                ArgumentConstraints =
                    CB.getExprConstraintVars(A->IgnoreImpCasts());
              else
                ArgumentConstraints = CB.getExprConstraintVars(A);
            } else
              ArgumentConstraints = CB.getExprConstraintVars(A);

            if (I < TargetFV->numParams()) {
              // Constrain the arg CV to the param CV.
              ConstraintVariable *ParameterDC = TargetFV->getExternalParam(I);

              // We cannot insert a cast if the source location of a call
              // expression is not writable. By using Same_to_Same for calls at
              // unwritable source locations, we ensure that we will not need to
              // insert a cast because this unifies the checked type for the
              // parameter and the argument.
              ConsAction CA = Rewriter::isRewritable(A->getExprLoc())
                              ? Wild_to_Safe : Same_to_Same;
              // Do not handle bounds key here because we will be
              // doing context-sensitive assignment next.
              constrainConsVarGeq(ParameterDC, ArgumentConstraints.first, CS,
                                  &PL, CA, false, &Info, false);

              if (AllTypes && TFD != nullptr && I < TFD->getNumParams()) {
                auto *PVD = TFD->getParamDecl(I);
                auto &CSBI = Info.getABoundsInfo().getCtxSensBoundsHandler();
                // Here, we need to handle context-sensitive assignment.
                CSBI.handleContextSensitiveAssignment(PL, PVD, ParameterDC, A,
                                                      ArgumentConstraints.first,
                                                      ArgumentConstraints.second,
                                                      Context, &CB);
              }
            } else {
              // The argument passed to a function ith varargs; make it wild
              if (HandleVARARGS) {
                CB.constraintAllCVarsToWild(ArgumentConstraints.first,
                                            "Passing argument to a function "
                                            "accepting var args.",
                                            E);
              } else {
                if (Verbose) {
                  std::string FuncName = TargetFV->getName();
                  errs() << "Ignoring function as it contains varargs:"
                         << FuncName << "\n";
                }
              }
            }
            I++;
          }
        }
      }
    }
    return true;
  }

  // e1[e2]
  bool VisitArraySubscriptExpr(ArraySubscriptExpr *E) {
    Constraints &CS = Info.getConstraints();
    constraintInBodyVariable(E->getBase(), CS.getArr());
    return true;
  }

  // return e;
  bool VisitReturnStmt(ReturnStmt *S) {
    // Get function variable constraint of the body
    PersistentSourceLoc PL = PersistentSourceLoc::mkPSL(S, *Context);
    CVarOption CVOpt = Info.getVariable(Function, Context);

    // Constrain the value returned (if present) against the return value
    // of the function.
    Expr *RetExpr = S->getRetValue();

    CVarSet RconsVar = CB.getExprConstraintVarsSet(RetExpr);
    // Constrain the return type of the function
    // to the type of the return expression.
    if (CVOpt.hasValue()) {
      if (FVConstraint *FV = dyn_cast<FVConstraint>(&CVOpt.getValue())) {
        // This is to ensure that the return type of the function is same
        // as the type of return expression.
        constrainConsVarGeq(FV->getInternalReturn(), RconsVar,
                            Info.getConstraints(), &PL, Same_to_Same, false,
                            &Info);
      }
    }
    return true;
  }

  // ++e, --e, e++, and e--
  bool VisitUnaryOperator(UnaryOperator *O) {
    switch (O->getOpcode()) {
    case clang::UO_PreInc:
    case clang::UO_PreDec:
    case clang::UO_PostInc:
    case clang::UO_PostDec:
      constraintPointerArithmetic(O->getSubExpr());
      break;
    default:
      break;
    }
    return true;
  }

  bool VisitBinaryOperator(BinaryOperator *O) {
    switch (O->getOpcode()) {
    // e1 + e2 and e1 - e2
    case clang::BO_Add:
    case clang::BO_Sub:
      arithBinop(O);
      break;
    // x = e
    case clang::BO_Assign:
      CB.constrainLocalAssign(O, O->getLHS(), O->getRHS(), Same_to_Same);
      break;
    default:
      break;
    }
    return true;
  }

private:
  // Constraint all the provided vars to be
  // equal to the provided type i.e., (V >= type).
  void constrainVarsTo(CVarSet &Vars, ConstAtom *CAtom) {
    Constraints &CS = Info.getConstraints();
    for (const auto &I : Vars)
      if (PVConstraint *PVC = dyn_cast<PVConstraint>(I)) {
        PVC->constrainOuterTo(CS, CAtom);
      }
  }

  // Constraint helpers.
  void constraintInBodyVariable(Expr *e, ConstAtom *CAtom) {
    CVarSet Var = CB.getExprConstraintVarsSet(e);
    constrainVarsTo(Var, CAtom);
  }

  // Constraint all the argument of the provided
  // call expression to be WILD.
  void constraintAllArgumentsToWild(CallExpr *E) {
    PersistentSourceLoc Psl = PersistentSourceLoc::mkPSL(E, *Context);
    for (const auto &A : E->arguments()) {
      // Get constraint from within the function body
      // of the caller.
      CVarSet ParameterEC = CB.getExprConstraintVarsSet(A);

      // Assign WILD to each of the constraint variables.
      FunctionDecl *FD = E->getDirectCallee();
      std::string Rsn = "Argument to function " +
                        (FD != nullptr ? FD->getName().str() : "pointer call");
      Rsn += " with out Constraint vars.";
      CB.constraintAllCVarsToWild(ParameterEC, Rsn, E);
    }
  }

  // Here the flag, ModifyingExpr indicates if the arithmetic operation
  // is modifying any variable.
  void arithBinop(BinaryOperator *O, bool ModifyingExpr = false) {
    constraintPointerArithmetic(O->getLHS(), ModifyingExpr);
    constraintPointerArithmetic(O->getRHS(), ModifyingExpr);
  }

  // Pointer arithmetic constrains the expression to be at least ARR,
  // unless it is on a function pointer. In this case the function pointer
  // is WILD.
  void constraintPointerArithmetic(Expr *E, bool ModifyingExpr = true) {
    if (E->getType()->isFunctionPointerType()) {
      CVarSet Var = CB.getExprConstraintVarsSet(E);
      std::string Rsn = "Pointer arithmetic performed on a function pointer.";
      CB.constraintAllCVarsToWild(Var, Rsn, E);
    } else {
      if (ModifyingExpr)
        Info.getABoundsInfo().recordArithmeticOperation(E, &CB);
      constraintInBodyVariable(E, Info.getConstraints().getArr());
    }
  }

  ASTContext *Context;
  ProgramInfo &Info;
  FunctionDecl *Function;
  ConstraintResolver CB;
  TypeVarInfo &TVInfo;
  InlineStructDetector ISD;
};

class PtrToStructDef : public RecursiveASTVisitor<PtrToStructDef> {
public:
  explicit PtrToStructDef(TypedefDecl *TDT) : TDT(TDT) {}

  bool VisitPointerType(clang::PointerType *PT) {
    IsPointer = true;
    return true;
  }

  bool VisitRecordType(RecordType *RT) {
    auto *Decl = RT->getDecl();
    auto DeclRange = Decl->getSourceRange();
    auto TypedefRange = TDT->getSourceRange();
    bool DeclContained = (TypedefRange.getBegin() < DeclRange.getBegin()) &&
                         !(TypedefRange.getEnd() < TypedefRange.getEnd());
    if (DeclContained) {
      StructDefInTD = true;
      return false;
    }
    return true;
  }

  bool VisitFunctionProtoType(FunctionProtoType *FPT) {
    IsPointer = true;
    return true;
  }

  bool getResult(void) { return StructDefInTD; }

  static bool containsPtrToStructDef(TypedefDecl *TDT) {
    PtrToStructDef Traverser(TDT);
    Traverser.TraverseDecl(TDT);
    return Traverser.getResult();
  }

private:
  TypedefDecl *TDT = nullptr;
  bool IsPointer = false;
  bool StructDefInTD = false;
};

// This class visits a global declaration, generating constraints
// for functions, variables, types, etc. that are visited.
class ConstraintGenVisitor : public RecursiveASTVisitor<ConstraintGenVisitor> {
public:
  explicit ConstraintGenVisitor(ASTContext *Context, ProgramInfo &I,
                                TypeVarInfo &TVI)
      : Context(Context), Info(I), CB(Info, Context), TVInfo(TVI), ISD() {}

  bool VisitVarDecl(VarDecl *G) {

    if (G->hasGlobalStorage() && isPtrOrArrayType(G->getType())) {
      if (G->hasInit()) {
        CB.constrainLocalAssign(nullptr, G, G->getInit(), Same_to_Same);
      }
      ISD.processVarDecl(G, Info, Context, CB);
    }
    return true;
  }

  bool VisitInitListExpr(InitListExpr *E) {
    if (E->getType()->isStructureType()) {
      E = E->getSemanticForm();
      const RecordDecl *Definition =
          E->getType()->getAsStructureType()->getDecl()->getDefinition();

      unsigned int InitIdx = 0;
      const auto Fields = Definition->fields();
      for (auto It = Fields.begin();
           InitIdx < E->getNumInits() && It != Fields.end(); InitIdx++, It++) {
        Expr *InitExpr = E->getInit(InitIdx);
        CB.constrainLocalAssign(nullptr, *It, InitExpr, Same_to_Same, true);
      }
    }
    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());

    if (Verbose)
      errs() << "Analyzing function " << D->getName() << "\n";

    if (FL.isValid()) { // TODO: When would this ever be false?
      if (D->hasBody() && D->isThisDeclarationADefinition()) {
        Stmt *Body = D->getBody();
        FunctionVisitor FV = FunctionVisitor(Context, Info, D, TVInfo);
        FV.TraverseStmt(Body);
        if (AllTypes) {
          // Only do this, if all types is enabled.
          LengthVarInference LVI(Info, Context, D);
          LVI.Visit(Body);
        }
      }
    }

    if (Verbose)
      errs() << "Done analyzing function\n";

    return true;
  }

  bool VisitRecordDecl(RecordDecl *Declaration) {
    ISD.processRecordDecl(Declaration, Info, Context, CB);
    return true;
  }

private:
  ASTContext *Context;
  ProgramInfo &Info;
  ConstraintResolver CB;
  TypeVarInfo &TVInfo;
  InlineStructDetector ISD;
};

// Some information about variables in the program is required by the type
// variable analysis and constraint building passes. This is gathered by this
// visitor which is executed before both of the other visitors.
class VariableAdderVisitor : public RecursiveASTVisitor<VariableAdderVisitor> {
public:
  explicit VariableAdderVisitor(ASTContext *Context, ProgramVariableAdder &VA)
    : Context(Context), VarAdder(VA) {}


  bool VisitTypedefDecl(TypedefDecl* TD) {
    CVarSet empty;
    auto PSL = PersistentSourceLoc::mkPSL(TD, *Context);
    // If we haven't seen this typedef before, initialize it's entry in the
    // typedef map. If we have seen it before, and we need to preserve the
    // constraints contained within it
    if (!VarAdder.seenTypedef(PSL))
      // Add this typedef to the program info, if it contains a ptr to
      // an anonymous struct we mark as not being rewritable
      VarAdder.addTypedef(PSL, !PtrToStructDef::containsPtrToStructDef(TD),
                          TD, *Context);
    return true;
  }

  bool VisitVarDecl(VarDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());
    if (FL.isValid() && !isa<ParmVarDecl>(D))
      addVariable(D);
    return true;
  }

  bool VisitFunctionDecl(FunctionDecl *D) {
    FullSourceLoc FL = Context->getFullLoc(D->getBeginLoc());
    if (FL.isValid())
      VarAdder.addVariable(D, Context);
    return true;
  }

  bool VisitRecordDecl(RecordDecl *Declaration) {
    if (Declaration->isThisDeclarationADefinition()) {
      RecordDecl *Definition = Declaration->getDefinition();
      assert("Declaration is a definition, but getDefinition() is null?" &&
             Definition);
      FullSourceLoc FL = Context->getFullLoc(Definition->getBeginLoc());
      if (FL.isValid())
        for (auto *const D : Definition->fields())
          addVariable(D);
    }
    return true;
  }

private:
  ASTContext *Context;
  ProgramVariableAdder &VarAdder;

  void addVariable(DeclaratorDecl *D) {
    VarAdder.addABoundsVariable(D);
    if (isPtrOrArrayType(D->getType()))
      VarAdder.addVariable(D, Context);
  }
};

void VariableAdderConsumer::HandleTranslationUnit(ASTContext &C) {
  Info.enterCompilationUnit(C);
  if (Verbose) {
    SourceManager &SM = C.getSourceManager();
    FileID MainFileId = SM.getMainFileID();
    const FileEntry *FE = SM.getFileEntryForID(MainFileId);
    if (FE != nullptr)
      errs() << "Analyzing file " << FE->getName() << "\n";
    else
      errs() << "Analyzing\n";
  }

  VariableAdderVisitor VAV = VariableAdderVisitor(&C, Info);
  TranslationUnitDecl *TUD = C.getTranslationUnitDecl();
  // Collect Variables.
  for (const auto &D : TUD->decls()) {
    VAV.TraverseDecl(D);
  }

  if (Verbose)
    errs() << "Done analyzing\n";

  Info.exitCompilationUnit();
  return;
}

void ConstraintBuilderConsumer::HandleTranslationUnit(ASTContext &C) {
  Info.enterCompilationUnit(C);
  if (Verbose) {
    SourceManager &SM = C.getSourceManager();
    FileID MainFileId = SM.getMainFileID();
    const FileEntry *FE = SM.getFileEntryForID(MainFileId);
    if (FE != nullptr)
      errs() << "Analyzing file " << FE->getName() << "\n";
    else
      errs() << "Analyzing\n";
  }

  auto &PStats = Info.getPerfStats();

  PStats.startConstraintBuilderTime();

  TypeVarVisitor TV = TypeVarVisitor(&C, Info);
  ConstraintResolver CSResolver(Info, &C);
  ContextSensitiveBoundsKeyVisitor CSBV =
      ContextSensitiveBoundsKeyVisitor(&C, Info, &CSResolver);
  ConstraintGenVisitor GV = ConstraintGenVisitor(&C, Info, TV);
  TranslationUnitDecl *TUD = C.getTranslationUnitDecl();

  // Generate constraints.
  for (const auto &D : TUD->decls()) {
    // The order of these traversals CANNOT be changed because the constraint
    // gen visitor requires the type variable information gathered in the type
    // variable traversal.

    CSBV.TraverseDecl(D);
    TV.TraverseDecl(D);
    GV.TraverseDecl(D);
  }

  // Store type variable information for use in rewriting
  TV.setProgramInfoTypeVars();

  if (Verbose)
    errs() << "Done analyzing\n";

  PStats.endConstraintBuilderTime();

  PStats.endConstraintBuilderTime();

  Info.exitCompilationUnit();
  return;
}

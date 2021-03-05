//=--ProgramInfo.h------------------------------------------------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This class represents all the information about a source file
// collected by the converter.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_3C_PROGRAMINFO_H
#define LLVM_CLANG_3C_PROGRAMINFO_H

#include "clang/3C/3CInteractiveData.h"
#include "clang/3C/AVarBoundsInfo.h"
#include "clang/3C/ConstraintVariables.h"
#include "clang/3C/PersistentSourceLoc.h"
#include "clang/3C/Utils.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

class ProgramVariableAdder {
public:
  virtual void addVariable(clang::DeclaratorDecl *D,
                           clang::ASTContext *AstContext) = 0;
  void addABoundsVariable(clang::Decl *D) {
    getABoundsInfo().insertVariable(D);
  }

protected:
  virtual AVarBoundsInfo &getABoundsInfo() = 0;
};

class ProgramInfo : public ProgramVariableAdder {
public:
  // This map holds similar information as the type variable map in
  // ConstraintBuilder.cpp, but it is stored in a form that is usable during
  // rewriting.
  typedef std::map<unsigned int, ConstraintVariable *> CallTypeParamBindingsT;
  typedef std::map<PersistentSourceLoc, CallTypeParamBindingsT>
      TypeParamBindingsT;

  typedef std::map<std::string, FVConstraint *> ExternalFunctionMapType;
  typedef std::map<std::string, ExternalFunctionMapType> StaticFunctionMapType;

  ProgramInfo();
  void print(llvm::raw_ostream &O) const;
  void dump() const { print(llvm::errs()); }
  void dumpJson(llvm::raw_ostream &O) const;
  void dumpStats(const std::set<std::string> &F) {
    printStats(F, llvm::errs());
  }
  void printStats(const std::set<std::string> &F, llvm::raw_ostream &O,
                  bool OnlySummary = false, bool JsonFormat = false);

  // Populate Variables, VarDeclToStatement, RVariables, and DepthMap with
  // AST data structures that correspond do the data stored in PDMap and
  // ReversePDMap.
  void enterCompilationUnit(clang::ASTContext &Context);

  // Remove any references we maintain to AST data structure pointers.
  // After this, the Variables, VarDeclToStatement, RVariables, and DepthMap
  // should all be empty.
  void exitCompilationUnit();

  bool hasPersistentConstraints(clang::Expr *E, ASTContext *C) const;
  const CVarSet &getPersistentConstraints(clang::Expr *E, ASTContext *C) const;
  void storePersistentConstraints(clang::Expr *E, const CVarSet &Vars,
                                  ASTContext *C);

  // Get constraint variable for the provided Decl
  CVarOption getVariable(clang::Decl *D, clang::ASTContext *C);

  // Retrieve a function's constraints by decl, or by name; nullptr if not found
  FVConstraint *getFuncConstraint(FunctionDecl *D, ASTContext *C) const;
  FVConstraint *getExtFuncDefnConstraint(std::string FuncName) const;
  FVConstraint *getStaticFuncConstraint(std::string FuncName,
                                        std::string FileName) const;

  // Called when we are done adding constraints and visiting ASTs.
  // Links information about global symbols together and adds
  // constraints where appropriate.
  bool link();

  const VariableMap &getVarMap() const { return Variables; }
  Constraints &getConstraints() { return CS; }
  AVarBoundsInfo &getABoundsInfo() { return ArrBInfo; }

  ConstraintsInfo &getInterimConstraintState() { return CState; }
  bool computeInterimConstraintState(const std::set<std::string> &FilePaths);

  const ExternalFunctionMapType &getExternFuncDefFVMap() const {
    return ExternalFunctionFVCons;
  }

  const StaticFunctionMapType &getStaticFuncDefFVMap() const {
    return StaticFunctionFVCons;
  }

  void setTypeParamBinding(CallExpr *CE, unsigned int TypeVarIdx,
                           ConstraintVariable *CV, ASTContext *C);
  bool hasTypeParamBindings(CallExpr *CE, ASTContext *C) const;
  const CallTypeParamBindingsT &getTypeParamBindings(CallExpr *CE,
                                                     ASTContext *C) const;

  void constrainWildIfMacro(ConstraintVariable *CV, SourceLocation Location,
                            PersistentSourceLoc *PSL = nullptr);

  void unifyIfTypedef(const clang::Type *, clang::ASTContext &,
                      clang::DeclaratorDecl *, PVConstraint *);

  CVarOption lookupTypedef(PersistentSourceLoc PSL);

  bool seenTypedef(PersistentSourceLoc PSL);

  void addTypedef(PersistentSourceLoc PSL, bool CanRewriteDef, TypedefDecl *TD,
                  ASTContext& C);

private:
  // List of constraint variables for declarations, indexed by their location in
  // the source. This information persists across invocations of the constraint
  // analysis from compilation unit to compilation unit.
  VariableMap Variables;

  // Map storing constraint information for typedefed types
  // The set contains all the constraint variables that also use this tyepdef
  // rewritten.
  std::map<PersistentSourceLoc, CVarOption> TypedefVars;

  // Map with the same purpose as the Variables map, this stores constraint
  // variables for non-declaration expressions.
  std::map<PersistentSourceLoc, CVarSet> ExprConstraintVars;

  // Implicit casts do not physically exist in the source code, so their source
  // location can collide with the source location of another expression. Since
  // we need to look up constraint variables for implicit casts for the cast
  // placement, the variables are stored in this separate map.
  std::map<PersistentSourceLoc, CVarSet> ImplicitCastConstraintVars;

  // Constraint system.
  Constraints CS;
  // Is the ProgramInfo persisted? Only tested in asserts. Starts at true.
  bool Persisted;

  // Map of global decls for which we don't have a definition, the keys are
  // names of external vars, the value is whether the def
  // has been seen before.
  std::map<std::string, bool> ExternGVars;

  // Maps for global/static functions, global variables.
  ExternalFunctionMapType ExternalFunctionFVCons;
  StaticFunctionMapType StaticFunctionFVCons;
  std::map<std::string, std::set<PVConstraint *>> GlobalVariableSymbols;

  // Object that contains all the bounds information of various array variables.
  AVarBoundsInfo ArrBInfo;
  // Constraints state.
  ConstraintsInfo CState;

  // For each call to a generic function, remember how the type parameters were
  // instantiated so they can be inserted during rewriting.
  TypeParamBindingsT TypeParamBindings;

  // Insert the given FVConstraint* set into the provided Map.
  void insertIntoExternalFunctionMap(ExternalFunctionMapType &Map,
                                     const std::string &FuncName,
                                     FVConstraint *NewC, FunctionDecl *FD,
                                     ASTContext *C);

  // Inserts the given FVConstraint* set into the provided static map.
  void insertIntoStaticFunctionMap(StaticFunctionMapType &Map,
                                   const std::string &FuncName,
                                   const std::string &FileName,
                                   FVConstraint *ToIns, FunctionDecl *FD,
                                   ASTContext *C);

  // Special-case handling for decl introductions. For the moment this covers:
  //  * void-typed variables
  //  * va_list-typed variables
  void specialCaseVarIntros(ValueDecl *D, ASTContext *Context);

  // Inserts the given FVConstraint set into the extern or static function map.
  // Note: This can trigger a brainTransplant from an existing FVConstraint into
  // the argument FVConstraint. The brainTransplant copies the atoms of the
  // existing FVConstraint into the argument. This effectively throws out any
  // constraints that may been applied to the argument FVConstraint, so do not
  // call this function any time other than immediately after constructing an
  // FVConstraint.
  void insertNewFVConstraint(FunctionDecl *FD, FVConstraint *FVCon,
                             ASTContext *C);

  // Retrieves a FVConstraint* from a Decl (which could be static, or global)
  FVConstraint *getFuncFVConstraint(FunctionDecl *FD, ASTContext *C);

  // For each pointer type in the declaration of D, add a variable to the
  // constraint system for that pointer type.
  void addVariable(clang::DeclaratorDecl *D, clang::ASTContext *AstContext);

  void insertIntoPtrSourceMap(const PersistentSourceLoc *PSL,
                              ConstraintVariable *CV);

  void computePtrLevelStats();

  void insertCVAtoms(ConstraintVariable *CV,
                     std::map<ConstraintKey, ConstraintVariable *> &AtomMap);
};

#endif

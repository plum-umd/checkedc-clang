//===------ PreorderAST.cpp: An n-ary preorder abstract syntax tree -------===//
//
//                     The LLVM Compiler Infrastructure
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//  This file implements methods to create and manipulate an n-ary preorder
//  abstract syntax tree which is used to semantically compare two expressions.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/PreorderAST.h"

using namespace clang;

void PreorderAST::AddNode(Node *N, OperatorNode *Parent) {
  // If the root is null, make the current node the root.
  if (!Root)
    Root = N;

  // Add the current node to the list of children of its parent.
  if (Parent)
    Parent->Children.push_back(N);
}

bool PreorderAST::CanCoalesceNode(OperatorNode *O) {
  if (!O || !isa<OperatorNode>(O) || !O->Parent)
    return false;

  // We can only coalesce if the operator of the current and parent node is
  // commutative and associative. This is because after coalescing we later
  // need to sort the nodes and if the operator is not commutative and
  // associative then sorting would be incorrect.
  if (!O->IsOpCommutativeAndAssociative() ||
      !O->Parent->IsOpCommutativeAndAssociative())
    return false;

  // We can coalesce in the following scenarios:
  // 1. The current and parent nodes have the same operator OR
  // 2. The current node is the only child of its operator node (maybe as a
  // result of constant folding).
  return O->Opc == O->Parent->Opc || O->Children.size() == 1;
}

void PreorderAST::CoalesceNode(OperatorNode *O) {
  if (!CanCoalesceNode(O)) {
    assert(0 && "Attempting to coalesce invalid node");
    SetError();
    return;
  }

  // Remove the current node from the list of children of its parent.
  for (auto I = O->Parent->Children.begin(),
            E = O->Parent->Children.end(); I != E; ++I) {
    if (*I == O) {
      O->Parent->Children.erase(I);
      break;
    }
  }

  // Move all children of the current node to its parent.
  for (auto *Child : O->Children) {
    Child->Parent = O->Parent;
    O->Parent->Children.push_back(Child);
  }

  // Delete the current node.
  delete O;
}

void PreorderAST::Create(Expr *E, OperatorNode *Parent) {
  if (!E)
    return;

  E = Lex.IgnoreValuePreservingOperations(Ctx, E->IgnoreParens());

  if (!Parent) {
    // The invariant is that the root node must be a OperatorNode with an
    // addition operator. So for expressions like "if (*p)", we don't have a
    // BinaryOperator. So when we enter this function there is no root and the
    // parent is null. So we create a new OperatorNode with + as the operator
    // and add 0 as a LeafExprNode child of this OperatorNode. This helps us
    // compare expressions like "p" and "p + 1" by normalizing "p" to "p + 0".

    auto *N = new OperatorNode(BO_Add, Parent);
    AddNode(N, Parent);

    llvm::APInt Zero(Ctx.getTargetInfo().getIntWidth(), 0);
    auto *ZeroLiteral = new (Ctx) IntegerLiteral(Ctx, Zero, Ctx.IntTy,
                                                 SourceLocation());
    auto *L = new LeafExprNode(ZeroLiteral, N);
    AddNode(L, /*Parent*/ N);
    Create(E, /*Parent*/ N);

  } else if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    BinaryOperator::Opcode BinOp = BO->getOpcode();
    Expr *LHS = BO->getLHS();
    Expr *RHS = BO->getRHS();

    // We can convert (e1 - e2) to (e1 + -e2) if -e2 does not overflow.  One
    // instance where -e2 can overflow is if e2 is INT_MIN. Here, instead of
    // specifically checking whether e2 is INT_MIN, we add a unary minus to e2
    // and then check if the resultant expression -e2 overflows. If it
    // overflows, we undo the unary minus operator.

    // TODO: Currently, we can only prove that integer constant expressions do
    // not overflow. We still need to handle proving that non-constant
    // expressions do not overflow.
    if (BO->getOpcode() == BO_Sub &&
        RHS->isIntegerConstantExpr(Ctx)) {
      Expr *UOMinusRHS =
        UnaryOperator::Create(Ctx, RHS, UO_Minus, RHS->getType(),
                              RHS->getValueKind(), RHS->getObjectKind(),
                              SourceLocation(), /*CanOverflow*/ true,
                              FPOptionsOverride());

      SmallVector<PartialDiagnosticAt, 8> Diag;
      UOMinusRHS->EvaluateKnownConstIntCheckOverflow(Ctx, &Diag);

      bool Overflow = false;
      for (auto &PD : Diag) {
        if (PD.second.getDiagID() == diag::note_constexpr_overflow) {
          Overflow = true;
          break;
        }
      }

      if (!Overflow) {
        BinOp = BO_Add;
        RHS = UOMinusRHS;
      }

      // TODO: In case of overflow we leak the memory allocated to UOMinusRHS.
      // Whereas if there is no overflow we leak the memory initially allocated
      // to RHS.
    }

    auto *N = new OperatorNode(BinOp, Parent);
    AddNode(N, Parent);

    Create(LHS, /*Parent*/ N);
    Create(RHS, /*Parent*/ N);

  } else {
    auto *N = new LeafExprNode(E, Parent);
    AddNode(N, Parent);
  }
}

void PreorderAST::Coalesce(Node *N, bool &Changed) {
  if (Error)
    return;

  auto *O = dyn_cast_or_null<OperatorNode>(N);
  if (!O)
    return;

  // Coalesce the children first.
  for (auto *Child : O->Children)
    if (isa<OperatorNode>(Child))
      Coalesce(Child, Changed);

  if (CanCoalesceNode(O)) {
    CoalesceNode(O);
    Changed = true;
  }
}

bool PreorderAST::CompareNodes(const Node *N1, const Node *N2) {
  if (const auto *L1 = dyn_cast<LeafExprNode>(N1)) {
    if (const auto *L2 = dyn_cast<LeafExprNode>(N2)) {
      // If L1 is a UnaryOperatorExpr and L2 is not, then
      // 1. If L1 contains an integer constant then sorted order is (L2, L1)
      // 2. Else sorted order is (L1, L2).
      if (isa<UnaryOperator>(L1->E) && !isa<UnaryOperator>(L2->E))
        return !L1->E->isIntegerConstantExpr(Ctx);

      // If L2 is a UnaryOperatorExpr and L1 is not, then
      // 1. If L2 contains an integer constant then sorted order is (L1, L2)
      // 2. Else sorted order is (L2, L1).
      if (!isa<UnaryOperator>(L1->E) && isa<UnaryOperator>(L2->E))
        return L2->E->isIntegerConstantExpr(Ctx);

      // If both nodes are LeafExprNodes compare the exprs.
      return Lex.CompareExpr(L1->E, L2->E) == Result::LessThan;
    }

    // N2:OperatorNodeExpr < N1:LeafExprNode.
    return false;
  }

  // N1:OperatorNodeExpr < N2:LeafExprNode.
  if (isa<LeafExprNode>(N2))
    return true;

  // Compare N1:OperatorNode and N2:OperatorNode.
  const auto *O1 = dyn_cast<OperatorNode>(N1);
  const auto *O2 = dyn_cast<OperatorNode>(N2);

  if (O1->Opc != O2->Opc)
    return O1->Opc < O2->Opc;
  return O1->Children.size() < O2->Children.size();
}

void PreorderAST::Sort(Node *N) {
  auto *O = dyn_cast_or_null<OperatorNode>(N);
  if (!O)
    return;

  // Sort the children first.
  for (auto *Child : O->Children)
    if (isa<OperatorNode>(Child))
      Sort(Child);

  // We can only sort if the operator is commutative and associative.
  if (!O->IsOpCommutativeAndAssociative())
    return;

  // Sort the children.
  llvm::sort(O->Children.begin(), O->Children.end(),
             [&](const Node *N1, const Node *N2) {
               return CompareNodes(N1, N2);
            });
}

void PreorderAST::ConstantFold(Node *N, bool &Changed) {
  // Note: This function assumes that the children of each OperatorNode of the
  // preorder AST have already been sorted.

  if (Error)
    return;

  auto *O = dyn_cast_or_null<OperatorNode>(N);
  if (!O)
    return;

  size_t ConstStartIdx = 0;
  unsigned NumConsts = 0;
  llvm::APSInt ConstFoldedVal;

  for (size_t I = 0; I != O->Children.size(); ++I) {
    auto *Child = O->Children[I];

    // Recursively constant fold the children of a OperatorNode.
    if (isa<OperatorNode>(Child)) {
      ConstantFold(Child, Changed);
      continue;
    }

    // We can only constant fold if the operator is commutative and
    // associative.
    if (!O->IsOpCommutativeAndAssociative())
      continue;

    auto *ChildLeafNode = dyn_cast_or_null<LeafExprNode>(Child);
    if (!ChildLeafNode)
      continue;

    // Check if the child node is an integer constant.
    llvm::APSInt CurrConstVal;
    if (!ChildLeafNode->E->isIntegerConstantExpr(CurrConstVal, Ctx))
      continue;

    ++NumConsts;

    if (NumConsts == 1) {
      // We will use ConstStartIdx later in this function to delete the
      // constant folded nodes.
      ConstStartIdx = I;
      ConstFoldedVal = CurrConstVal;

    } else {
      // Constant fold based on the operator.
      bool Overflow;
      switch(O->Opc) {
        default: continue;
        case BO_Add:
          ConstFoldedVal = ConstFoldedVal.sadd_ov(CurrConstVal, Overflow);
          break;
        case BO_Mul:
          ConstFoldedVal = ConstFoldedVal.smul_ov(CurrConstVal, Overflow);
          break;
      }

      // If we encounter an overflow during constant folding we cannot proceed.
      if (Overflow) {
        SetError();
        return;
      }
    }
  }

  // To fold constants we need at least 2 constants.
  if (NumConsts <= 1)
    return;

  // Delete the folded constants and reclaim memory.
  // Note: We do not explicitly need to increment the iterator because after
  // erase the iterator automatically points to the new location of the element
  // following the one we just erased.
  llvm::SmallVector<Node *, 2>::iterator I =
    O->Children.begin() + ConstStartIdx;
  while (NumConsts--) {
    delete(*I);
    O->Children.erase(I);
  }

  llvm::APInt IntVal(Ctx.getTargetInfo().getIntWidth(),
                     ConstFoldedVal.getLimitedValue());

  Expr *ConstFoldedExpr = new (Ctx) IntegerLiteral(Ctx, IntVal, Ctx.IntTy,
                                                   SourceLocation());

  // Add the constant folded expression to list of children of the current
  // OperatorNode.
  O->Children.push_back(new LeafExprNode(ConstFoldedExpr, O));

  // If the constant folded expr is the only child of this OperatorNode we can
  // coalesce the node.
  if (O->Children.size() == 1 && CanCoalesceNode(O))
    CoalesceNode(O);

  Changed = true;
}

bool PreorderAST::GetDerefOffset(Node *UpperNode, Node *DerefNode,
				 llvm::APSInt &Offset) {
  // Extract the offset by which a pointer is dereferenced. For the pointer we
  // compare the dereference expr with the declared upper bound expr. If the
  // non-integer parts of the two exprs are not equal we say that a valid
  // offset does not exist and return false. If the non-integer parts of the
  // two exprs are equal the offset is calculated as:
  // (integer part of deref expr - integer part of upper bound expr).

  // Since we have already normalized exprs like "*p" to "*(p + 0)" we require
  // that the root of the preorder AST is a OperatorNode.
  auto *O1 = dyn_cast_or_null<OperatorNode>(UpperNode);
  auto *O2 = dyn_cast_or_null<OperatorNode>(DerefNode);

  if (!O1 || !O2)
    return false;

  // If the opcodes mismatch we cannot have a valid offset.
  if (O1->Opc != O2->Opc)
    return false;

  // We have already constant folded the constants. So return false if the
  // number of children mismatch.
  if (O1->Children.size() != O2->Children.size())
    return false;

  // Check if the children are equivalent.
  for (size_t I = 0; I != O1->Children.size(); ++I) {
    auto *Child1 = O1->Children[I];
    auto *Child2 = O2->Children[I];

    if (Compare(Child1, Child2) == Result::Equal)
      continue;

    // If the children are not equal we require that they be integer constant
    // leaf nodes. Otherwise we cannot have a valid offset.
    auto *L1 = dyn_cast_or_null<LeafExprNode>(Child1);
    auto *L2 = dyn_cast_or_null<LeafExprNode>(Child2);

    if (!L1 || !L2)
      return false;

    // Return false if either of the leaf nodes is not an integer constant.
    llvm::APSInt UpperOffset;
    if (!L1->E->isIntegerConstantExpr(UpperOffset, Ctx))
      return false;

    llvm::APSInt DerefOffset;
    if (!L2->E->isIntegerConstantExpr(DerefOffset, Ctx))
      return false;

    // Offset should always be of the form (ptr + offset). So we check for
    // addition.
    // Note: We have already converted (ptr - offset) to (ptr + -offset). So
    // its okay to only check for addition.
    if (O1->Opc != BO_Add)
      return false;

    // This guards us from a case where the constants were not folded for
    // some reason. In theory this should never happen. But we are adding this
    // check just in case.
    llvm::APSInt Zero(Ctx.getTargetInfo().getIntWidth(), 0);
    if (llvm::APSInt::compareValues(Offset, Zero) != 0)
      return false;

    // offset = deref offset - declared upper bound offset.
    // Return false if we encounter an overflow.
    bool Overflow;
    Offset = DerefOffset.ssub_ov(UpperOffset, Overflow);
    if (Overflow)
      return false;
  }

  return true;
}

Result PreorderAST::Compare(const Node *N1, const Node *N2) const {
  // If both the nodes are null.
  if (!N1 && !N2)
    return Result::Equal;

  // If only one of the nodes is null.
  if (!N1 && N2)
    return Result::LessThan;
  if (N1 && !N2)
    return Result::GreaterThan;

  if (const auto *O1 = dyn_cast<OperatorNode>(N1)) {
    // OperatorNode > LeafNode.
    if (!isa<OperatorNode>(N2))
      return Result::GreaterThan;

    const auto *O2 = dyn_cast<OperatorNode>(N2);

    // If the Opcodes mismatch.
    if (O1->Opc < O2->Opc)
      return Result::LessThan;
    if (O1->Opc > O2->Opc)
      return Result::GreaterThan;

    size_t ChildCount1 = O1->Children.size(),
           ChildCount2 = O2->Children.size();

    // If the number of children of the two nodes mismatch.
    if (ChildCount1 < ChildCount2)
      return Result::LessThan;
    if (ChildCount1 > ChildCount2)
      return Result::GreaterThan;

    // Match each child of the two nodes.
    for (size_t I = 0; I != ChildCount1; ++I) {
      auto *Child1 = O1->Children[I];
      auto *Child2 = O2->Children[I];

      Result ChildComparison = Compare(Child1, Child2);

      // If any child differs between the two nodes.
      if (ChildComparison != Result::Equal)
        return ChildComparison;
    }
  }

  if (const auto *L1 = dyn_cast<LeafExprNode>(N1)) {
    // Compare the exprs for two leaf nodes.
    if (const auto *L2 = dyn_cast<LeafExprNode>(N2))
      return Lex.CompareExpr(L1->E, L2->E);

    // LeafNode < OperatorNode.
    return Result::LessThan;
  }

  return Result::Equal;
}

void PreorderAST::Normalize() {
  // TODO: Perform simple arithmetic optimizations/transformations on the
  // constants in the nodes.

  bool Changed = true;
  while (Changed) {
    Changed = false;
    Coalesce(Root, Changed);
    if (Error)
      break;
    Sort(Root);
    ConstantFold(Root, Changed);
    if (Error)
      break;
  }

  if (Ctx.getLangOpts().DumpPreorderAST) {
    PrettyPrint(Root);
    OS << "--------------------------------------\n";
  }
}

void PreorderAST::PrettyPrint(Node *N) {
  if (const auto *O = dyn_cast_or_null<OperatorNode>(N)) {
    OS << BinaryOperator::getOpcodeStr(O->Opc) << "\n";

    for (auto *Child : O->Children)
      PrettyPrint(Child);
  }
  else if (const auto *L = dyn_cast_or_null<LeafExprNode>(N))
    L->E->dump(OS, Ctx);
}

void PreorderAST::Cleanup(Node *N) {
  if (auto *O = dyn_cast_or_null<OperatorNode>(N))
    for (auto *Child : O->Children)
      Cleanup(Child);

  if (N)
    delete N;
}

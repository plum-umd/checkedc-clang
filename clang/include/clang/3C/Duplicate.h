#ifndef LLVM_DUPLICATE_H
#define LLVM_DUPLICATE_H

#include "ProgramInfo.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <getopt.h>

typedef std::function<bool (clang::VarDecl*)> selector;

// Create a duplicate of a variable.
// ToScan is the context to search for the variable
// P is a predicate that decides which variable to make a duplicate of
// Returns the name of the newly created variable
// or None if no variable matching P is found
Option<std::string> createDuplicate(clang::ASTContext &Context,
                                    clang::Rewriter &R, ProgramInfo &Info,
                                    clang::Decl *ToScan, selector P);



#endif //LLVM_DUPLICATE_H

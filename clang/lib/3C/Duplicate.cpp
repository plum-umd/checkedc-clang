#include "clang/3C/Duplicate.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/3C/ProgramInfo.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;
using namespace llvm;
using namespace std;


// Recursive Visitor
class FindVarDecl : public RecursiveASTVisitor<FindVarDecl> {
public:
  explicit FindVarDecl(ASTContext &Context, Rewriter &R,
                       ProgramInfo &I, selector P)
      : Context(Context),  R(R), Info(I), P(P) {}


  bool VisitVarDecl(VarDecl *VD) {
    // If the selector matches execute the duplicate function
    // and store the new name in the optional
    if(P(VD)){
      auto NewName = addDuplicate(VD);
      Replacement = { NewName };
      return false;
    } else {
      return true;
    }
  }

  Option<string> didReplace(void) {
    return Replacement;
  }

private:
  // Insert the duplicate into the rewriter
  // Returns the new var's name
  string addDuplicate(VarDecl *VD) {
    SourceLocation End = VD->getEndLoc();
    auto Dup = createDuplicateString(VD);
    R.InsertTextAfterToken(End, Dup.second);
    return Dup.first;
  }

  // Create the text for the new variable
  // Returns a pair containing the new name
  // and the new line containing declaration and assignment
  pair<string,string> createDuplicateString(VarDecl *VD) {
    auto CV = Info.getVariable(VD, &Context);
    auto Type =
        CV.hasValue() ?
                      CV.getValue()
                          .mkString(Info.getConstraints().getVariables(),
                                    false)
                      : VD->getType().getAsString();
    auto TargetName = VD->getNameAsString();
    auto NewName = "__" + TargetName + "_copy";
    return make_pair(NewName, ";\n" + Type + " " + NewName + "= " + TargetName + "");
  }



  ASTContext &Context;
  Rewriter &R;
  ProgramInfo &Info;
  // Predicate for finding the variable to duplicate
  selector P;
  // Duplicate's name or none if var not found
  Option<string> Replacement = {};
};

// Main entrypoint
Option<string> createDuplicate(ASTContext &Context, Rewriter &R,
                      ProgramInfo &Info,
                      Decl *FD, selector P) {
  auto V = FindVarDecl(Context, R, Info, P);
  V.TraverseDecl(FD);
  return V.didReplace();
}

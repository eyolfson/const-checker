#include "Methods.h"

#include "Exprs.h"
#include "Types.h"
#include "VariableKinds.h"

#include <clang/AST/RecursiveASTVisitor.h>

using namespace clang;
using namespace clang::immutability;

namespace {

enum class FirstReturnResult {
  NOOP,
  FIELD,
  OTHER,
};

class ResultVisitor : public RecursiveASTVisitor<ResultVisitor> {
  void handleAssignmentOp(const Expr *E, const Expr *lhs) {
    LHSTuple T = getLHSTuple(lhs);
    for (const Expr *root : T.roots) {
      if (isa<DeclRefExpr>(root)) {
        if (T.isDereference) {
          if (variableKinds.maybeField(E, lhs)) {
            mutateResult = MutateResult::MAYBE_MUTATION;
          }
        }
      }
      else {
        if (variableKinds.maybeField(E, lhs)) {
          mutateResult = MutateResult::MAYBE_MUTATION;
        }
      }
    }
  }

  bool firstReturn;

public:
  const CXXMethodDecl *Method;
  ClangDatabase &ClangDB;
  VariableKinds &variableKinds;
  MutateResult mutateResult;
  FirstReturnResult firstReturnResult;

  ResultVisitor(const CXXMethodDecl *M, ClangDatabase &ClangDB, VariableKinds &vk)
  : Method(M), ClangDB(ClangDB), variableKinds(vk),
    mutateResult(MutateResult::NO_MUTATION),
    firstReturn(true), firstReturnResult(FirstReturnResult::NOOP) {}

  bool VisitReturnStmt(const ReturnStmt *RS) {
    const Expr *E = RS->getRetValue();
    if (!E) {
      return true;
    }
    QualType T = E->getType();
    if (E->getType().getTypePtr()->isVoidType()) {
      firstReturnResult = FirstReturnResult::NOOP;
      return true;
    }

    const Expr *Val = RS->getRetValue();
    if (firstReturn) {
      if (variableKinds.mustLiteral(RS, Val)) {
        firstReturnResult = FirstReturnResult::NOOP;
      }
      else if (variableKinds.maybeField(RS, Val)) {
        firstReturnResult = FirstReturnResult::FIELD;
      }
      else {
        firstReturnResult = FirstReturnResult::OTHER;
      }
      firstReturn = false;
    }
    else {
      if (variableKinds.mustLiteral(RS, Val)) {
        // No change
      }
      else if (variableKinds.maybeField(RS, Val)) {
        firstReturnResult = FirstReturnResult::FIELD;
      }
      else {
        if (firstReturnResult == FirstReturnResult::NOOP) {
          firstReturnResult = FirstReturnResult::OTHER;
        }
      }
    }

    return true;
  }

  bool VisitCallExpr(const CallExpr *CE) {
    // Prune any comparsion operators
    if (const auto *OCE = dyn_cast<CXXOperatorCallExpr>(CE)) {
      auto Op = OCE->getOperator();
      switch (Op) {
      case OO_EqualEqual:
      case OO_Less:
      case OO_Greater:
      case OO_ExclaimEqual:
      case OO_LessEqual:
      case OO_GreaterEqual:
        return true;
      }

      if (OCE->isAssignmentOp()) {
        handleAssignmentOp(CE, OCE->getArg(0));
        return true;
      }
    }

    // Skip assertions
    const FunctionDecl *FD = CE->getDirectCallee();
    if (FD && (FD->getQualifiedNameAsString() == "__assert_fail")) {
      return true;
    }

    // Sometimes member calls aren't CXXMemberCallExprs, it could be a CallExpr
    // with CXXDependentScopeMemberExpr as a callee
    const Expr *Callee = CE->getCallee();
    if (variableKinds.maybeField(CE, Callee)) {
      if (const MemberExpr *memberExpr = dyn_cast<MemberExpr>(Callee)) {
        if (const CXXMethodDecl *MethodCallee = dyn_cast<CXXMethodDecl>(memberExpr->getMemberDecl())) {
	  ClangDB.insertMethodDependence(Method, MethodCallee);
	  return true;
        }
      }
      mutateResult = MutateResult::MAYBE_MUTATION;
      return true;
    }

    for (const Expr *Arg : CE->arguments()) {
      const Type *T = Arg->getType().getTypePtr();

      if (T->isIntegerType()
          || T->isBooleanType()
          || T->isAnyCharacterType()
          || T->isFloatingType()) {
        // Skip value types
        continue;
      }

      if (variableKinds.maybeField(CE, Arg)) {
        mutateResult = MutateResult::MAYBE_MUTATION;
        break;
      }
    }

    return true;
  }

  bool VisitCXXMemberCallExpr(const CXXMemberCallExpr *E) {
    const CXXMethodDecl *MD = E->getMethodDecl();

    if (variableKinds.maybeField(E, E->getImplicitObjectArgument())) {
      if (MD) {
	ClangDB.insertMethodDependence(Method, MD);
	return true;
      }
      else {
	mutateResult = MutateResult::MAYBE_MUTATION;
	return true;
      }
    }
    return VisitCallExpr(E);
  }
  bool VisitUnaryOperator(const UnaryOperator *U) {
    if (U->isIncrementDecrementOp()) {
      if (variableKinds.maybeField(U, U->getSubExpr())) {
        mutateResult = MutateResult::MAYBE_MUTATION;
      }
    }
    return true;
  }
  bool VisitGCCAsmStmt(const GCCAsmStmt *G) {
    for (const Expr *Input : G->inputs()) {
      if (variableKinds.maybeField(G, Input)) {
        mutateResult = MutateResult::MAYBE_MUTATION;
      }
    }
    return true;
  }
  bool VisitBinaryOperator(const BinaryOperator *O) {
    if (O->isAssignmentOp()) {
      handleAssignmentOp(O, O->getLHS());
      // The LHS is a local variable, it can't mutate a field
    }
    return true;
  }
};

} // namespace

namespace clang {
namespace immutability {

MethodResultTuple MethodResult::getMethodResult(const CXXMethodDecl *D) {
  Method = D;

  VariableKinds VK(D);
  if (!VK.isValid()) {
    if (D->getReturnType().getTypePtr()->isVoidType()) {
      return MethodResultTuple(MutateResult::MAYBE_MUTATION,
			       ReturnResult::NOOP);
    }

    bool IsTransitive = isTransitivelyConst(D->getReturnType());
    if (IsTransitive) {
      return MethodResultTuple(MutateResult::MAYBE_MUTATION,
			       ReturnResult::FIELD_TRANSITIVE);
    }
    else {
      return MethodResultTuple(MutateResult::MAYBE_MUTATION,
			       ReturnResult::FIELD_NON_TRANSITIVE);
    }
  }

  ResultVisitor RV(Method, ClangDB, VK);
  RV.TraverseStmt(const_cast<Stmt *>(D->getBody()));
  ReturnResult returnResult;
  switch (RV.firstReturnResult) {
  case FirstReturnResult::NOOP:
    returnResult = ReturnResult::NOOP;
    break;
  case FirstReturnResult::FIELD: {
    bool IsTransitive = isTransitivelyConst(D->getReturnType());
    if (IsTransitive) {
      returnResult = ReturnResult::FIELD_TRANSITIVE;
    }
    else {
      returnResult = ReturnResult::FIELD_NON_TRANSITIVE;
    }
    break;
  }
  case FirstReturnResult::OTHER:
    returnResult = ReturnResult::OTHER;
    break;
  }

  return MethodResultTuple(RV.mutateResult, returnResult);
}

}
}

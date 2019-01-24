#include "Exprs.h"

#include <clang/AST/StmtVisitor.h>

using namespace clang;

namespace {

class LHSVisitor : public ConstStmtVisitor<LHSVisitor> {
  void baseCase(const Expr *E) {
    roots.push_back(E);
  }
public:
  LHSVisitor() : isDereference(false) {}
  std::vector<const Expr *> roots;
  bool isDereference;

  void VisitExpr(const Expr *E) {
    E->dump();
    llvm_unreachable("Fallback Expr");
  }
  void VisitBinaryOperator(const BinaryOperator *O) {
    O->dump();
    llvm_unreachable("Fallbback BinaryOperator");
  }
  void VisitCompoundAssignOperator(const CompoundAssignOperator *O) {
    O->dump();
    llvm_unreachable("Fallback CompoundAssignOperator");
  }
  void VisitUnaryOperator(const UnaryOperator *O) {
    O->dump();
    llvm_unreachable("Fallback UnaryOperator");
  }

  void VisitUnaryAddrOf(const UnaryOperator *O) {
    Visit(O->getSubExpr());
  }
  void VisitUnaryPostInc(const UnaryOperator *O) {
    Visit(O->getSubExpr());
  }
  void VisitUnaryPostDec(const UnaryOperator *O) {
    Visit(O->getSubExpr());
  }
  void VisitUnaryPreInc(const UnaryOperator *O) {
    Visit(O->getSubExpr());
  }
  void VisitUnaryPreDec(const UnaryOperator *O) {
    Visit(O->getSubExpr());
  }

  // This should be a pointer offset, so we only care about the LHS
  void VisitBinPtrMemD(const BinaryOperator *O) { // .*
    Visit(O->getLHS());
  }
  void VisitBinPtrMemI(const BinaryOperator *O) { // ->*
    Visit(O->getLHS());
  }
  void VisitBinAdd(const BinaryOperator *O) {
    Visit(O->getLHS());
  }
  void VisitBinSub(const BinaryOperator *O) {
    Visit(O->getLHS());
  }
  void VisitBinXor(const BinaryOperator *O) {
    Visit(O->getLHS());
  }

  void VisitParenExpr(const ParenExpr *E) {
    Visit(E->getSubExpr());
  }
  void VisitMemberExpr(const MemberExpr *E) {
    baseCase(E);
  }
  void VisitCXXDependentScopeMemberExpr(const CXXDependentScopeMemberExpr *E) {
    if (E->isArrow()) {
      isDereference = true;
    }
    if (E->isImplicitAccess()) {
      baseCase(E);
    }
    else {
      Visit(E->getBase());
    }
  }
  void VisitArraySubscriptExpr(const ArraySubscriptExpr *E) {
    // CLANG-BUG: Expr.h
    // `getBase` just checks which part of the array access is not an integer
    // and returns the other. However, in some cases, the index can be a string
    // literal and not an integer. For example: cvflann::KDTreeIndex::loadIndex
    // in the OpenCV repository. We'll add an explicit check here instead.
    // Note that `isa<StringLiteral>(E->getRHS())` is actually wrong, since more
    // complex things could be the base. The check done in Expr.h seems like it
    // works better for more modern C++ projects if we just assume LHS.
    isDereference = true;
    Visit(E->getLHS());
  }
  void VisitCXXMemberCallExpr(const CXXMemberCallExpr *E) {
    Visit(E->getImplicitObjectArgument());
  }
  void VisitCXXOperatorCallExpr(const CXXOperatorCallExpr *E) {
    switch (E->getOperator()) {
    case OO_Star:
    case OO_Subscript:
    case OO_Arrow:
      isDereference = true;
      Visit(E->getArg(0));
      break;
    case OO_PlusPlus:
    case OO_Plus:
    case OO_Minus:
    case OO_Equal:
    case OO_ExclaimEqual:
    case OO_PipeEqual:
    case OO_AmpEqual:
    case OO_CaretEqual:
      Visit(E->getArg(0));
      break; 
    case OO_Call: // for example: `weights(i, j) = ...;`
      baseCase(E);
      break;
    default:
      E->dump();
      llvm::errs() << "Unhandled " << getOperatorSpelling(E->getOperator()) << '\n';
      llvm_unreachable("CXXOperatorCallExpr");
    }
  }
  void VisitCastExpr(const CastExpr *E) {
    Visit(E->getSubExpr());
  }
  void VisitUnaryDeref(const UnaryOperator *O) {
    isDereference = true;
    Visit(O->getSubExpr());
  }
  void VisitCXXBindTemporaryExpr(const CXXBindTemporaryExpr *E) {
    Visit(E->getSubExpr());
  }
  void VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr *E) {
    Visit(E->GetTemporaryExpr());
  }
  void VisitConditionalOperator(const ConditionalOperator *E) {
    Visit(E->getTrueExpr());
    Visit(E->getFalseExpr());
  }
  void VisitCXXPseudoDestructorExpr(const CXXPseudoDestructorExpr *E) {
    Visit(E->getBase());
  }

  // Base cases
  void VisitCXXTemporaryObjectExpr(const CXXTemporaryObjectExpr *E) { baseCase(E); }
  void VisitCXXThisExpr(const CXXThisExpr *E) { baseCase(E); }
  void VisitDeclRefExpr(const DeclRefExpr *E) { baseCase(E); }
  //// Something like *(__errno_location()) = ERANGE
  void VisitCallExpr(const CallExpr *E) { baseCase(E); }
  void VisitUnresolvedMemberExpr(const UnresolvedMemberExpr *E) {
    if (E->isImplicitAccess()) {
      baseCase(E);
    }
    else {
      Visit(E->getBase());
    }
  }
  void VisitGNUNullExpr(const GNUNullExpr *E) {
    baseCase(E);
  }
  void VisitCXXUnresolvedConstructExpr(const CXXUnresolvedConstructExpr *E) {
    baseCase(E);
  }
  void VisitCXXConstructExpr(const CXXConstructExpr *E) {
    baseCase(E);
  }
};

}

namespace clang {
namespace immutability {
        
LHSTuple getLHSTuple(const Expr *E) {
  LHSVisitor V;
  V.Visit(E);
  LHSTuple T;
  T.roots = V.roots;
  T.isDereference = V.isDereference;
  return T;
}

}
}

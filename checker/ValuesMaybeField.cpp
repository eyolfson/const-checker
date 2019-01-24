#include "Values.h"

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/StmtVisitor.h>

#include "Methods.h" // DEBUG

using namespace clang;
using namespace immutability;

namespace {

class MaybeFieldsVisitor : public ConstStmtVisitor<MaybeFieldsVisitor, bool> {
  const Values::Set &maybeFields;
public:
  MaybeFieldsVisitor(const Values::Set &S) : maybeFields(S) {}
  bool VisitExpr(const Expr *E) {
    E->dump();
    llvm_unreachable("Fallback Expr");
  }
  bool VisitBinaryOperator(const BinaryOperator *O) {
    O->dump();
    llvm_unreachable("Fallback BinaryOperator");
  }
  bool VisitCompoundAssignOperator(const CompoundAssignOperator *O) {
    O->dump();
    llvm_unreachable("Fallback CompoundAssignOperator");
  }
  bool VisitUnaryOperator(const UnaryOperator *O) {
    O->dump();
    llvm_unreachable("Fallback UnaryOperator");
  }

  // Statements
  bool VisitAsmStmt(const AsmStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitAttributedStmt(const AttributedStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitBreakStmt(const BreakStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitCapturedStmt(const CapturedStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitCompoundStmt(const CompoundStmt *S) {
    if (S->body_empty()) {
      return false;
    }
    return Visit(S->body_back());
  }
  bool VisitContinueStmt(const ContinueStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitCoreturnStmt(const CoreturnStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitCoroutineBodyStmt(const CoroutineBodyStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitCXXCatchStmt(const CXXCatchStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitCXXForRangeStmt(const CXXForRangeStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitCXXTryStmt(const CXXTryStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitDeclStmt(const DeclStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitDoStmt(const DoStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitForStmt(const ForStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitGotoStmt(const GotoStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitIfStmt(const IfStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitIndirectGotoStmt(const IndirectGotoStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitLabelStmt(const LabelStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitMSDependentExistsStmt(const MSDependentExistsStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitNullStmt(const NullStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitObjCAtCatchStmt(const ObjCAtCatchStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitObjCAtFinallyStmt(const ObjCAtFinallyStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitObjCAtSynchronizedStmt(const ObjCAtSynchronizedStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitObjCAtThrowStmt(const ObjCAtThrowStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitObjCAtTryStmt(const ObjCAtTryStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitObjCAutoreleasePoolStmt(const ObjCAutoreleasePoolStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitObjCForCollectionStmt(const ObjCForCollectionStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitOMPExecutableDirective(const OMPExecutableDirective *S) { llvm_unreachable("Stmt"); }
  bool VisitReturnStmt(const ReturnStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitSEHExceptStmt(const SEHExceptStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitSEHFinallyStmt(const SEHFinallyStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitSEHLeaveStmt(const SEHLeaveStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitSEHTryStmt(const SEHTryStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitSwitchCase(const SwitchCase *S) { llvm_unreachable("Stmt"); }
  bool VisitSwitchStmt(const SwitchStmt *S) { llvm_unreachable("Stmt"); }
  bool VisitWhileStmt(const WhileStmt *S) { llvm_unreachable("Stmt"); }

  // Calls
  bool VisitCallExpr(const CallExpr *E) {
    for (const Expr *A : E->arguments()) {
      if (Visit(A)) {
        return true;
      }
    }
    return false;
  }
  bool VisitCXXMemberCallExpr(const CXXMemberCallExpr *E) {
    if (Visit(E->getImplicitObjectArgument())) {
      return true;
    }
    return VisitCallExpr(E);
  }

  bool VisitCXXConstructExpr(const CXXConstructExpr *E) {
    for (const Expr *A : E->arguments()) {
      if (Visit(A)) {
        return true;
      }
    }
    return false;
  }
  bool VisitCXXUnresolvedConstructExpr(const CXXUnresolvedConstructExpr *E) {
    for (auto I = E->arg_begin(), IE = E->arg_end(); I != IE; ++I) { 
      const Expr *A = *I;
      if (Visit(A)) {
        return true;
      }
    }
    return false;
  }
  bool VisitInitListExpr(const InitListExpr *E) {
    for (const Expr *A : E->inits()) {
      if (Visit(A)) {
        return true;
      }
    }
    if (E->hasArrayFiller()) {
      if (Visit(E->getArrayFiller())) {
        return true;
      }
    }
    return false;
  }
  bool VisitCXXStdInitializerListExpr(const CXXStdInitializerListExpr *E) {
    return Visit(E->getSubExpr());
  }
  bool VisitParenListExpr(const ParenListExpr *E) {
    for (const Expr *A : const_cast<ParenListExpr *>(E)->exprs()) { // const mistake
      if (Visit(A)) {
        return true;
      }
    }
    return false;
  }
  bool VisitShuffleVectorExpr(const ShuffleVectorExpr *E) {
    for (unsigned i = 0; i < E->getNumSubExprs(); ++i) {
      if (Visit(E->getExpr(i))) {
        return true;
      }
    }
    return false;
  }
  bool VisitVAArgExpr(const VAArgExpr *E) {
    return Visit(E->getSubExpr());
  }

  bool VisitMemberExpr(const MemberExpr *E) {
    return Visit(E->getBase());
  }
  bool VisitCXXBindTemporaryExpr(const CXXBindTemporaryExpr *E) {
    return Visit(E->getSubExpr());
  }
  bool VisitUnresolvedMemberExpr(const UnresolvedMemberExpr *E) {
    if (E->isImplicitAccess()) {
      return false;
    }
    return Visit(E->getBase());
  }
  bool VisitCXXDependentScopeMemberExpr(const CXXDependentScopeMemberExpr *E) {
    if (E->isImplicitAccess()) {
      return false;
    }
    return Visit(E->getBase());
  }
  bool VisitCXXPseudoDestructorExpr(const CXXPseudoDestructorExpr *E) {
    return Visit(E->getBase());
  }
  bool VisitCXXDefaultArgExpr(const CXXDefaultArgExpr *E) {
    return Visit(E->getExpr());
  }
  bool VisitCastExpr(const CastExpr *E) {
    return Visit(E->getSubExpr());
  }
  bool VisitExprWithCleanups(const ExprWithCleanups *E) {
    return Visit(E->getSubExpr());
  }
  bool VisitDeclRefExpr(const DeclRefExpr *E) {
    const ValueDecl *D = E->getDecl();
    if (const VarDecl *varDecl = dyn_cast<VarDecl>(D)) {
      return maybeFields.contains(varDecl);
    }
    else if (isa<EnumConstantDecl>(D)) {
      return false;
    }
    else if (isa<FunctionDecl>(D)) {
      return false;
    }
    else if (isa<NonTypeTemplateParmDecl>(D)) {
      return false;
    }
    // Result of taking a pointer to a member
    else if (isa<FieldDecl>(D)) {
      return false;
    }
    E->getDecl()->dump();
    llvm_unreachable("Unhandled DeclRefExpr");
    return true;
  }

  bool VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr *E) {
    return Visit(E->GetTemporaryExpr());
  }
  bool VisitParenExpr(const ParenExpr *E) {
    return Visit(E->getSubExpr());
  }
  bool VisitAtomicExpr(const AtomicExpr *E) {
    return Visit(E->getPtr()) || Visit(E->getVal1()) || Visit(E->getVal2());
  }

  // Binary Operators
  bool VisitBinPtrMemD(const BinaryOperator *O) { // .*
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinPtrMemI(const BinaryOperator *O) { // ->*
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinAssign(const BinaryOperator *O) {
    return Visit(O->getRHS());
  }
  bool VisitBinComma(const BinaryOperator *O) {
    return Visit(O->getRHS());
  }
  //// Multiplicatve Ops
  bool VisitBinMul(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinMulAssign(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinDiv(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinDivAssign(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinRem(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinRemAssign(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  //// Additive Ops
  bool VisitBinAdd(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinAddAssign(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinSub(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinSubAssign(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  //// Shift Ops (only use LHS for fields)
  bool VisitBinShl(const BinaryOperator *O) { // <<
    return Visit(O->getLHS());
  }
  bool VisitBinShlAssign(const BinaryOperator *O) { // <<
    return Visit(O->getLHS());
  }
  bool VisitBinShr(const BinaryOperator *O) { // >>
    return Visit(O->getLHS());
  }
  bool VisitBinShrAssign(const BinaryOperator *O) { // >>
    return Visit(O->getLHS());
  }
  //// Comparsion Ops
  bool VisitBinLT(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinGT(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinLE(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinGE(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinEQ(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinNE(const BinaryOperator *O) {
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  //// Logical Ops
  bool VisitBinAnd(const BinaryOperator *O) { // &
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinAndAssign(const BinaryOperator *O) { // &
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinOr(const BinaryOperator *O) { // |
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinOrAssign(const BinaryOperator *O) { // |
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinXor(const BinaryOperator *O) { // ^
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinXorAssign(const BinaryOperator *O) { // ^
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinLAnd(const BinaryOperator *O) { // &&
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  bool VisitBinLOr(const BinaryOperator *O) { // ||
    return Visit(O->getLHS()) || Visit(O->getRHS());
  }
  // Unary Operators
  //// Increment/decrement Ops
  bool VisitUnaryPostInc(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryPostDec(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryPreInc(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryPreDec(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  //// Memory Ops
  bool VisitUnaryAddrOf(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryDeref(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryPlus(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryMinus(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryNot(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryLNot(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  bool VisitUnaryExtension(const UnaryOperator *O) {
    return Visit(O->getSubExpr());
  }
  ////// Extensions
  bool VisitStmtExpr(const StmtExpr *E) {
    return Visit(E->getSubStmt());
  }

  bool VisitPredefinedExpr(const PredefinedExpr *E) {
    return false;
  }
  bool VisitUnresolvedLookupExpr(const UnresolvedLookupExpr *E) {
    return false;
  }

  // cond ? true : false (Don't consider a field leaking from the cond)
  bool VisitConditionalOperator(const ConditionalOperator *O) {
    return Visit(O->getTrueExpr()) || Visit(O->getFalseExpr());
  }

  // Only use the base
  bool VisitArraySubscriptExpr(const ArraySubscriptExpr *E) {
    return Visit(E->getBase());
  }

  bool VisitCXXNewExpr(const CXXNewExpr *E) {
    if (!E->getInitializer()) {
      return false;
    }
    return Visit(E->getInitializer());
  }
  bool VisitCXXThisExpr(const CXXThisExpr *E) {
    return true;
  }
  bool VisitPackExpansionExpr(const PackExpansionExpr *E) {
    return Visit(E->getPattern());
  }
  bool VisitCXXScalarValueInitExpr(const CXXScalarValueInitExpr *E) { return false; }
  bool VisitCXXTypeidExpr(const CXXTypeidExpr *E) {
    if (E->isTypeOperand()) {
      return false;
    }
    return E->getExprOperand();
  }
  // I think this is just a static access to a class named by a template
  bool VisitDependentScopeDeclRefExpr(const DependentScopeDeclRefExpr *E) { return false; }
  // Currently unhandled
  bool VisitLambdaExpr(const LambdaExpr *E) { return true; }

  // Literals
  bool VisitCXXBoolLiteralExpr(const CXXBoolLiteralExpr *L) { return false; }
  bool VisitCXXNullPtrLiteralExpr(const CXXNullPtrLiteralExpr *L) { return false; }
  bool VisitUserDefinedLiteral(const UserDefinedLiteral *L) { return false; }
  bool VisitCharacterLiteral(const CharacterLiteral *L) { return false; }
  bool VisitCompoundLiteralExpr(const CompoundLiteralExpr *L) { return false; }
  bool VisitFloatingLiteral(const FloatingLiteral *L) { return false; }
  bool VisitImaginaryLiteral(const ImaginaryLiteral *L) { return false; }
  bool VisitIntegerLiteral(const IntegerLiteral *L) { return false; }
  bool VisitObjCArrayLiteral(const ObjCArrayLiteral *L) { return false; }
  bool VisitObjCBoolLiteralExpr(const ObjCBoolLiteralExpr *L) { return false; }
  bool VisitObjCDictionaryLiteral(const ObjCDictionaryLiteral *L) { return false; }
  bool VisitObjCStringLiteral(const ObjCStringLiteral *L) { return false; }
  bool VisitStringLiteral(const StringLiteral *L) { return false; }
  bool VisitGNUNullExpr(const GNUNullExpr *E) { return false; }
  bool VisitImplicitValueInitExpr(const ImplicitValueInitExpr *E) { return false; }
  // alignof/sizeof
  bool VisitUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *E) { return false; }
  bool VisitOffsetOfExpr(const OffsetOfExpr *E) { return false; }
  bool VisitSizeOfPackExpr(const SizeOfPackExpr *E) { return false; }
};

}

bool Values::maybeField(const Expr *E) const {
  MaybeFieldsVisitor V(maybeFields);
  return V.Visit(E);
}

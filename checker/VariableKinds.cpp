#include "VariableKinds.h"

#include "Exprs.h"

#include <clang/AST/StmtVisitor.h>

using namespace clang;
using namespace immutability;

namespace {

class TransferFunctions : public StmtVisitor<TransferFunctions> {
  VariableKinds &parent;
  Values &vals;

  void killAll(const VarDecl *VD) {
    vals.maybeFields = parent.getVarSetFact().remove(vals.maybeFields, VD);
    vals.mustLiterals = parent.getVarSetFact().remove(vals.mustLiterals, VD);
  }
  void genMustLiteral(const VarDecl *VD) {
    vals.mustLiterals = parent.getVarSetFact().add(vals.mustLiterals, VD);
  }
  void genMaybeField(const VarDecl *VD) {
    vals.maybeFields = parent.getVarSetFact().add(vals.maybeFields, VD);
  }
  void handleAssignmentOp(const Expr *lhs, const Expr *rhs) {
    LHSTuple T = getLHSTuple(lhs);
    for (const Expr *root : T.roots) {
      const DeclRefExpr *declRef = dyn_cast<DeclRefExpr>(root);
      if (!declRef) {
        continue;
      }
      const VarDecl *varDecl = dyn_cast<VarDecl>(declRef->getDecl());
      if (!varDecl) {
        continue;
      }
      killAll(varDecl);
      if (vals.mustLiteral(rhs)) {
        genMustLiteral(varDecl);
      }
      if (vals.maybeField(rhs)) {
        genMaybeField(varDecl);
      }
    }
  }
public:
  TransferFunctions(VariableKinds &vk, Values &vs)
  :  parent(vk), vals(vs) {}

  void VisitDeclStmt(const DeclStmt *DS) {
    for (const auto *DI : DS->decls()) {
      if (const auto *varDecl = dyn_cast<VarDecl>(DI)) {
        killAll(varDecl);
        if (const auto *E = varDecl->getInit()) {
          if (vals.mustLiteral(E)) {
            genMustLiteral(varDecl);
          }
          if (vals.maybeField(E)) {
            genMaybeField(varDecl);
          }
        }
      }
    }
  }

  void VisitBinaryOperator(const BinaryOperator *B) {
    if (B->isAssignmentOp()) {
      handleAssignmentOp(B->getLHS(), B->getRHS());
    }
  }

  void VisitCXXOperatorCallExpr(const CXXOperatorCallExpr *E) {
    if (E->isAssignmentOp()) {
      handleAssignmentOp(E->getArg(0), E->getArg(1));
    }
  }
};

}

Values VariableKinds::runOnBlock(const CFGBlock *block, Values vals) {
  TransferFunctions TF(*this, vals);

  if (const Stmt *term = block->getTerminator()) {
    stmtsToValues[term] = vals;
    TF.Visit(const_cast<Stmt*>(term));
  }
  for (CFGBlock::const_iterator I = block->begin(), E = block->end();
       I != E; ++I) {
    const CFGElement &elem = *I;
    if (!elem.getAs<CFGStmt>()) {
      continue;
    }
    const Stmt *S = elem.castAs<CFGStmt>().getStmt();

    stmtsToValues[S] = vals;
    TF.Visit(const_cast<Stmt*>(S));
  }

  return vals;
}

void VariableKinds::dump(SourceManager &SM) const {
  for (const auto &stmtValues : stmtsToValues) {
    const Stmt *stmt = stmtValues.first;
    const Values &values = stmtValues.second;
    auto locStartData = SM.getCharacterData(stmt->getLocStart());
    auto locEndData = SM.getCharacterData(stmt->getLocEnd());
    llvm::StringRef loc(locStartData, locEndData - locStartData + 1);
    llvm::errs() << "  stmt: " << loc << '\n';
    llvm::errs() << "    " << stmt->getStmtClassName() << '\n';
    llvm::errs() << "  values (before)\n";
    llvm::errs() << "    fields: ";
    values.dumpFields();
    llvm::errs() << '\n';
    llvm::errs() << "    literals: ";
    values.dumpLiterals();
    llvm::errs() << '\n';
  }
}

#ifndef CLANG_IMMUTABILITY_CHECK_VARIABLE_KINDS_H
#define CLANG_IMMUTABILITY_CHECK_VARIABLE_KINDS_H

#include "Values.h"
#include "Worklist.h"

#include <clang/AST/DeclCXX.h>

namespace clang {
namespace immutability {

class VariableKinds {

  Values::SetRef unionSets(Values::SetRef A, Values::SetRef B) {
    if (A.isEmpty()) {
      return B;
    }
    for (Values::SetRef::iterator I = B.begin(), E = B.end(); I != E; ++I) {
      A = A.add(*I);
    }
    return A;
  }
  Values::SetRef intersectSets(Values::SetRef A, Values::SetRef B) {
    if (A.isEmpty()) {
      return A;
    }
    for (Values::SetRef::iterator I = B.begin(), E = B.end(); I != E; ++I) {
      A = A.remove(*I);
    }
    return A;
  }
  Values merge(Values valsA, Values valsB) {
    Values::SetRef
      maybeFieldsRefA(valsA.maybeFields.getRootWithoutRetain(),
                      varSetFact.getTreeFactory()),
      maybeFieldsRefB(valsB.maybeFields.getRootWithoutRetain(),
                      varSetFact.getTreeFactory());

    Values::SetRef
      mustLiteralsRefA(valsA.maybeFields.getRootWithoutRetain(),
                       varSetFact.getTreeFactory()),
      mustLiteralsRefB(valsB.maybeFields.getRootWithoutRetain(),
                       varSetFact.getTreeFactory());

    maybeFieldsRefA = unionSets(maybeFieldsRefA, maybeFieldsRefB);
    mustLiteralsRefA = intersectSets(mustLiteralsRefA, mustLiteralsRefB);

    return Values(maybeFieldsRefA.asImmutableSet(),
                  mustLiteralsRefA.asImmutableSet());
  }
  Values mergePreds(const CFGBlock *block) {
    Values current;
    bool first = true;
    for (CFGBlock::const_pred_iterator I = block->pred_begin(),
         E = block->pred_end(); I != E; ++I) {
      const CFGBlock *pred = *I;
      if (!pred) {
        continue;
      }
      if (first) {
        current = blocksEndToValues[pred];
        first = false;
      }
      else {
        current = merge(current, blocksEndToValues[pred]);
      }
    }

    return current;
  }

  Values runOnBlock(const CFGBlock *block, Values vals);

  class Observer : public CFGCallback {
  public:
    virtual void compareAlwaysTrue(const BinaryOperator *B, bool isAlwaysTrue) {
    }
    virtual void compareBitwiseEquality(const BinaryOperator *B,
                                        bool isAlwaysTrue) {
    }
  };

  std::unique_ptr<CFG> cfg;
  std::unique_ptr<Worklist> worklist;
  Values::Set::Factory varSetFact;
  llvm::DenseMap<const CFGBlock *, Values> blocksBeginToValues;
  llvm::DenseMap<const CFGBlock *, Values> blocksEndToValues;
  llvm::DenseMap<const Stmt *, Values> stmtsToValues;
  bool valid;
public:
  VariableKinds(const CXXMethodDecl *D)
  : varSetFact(false), valid(true) {
    CFG::BuildOptions buildOptions;
    Observer observer;
    buildOptions.Observer = &observer;
    cfg = CFG::buildCFG(D, D->getBody(),
                        &D->getASTContext(), buildOptions);
    // Cannot build CFG
    if (!cfg) {
      valid = false;
      return;
    }
    worklist = llvm::make_unique<Worklist>(*cfg);
    worklist->enqueueBlock(&cfg->getEntry());
    llvm::BitVector everAnalyzedBlock(cfg->getNumBlockIDs());
    while (const CFGBlock *block = worklist->dequeue()) {
      Values &prev = blocksBeginToValues[block];

      Values current = mergePreds(block);

      auto id = block->getBlockID();
      if (!everAnalyzedBlock[id]) {
        everAnalyzedBlock[id] = true;
      }
      else if (prev.equals(current)) {
        continue;
      }

      blocksEndToValues[block] = runOnBlock(block, current);

      prev = current;
      worklist->enqueueSuccessors(block);
    }
  }

  Values::Set::Factory &getVarSetFact() {
    return varSetFact;
  }

  void dump(SourceManager &SM) const;

  bool isValid() const {
    return valid;
  }
  bool maybeField(const Stmt *S, const VarDecl *D) {
    return stmtsToValues[S].maybeField(D);
  }
  bool maybeField(const Stmt *S, const Expr *E) {
    return stmtsToValues[S].maybeField(E);
  }
  bool mustLiteral(const Stmt *S, const VarDecl *D) {
    return stmtsToValues[S].mustLiteral(D);
  }
  bool mustLiteral(const Stmt *S, const Expr *E) {
    return stmtsToValues[S].mustLiteral(E);
  }
};

}
}

#endif

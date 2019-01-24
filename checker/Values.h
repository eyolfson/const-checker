#ifndef CLANG_IMMUTABILITY_CHECK_VALUES_H
#define CLANG_IMMUTABILITY_CHECK_VALUES_H

#include <clang/AST/Decl.h>
#include <llvm/ADT/ImmutableSet.h>

namespace clang {
namespace immutability {

class Values {
public:
  typedef llvm::ImmutableSet<const VarDecl *> Set;
  typedef llvm::ImmutableSetRef<const VarDecl *> SetRef;

  Set maybeFields;
  Set mustLiterals;

  Values()
  : maybeFields(nullptr), mustLiterals(nullptr) {}
  Values(Set fields, Set literals)
  : maybeFields(fields), mustLiterals(literals) {}

  bool maybeField(const VarDecl *D) const {
    return maybeFields.contains(D);
  }
  bool maybeField(const Expr *E) const;

  bool mustLiteral(const VarDecl *D) const {
    return mustLiterals.contains(D);
  }
  bool mustLiteral(const Expr *E) const;

  bool equals(const Values &V) const {
    return maybeFields == V.maybeFields && mustLiterals == V.mustLiterals;
  }

  void dumpFields() const {
    bool first = true;
    for (const VarDecl *varDecl : maybeFields) {
      if (first) {
        first = false;
      }
      else {
        llvm::errs() << ", ";
      }
      llvm::errs() << varDecl->getQualifiedNameAsString();
    }
  }
  void dumpLiterals() const {
    bool first = true;
    for (const VarDecl *varDecl : mustLiterals) {
      if (first) {
        first = false;
      }
      else {
        llvm::errs() << ", ";
      }
      llvm::errs() << varDecl->getQualifiedNameAsString();
    }
  }
};

}
}

#endif

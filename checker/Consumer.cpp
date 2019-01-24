#include "Consumer.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/CXXInheritance.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Support/raw_ostream.h>

#include "Database.h"
#include "Methods.h"
#include "Types.h"

namespace clang {

using namespace immutability;

namespace {

bool isSkippedRecord(ClangDatabase &ClangDB, const CXXRecordDecl *D) {
  if (!D->hasDefinition())
    return true;

  if (D->isAbstract())
    return true;

  if (D->hasAnyDependentBases())
    return true;

  if (D->isAnonymousStructOrUnion())
    return true;

  if (D->isEnum() || D->isUnion())
    return true;

  uint32_t PresumedLocID = ClangDB.getPresumedLocID(D);
  if (PresumedLocID == 0)
    return true;

  return false;
}

class InsertIntoDatabaseConsumer
    : public ASTConsumer,
      public RecursiveASTVisitor<InsertIntoDatabaseConsumer> {
private:
  void addUnskippedPublicMethods(
      const CXXRecordDecl *Class,
      llvm::SmallPtrSet<const CXXMethodDecl *, 16> &PublicMethods,
      llvm::SmallPtrSet<const CXXMethodDecl *, 16> &OverriddenMethods) {
    for (auto Decl : Class->decls()) {
      auto Template = dyn_cast<FunctionTemplateDecl>(Decl);
      if (!Template) {
        continue;
      }
      auto Method = dyn_cast<CXXMethodDecl>(Template->getTemplatedDecl());
      if (!Method) {
        continue;
      }
      if (ClangDB.isSkippedMethod(Method))
        continue;
      if (Method->getAccess() == AS_public) {
        if (!OverriddenMethods.count(Method)) {
          PublicMethods.insert(Method);
        }
        for (auto OverriddenMethod : Method->overridden_methods()) {
          OverriddenMethods.insert(OverriddenMethod);
        }
      }
    }

    for (auto Method : Class->methods()) {
      if (ClangDB.isSkippedMethod(Method))
        continue;
      if (Method->getAccess() == AS_public) {
        if (!OverriddenMethods.count(Method)) {
          PublicMethods.insert(Method);
        }
        for (auto OverriddenMethod : Method->overridden_methods()) {
          OverriddenMethods.insert(OverriddenMethod);
        }
      }
    }
  }

  void addPublicMethodsAndFields(
      const CXXRecordDecl *Class,
      llvm::SmallPtrSet<const CXXMethodDecl *, 16> &PublicMethods,
      std::vector<FieldDecl *> &PublicFields) {
    llvm::SmallPtrSet<const CXXMethodDecl *, 16> OverriddenMethods;

    addUnskippedPublicMethods(Class, PublicMethods, OverriddenMethods);
    for (auto Field : Class->fields()) {
      if (Field->getAccess() == AS_public)
        PublicFields.push_back(Field);
    }

    auto UntilLeaf = [](const CXXBaseSpecifier *Specifier, CXXBasePath &Path) {
      CXXRecordDecl *Class = Specifier->getType()->getAsCXXRecordDecl();
      if (Class->getNumBases() == 0 && Class->getNumVBases() == 0)
        return true;
      else
        return false;
    };

    CXXBasePaths Paths;
    Class->lookupInBases(UntilLeaf, Paths, /*LookupInDependent=*/ false);
    for (CXXBasePath &Path : Paths) {
      for (CXXBasePathElement &Element : Path) {
        // Stop exploring this path, no inherited methods or fields are public
        const CXXBaseSpecifier *Specifier = Element.Base;
        if (Specifier->getAccessSpecifier() != AS_public) {
          break;
        }

        CXXRecordDecl *Base = Specifier->getType()->getAsCXXRecordDecl();
        addUnskippedPublicMethods(Base, PublicMethods, OverriddenMethods);
        for (auto Field : Base->fields()) {
          if (Field->getAccess() == AS_public)
            PublicFields.push_back(Field);
        }
      }
    }
  }

public:
  explicit InsertIntoDatabaseConsumer(Database &DB,
				      ASTContext &Ctx,
				      SourceManager &SM)
      : Ctx(Ctx), SM(SM), DB(DB), ClangDB(DB, Ctx, SM) {
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    TranslationUnitDecl *D = Context.getTranslationUnitDecl();
    TraverseDecl(D);
  }
  void HandleVTable(CXXRecordDecl *RD) override {
  }
  bool VisitCXXRecordDecl(CXXRecordDecl *D) {
    D = D->getCanonicalDecl();

    if (isSkippedRecord(ClangDB, D)) {
      return true;
    }

    auto UntilLeaf = [](const CXXBaseSpecifier *Specifier, CXXBasePath &Path) {
      CXXRecordDecl *Class = Specifier->getType()->getAsCXXRecordDecl();
      if (Class->getNumBases() == 0 && Class->getNumVBases() == 0)
        return true;
      else
        return false;
    };

    CXXBasePaths Paths;
    D->lookupInBases(UntilLeaf, Paths, /*LookupInDependent=*/ false);
    for (CXXBasePath &Path : Paths) {
      for (CXXBasePathElement &Element : Path) {
        // Stop exploring this path, no inherited methods or fields are public
        const CXXBaseSpecifier *Specifier = Element.Base;
        if (Specifier->getAccessSpecifier() != AS_public) {
          break;
        }

        CXXRecordDecl *Base = Specifier->getType()->getAsCXXRecordDecl();
        uint32_t PresumedLocID = ClangDB.getPresumedLocID(Base);
        if (PresumedLocID == 0)
          return true;
      }
    }

    if (D->getQualifiedNameAsString() == "") {
      D->dump();
      llvm::errs() << "Class: " << D->getQualifiedNameAsString() << '\n';
    }

    llvm::SmallPtrSet<const CXXMethodDecl *, 16> PublicMethods;
    std::vector<FieldDecl *> PublicFields;

    addPublicMethodsAndFields(D, PublicMethods, PublicFields);

    for (auto Method : PublicMethods) {
      ClangDB.insertPublicMethod(D, Method);
    }
    for (auto Field : PublicFields) {
      ClangDB.insertPublicField(D, Field);
    }

    return true;
  }

  bool VisitCXXMethodDecl(CXXMethodDecl *D) {
    D = D->getCanonicalDecl();

    if (!D->isThisDeclarationADefinition())
      if (D->isDefined())
        D = cast<CXXMethodDecl>(D->getDefinition());
      else
        return true;

    if (ClangDB.isSkippedMethod(D))
      return true;

    uint32_t PresumedLocID = ClangDB.getPresumedLocID(D);
    if (PresumedLocID == 0)
      return true;

    for (const CXXMethodDecl *Overridden : D->overridden_methods()) {
      ClangDB.insertMethodDependence(D, Overridden);
      ClangDB.insertMethodDependence(Overridden, D);
    }

    MethodResultTuple R;
    {
      MethodResult MR(ClangDB);
      R = MR.getMethodResult(D);
    }
    ClangDB.insertMethodCheck(D, R);

    return true;
  }

  bool VisitFieldDecl(FieldDecl *D) {
    D = D->getCanonicalDecl();

    uint32_t PresumedLocID = ClangDB.getPresumedLocID(D);
    if (PresumedLocID == 0)
      return true;

    bool isExplicit = isExplicitlyConst(D->getType());
    bool isTransitive = isTransitivelyConst(D->getType());
    ClangDB.insertFieldCheck(D, isExplicit, isTransitive);

    return true;
  }

private:
  Database &DB;
  ClangDatabase ClangDB;
  const ASTContext &Ctx;
  const SourceManager &SM;
};


}

std::unique_ptr<ASTConsumer> CreateInsertIntoDatabaseConsumer(
    Database &DB, ASTContext &Ctx, SourceManager &SM) {
  return llvm::make_unique<InsertIntoDatabaseConsumer>(DB, Ctx, SM);
}

} // end namespace clang

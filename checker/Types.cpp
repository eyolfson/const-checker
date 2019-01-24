#include "Types.h"

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/TemplateBase.h>

using namespace clang;

namespace {

bool isTransitivelyConst(QualType QT, bool First) {
  auto S = QT.split();

  if (!(First || isa<ReferenceType>(S.Ty))) {
    if (!S.Quals.hasConst()) {
      return false;
    }
  }

  if (auto PT = dyn_cast<PointerType>(S.Ty)) {
    return isTransitivelyConst(PT->getPointeeType(), false);
  }
  else if (auto RT = dyn_cast<ReferenceType>(S.Ty)) {
    return isTransitivelyConst(RT->getPointeeType(), false);
  }
  else if (auto TST = dyn_cast<TemplateSpecializationType>(S.Ty)) {
    for (auto TA : TST->template_arguments()) {
      bool IsConstTransitive;
      switch (TA.getKind()) {
      case TemplateArgument::ArgKind::Type:
        IsConstTransitive = isTransitivelyConst(TA.getAsType(), true);
        break;
      case TemplateArgument::ArgKind::Expression:
        IsConstTransitive = isTransitivelyConst(TA.getAsExpr()->getType(),
                                                true);
        break;
      case TemplateArgument::ArgKind::Declaration:
        break;
      case TemplateArgument::ArgKind::NullPtr:
        break;
      case TemplateArgument::ArgKind::Integral:
        break;
      case TemplateArgument::ArgKind::Template:
        break;
      case TemplateArgument::ArgKind::TemplateExpansion:
        break;
      case TemplateArgument::ArgKind::Pack:
        break;
      default:
        llvm_unreachable("???");
      }

      if (!IsConstTransitive) {
        return false;
      }
    }
    return true;
  }
  else if (auto RT = dyn_cast<RecordType>(S.Ty)) {
    const RecordDecl *D = RT->getDecl();
    if (const auto *CTSD = dyn_cast<ClassTemplateSpecializationDecl>(D)) {
      const auto &TAL = CTSD->getTemplateArgs();
      for (int i = 0; i < TAL.size(); ++i) {
        const auto &TA = TAL[i];
        bool IsConstTransitive;
        switch (TA.getKind()) {
        case TemplateArgument::ArgKind::Type:
          IsConstTransitive = isTransitivelyConst(TA.getAsType(), true);
          break;
        case TemplateArgument::ArgKind::Expression:
          IsConstTransitive = isTransitivelyConst(TA.getAsExpr()->getType(),
                                                  true);
          break;
        case TemplateArgument::ArgKind::Declaration:
          break;
        case TemplateArgument::ArgKind::NullPtr:
          break;
        case TemplateArgument::ArgKind::Integral:
          break;
        case TemplateArgument::ArgKind::Template:
          break;
        case TemplateArgument::ArgKind::TemplateExpansion:
          break;
        case TemplateArgument::ArgKind::Pack:
          break;
        default:
          llvm_unreachable("???");
        }

        if (!IsConstTransitive) {
          return false;
        }
      }
      return true;
    }
    return true;
  }
  else {
    return true;
  }
}

}

namespace clang {
namespace immutability {

bool isTransitivelyConst(QualType T) {
  return ::isTransitivelyConst(T.getCanonicalType(), true);
}

}
}

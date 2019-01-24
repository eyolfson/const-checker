#ifndef CLANG_IMMUTABILITY_CHECK_TYPES_H
#define CLANG_IMMUTABILITY_CHECK_TYPES_H

#include <clang/AST/Type.h>

namespace clang {
namespace immutability {

inline bool isExplicitlyConst(QualType T) {
  return T.getCanonicalType().isConstQualified();
}
bool isTransitivelyConst(QualType T);

}
}

#endif

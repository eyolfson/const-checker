#ifndef CLANG_IMMUTABILITY_CHECK_EXPRS_H
#define CLANG_IMMUTABILITY_CHECK_EXPRS_H

#include <clang/AST/Expr.h>

namespace clang {
namespace immutability {

struct LHSTuple {
  std::vector<const Expr *> roots;
  bool isDereference;
};

LHSTuple getLHSTuple(const Expr *E);

}
}

#endif

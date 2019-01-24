#ifndef CLANG_IMMUTABILITY_CHECK_METHODS_H
#define CLANG_IMMUTABILITY_CHECK_METHODS_H

#include <clang/AST/DeclCXX.h>

#include "Database.h"
#include "MethodResultTuple.h"

namespace clang {
namespace immutability {

class MethodResult {
public:
  MethodResult(ClangDatabase &ClangDB) : ClangDB(ClangDB) {}
  MethodResultTuple getMethodResult(const CXXMethodDecl *D);
private:
  ClangDatabase &ClangDB;
  const CXXMethodDecl *Method;
};

}
}

#endif

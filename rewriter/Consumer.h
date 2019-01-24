#ifndef CLANG_IMMUTABILITY_REWRITE_CONSUMER_H
#define CLANG_IMMUTABILITY_REWRITE_CONSUMER_H

#include <clang/AST/ASTConsumer.h>

namespace clang {
namespace immutability {

class RewriteConsumer : public ASTConsumer {
  CompilerInstance &Instance;
public:
  RewriteConsumer(CompilerInstance &Instance) : Instance(Instance) {}
  void HandleTranslationUnit(ASTContext &Ctx) override {
  }
};

}
}

#endif

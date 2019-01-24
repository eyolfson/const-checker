#ifndef CLANG_IMMUTABILITY_REWRITE_ACTION_H
#define CLANG_IMMUTABILITY_REWRITE_ACTION_H

#include <clang/Frontend/FrontendPluginRegistry.h>

#include "Consumer.h"

namespace clang {
namespace immutability {

class RewriteAction : public PluginASTAction {

protected:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
						 llvm::StringRef) override {
    return llvm::make_unique<RewriteConsumer>(CI);
  }
  bool ParseArgs(const CompilerInstance &CI,
		 const std::vector<std::string> &args) override {
    return true;
  }
};

}
}

#endif

#include "Action.h"

#include "Consumer.h"

#include <clang/Frontend/CompilerInstance.h>

using namespace clang;

std::unique_ptr<ASTConsumer>
InsertIntoDatabaseAction::CreateASTConsumer(CompilerInstance &CI,
                                            llvm::StringRef InFile) {

  return CreateInsertIntoDatabaseConsumer(DB,
					  CI.getASTContext(),
                                          CI.getSourceManager());
}

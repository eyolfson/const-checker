#ifndef INSERT_INTO_DATABASE_ACTION_H
#define INSERT_INTO_DATABASE_ACTION_H

#include <clang/Frontend/FrontendAction.h>

#include "Database.h"

class InsertIntoDatabaseAction : public clang::ASTFrontendAction {
public:
  InsertIntoDatabaseAction(clang::immutability::Database &DB) : DB(DB) {}
protected:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance &CI, llvm::StringRef InFile) override;
private:
  clang::immutability::Database &DB;
};

#endif

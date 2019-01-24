#ifndef INSERT_INTO_DATABASE_CONSUMER_H
#define INSERT_INTO_DATABASE_CONSUMER_H

#include "Database.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/Basic/SourceManager.h>
#include <memory>

namespace clang {

std::unique_ptr<ASTConsumer> CreateInsertIntoDatabaseConsumer(
    immutability::Database &DB, ASTContext &Ctx, SourceManager &SM);
}

#endif

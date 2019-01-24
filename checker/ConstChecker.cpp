#include <clang/Frontend/FrontendActions.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/Version.h>
#include <clang/AST/Mangle.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>

#include "Action.h"

#include "Database.h"
#include "Methods.h"
#include "PostgresCompliationDatabase.h"

#include "Types.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::immutability;
using namespace clang::tooling;
using namespace llvm;

namespace {

class CheckFactory : public FrontendActionFactory {
public:
  CheckFactory(Database &DB) : DB(DB) {}
  FrontendAction *create() override {
    return new InsertIntoDatabaseAction(DB);
  }
private:
  Database &DB;
};

}

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  llvm::cl::OptionCategory Category("clang-immutability-check Options");
  cl::opt<unsigned> CompileCommandID(
      cl::Positional, cl::desc("<compile_command_id>"), cl::Required,
      cl::cat(Category));
  cl::ResetAllOptionOccurrences();
  cl::HideUnrelatedOptions(Category);
  cl::ParseCommandLineOptions(argc, argv);

  Database DB(CompileCommandID);
  std::vector<std::string> Sources = { DB.getSource() };
  clang::immutability::PostgresCompilationDatabase CompilationDatabase(DB);

  ClangTool Tool(CompilationDatabase, Sources);

  CheckFactory Factory(DB);
  return Tool.run(&Factory);
}

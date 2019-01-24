#include <clang/Basic/Version.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>

#include "Database.h"
#include "PostgresCompliationDatabase.h"

#include "Action.h"

using namespace clang;
using namespace clang::immutability;
using namespace clang::tooling;

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  llvm::cl::OptionCategory Category("clang-immutability-check Options");
  llvm::cl::opt<unsigned> CompileCommandID(
      llvm::cl::Positional,
      llvm::cl::desc("<compile_command_id>"),
      llvm::cl::Required,
      llvm::cl::cat(Category));
  llvm::cl::ResetAllOptionOccurrences();
  llvm::cl::HideUnrelatedOptions(Category);
  llvm::cl::ParseCommandLineOptions(argc, argv);

  Database DB(CompileCommandID);
  std::vector<std::string> Sources = { DB.getSource() };
  clang::immutability::PostgresCompilationDatabase CompilationDatabase(DB);

  ClangTool Tool(CompilationDatabase, Sources);
  
  int Ret = 0;
  return Tool.run(newFrontendActionFactory<RewriteAction>().get());
}

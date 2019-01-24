#ifndef CLANG_IMMUTABILITY_CHECK_POSTGRES_COMPLIATION_DATABASE_H
#define CLANG_IMMUTABILITY_CHECK_POSTGRES_COMPLIATION_DATABASE_H

#include "Database.h"

#include <clang/Tooling/Tooling.h>

namespace clang {
namespace immutability {

class PostgresCompilationDatabase : public clang::tooling::CompilationDatabase {
public:
  PostgresCompilationDatabase(Database &DB) {
    CC.Directory = DB.getDirectory();
    CC.Filename = DB.getSource();;
    CC.CommandLine = DB.getCommands();
    CC.CommandLine.push_back("-I/usr/lib/clang/"
                             CLANG_VERSION_STRING
                             "/include");
    // CC.CommandLine.push_back("-I/usr/include/tirpc");
    // CC.CommandLine.push_back("-I/usr/share/skypeforlinux/glibc/usr/include");
  }

  std::vector<clang::tooling::CompileCommand>
  getCompileCommands(StringRef FilePath) const override {
    std::vector<clang::tooling::CompileCommand> Commands;
    if (FilePath == CC.Filename) {
      Commands.push_back(CC);
    }
    return Commands;
  }
  std::vector<std::string> getAllFiles() const override {
    std::vector<std::string> Files;
    Files.push_back(CC.Filename);
    return Files;
  }
  std::vector<clang::tooling::CompileCommand> getAllCompileCommands() const override {
    std::vector<clang::tooling::CompileCommand> Commands;
    Commands.push_back(CC);
    return Commands;
  }

private:
  clang::tooling::CompileCommand CC;
};

}
}

#endif

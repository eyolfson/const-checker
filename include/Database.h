#ifndef CLANG_IMMUTABILITY_CHECK_DATABASE_H
#define CLANG_IMMUTABILITY_CHECK_DATABASE_H

#include "MethodResultTuple.h"

#include <clang/AST/DeclCXX.h>
#include <clang/Tooling/CompilationDatabase.h>

namespace clang {
namespace immutability {
  
struct DatabaseImpl;

class Database {
public:
  explicit Database(unsigned CompileCommandID);
  ~Database();
  std::string getSource();
  std::string getDirectory();
  std::vector<std::string> getCommands();
  uint32_t getCompileCommandID() const;
  uint32_t getPackageID() const;
  uint32_t getRootDeclID() const;
  uint32_t getFileDescriptorID(StringRef FullPath);
private:
  std::string getSourceDirectory() const;
  uint32_t getFileDescriptorIDFromPath(StringRef Path);
  std::unique_ptr<DatabaseImpl> Impl;

  friend class ClangDatabase;
};

struct ClangDatabaseImpl;

class ClangDatabase {
public:
  ClangDatabase(Database &DB,
                ASTContext &Ctx,
                SourceManager &SM);
  ~ClangDatabase();
  uint32_t getPresumedLocID(const Decl *D);
  bool isSkippedMethod(const CXXMethodDecl *MD);
  void insertPublicMethod(const CXXRecordDecl *RD, const CXXMethodDecl *MD);
  void insertPublicField(const CXXRecordDecl *RD, const FieldDecl *FD);
  void insertMethodCheck(const CXXMethodDecl *MD, MethodResultTuple Result);
  void insertFieldCheck(const FieldDecl *FD, bool isExplicit, bool isTransitive);
  void insertMethodDependence(const CXXMethodDecl *Method, const CXXMethodDecl *Callee);
private:
  std::string getMangledName(const CXXMethodDecl *D);
  std::string getSignature(const FunctionDecl *Target, bool Qualified);
  uint32_t getPresumedLocIDPLoc(PresumedLoc PLoc);
  void insertMethod(const CXXMethodDecl *MD);
  uint32_t getDeclID(const Decl *D);
  std::unique_ptr<DatabaseImpl> &getDatabaseImpl() const;
  std::unique_ptr<ClangDatabaseImpl> Impl;
};

}
}

#endif

#include "Database.h"

#include <sstream>
#include <unordered_set>
#include <unordered_map>

#include <libpq-fe.h>
#include <arpa/inet.h>

#include <clang/AST/CXXInheritance.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Mangle.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/StringMap.h>

using namespace llvm;
using namespace clang;
using namespace clang::immutability;
using namespace clang::tooling;

namespace clang {
namespace immutability {

struct DatabaseImpl {
  DatabaseImpl() = default;
  DatabaseImpl(const DatabaseImpl &Impl) = delete;

  unsigned CompileCommandID;
  std::string SourceDirectory;

  PGconn *Connection;
  uint32_t PackageID;
  uint32_t RootDeclID;

  StringMap<uint32_t> FDCache;
};

struct ClangDatabaseImpl {
  ClangDatabaseImpl(Database &DB, ASTContext &Ctx, SourceManager &SM)
      : DB(DB), SM(SM), PP(Ctx.getPrintingPolicy()) {
    PP.SuppressSpecifiers = false;
    PP.SuppressTagKeyword = true;
    PP.IncludeTagDefinition = false;
    PP.SuppressScope = true;
    PP.SuppressUnwrittenScope = true;
    PP.SuppressInitializers = false;
    PP.ConstantArraySizeAsWritten = true;
    PP.AnonymousTagLocations = false;
    PP.SuppressStrongLifetime = true;
    PP.SuppressLifetimeQualifiers = true;
    PP.SuppressTemplateArgsInCXXConstructors = true;
    PP.Bool = true;
    PP.Restrict = true;
    PP.Alignof = true;
    PP.UnderscoreAlignof = false;
    PP.UseVoidForZeroParams = false;
    PP.TerseOutput = false;
    PP.PolishForDeclaration = true;
    PP.Half = true;
    PP.MSWChar = false;
    PP.IncludeNewlines = true;
    PP.MSVCFormatting = false;
    Mangler = ItaniumMangleContext::create(Ctx, SM.getDiagnostics());
  }

  Database &DB;

  SourceManager &SM;
  PrintingPolicy PP;
  ItaniumMangleContext *Mangler;

  std::unordered_map<const RecordDecl *, uint32_t> RecordIDCache;
  std::unordered_map<const NamespaceDecl *, uint32_t> NamespaceIDCache;
  std::unordered_map<const CXXMethodDecl *, uint32_t> MethodIDCache;
  std::unordered_map<const FieldDecl *, uint32_t> FieldIDCache;
  std::unordered_map<const FunctionDecl *, uint32_t> FunctionIDCache;
};

}
}

namespace {

class Params {
  std::vector<const char *> Values;
  std::list<uint32_t> BinaryValues; // Required for stable iterators
  std::vector<int> Lengths;
  std::vector<int> Formats;
public:
  void addText(const char *Text) {
    Values.push_back(Text);
    Lengths.push_back(0); // Ignored for text, used for binary
    Formats.push_back(0); // 0 is text, 1 is binary
  }
  void addBinary(uint32_t Binary) {
    BinaryValues.push_back(htonl(Binary));
    auto &BinaryValue = BinaryValues.back();
    const char *Value = (const char *) &BinaryValue;

    Values.push_back(Value);
    Lengths.push_back(sizeof(BinaryValue));
    Formats.push_back(1); // 1 is binary
  }
  void addBool(bool B) {
    if (B)
      addText("true");
    else
      addText("false");
  }
  void clear() {
    Values.clear();
    BinaryValues.clear();
    Lengths.clear();
    Formats.clear();
  }

  const char * const * getValues() const {
    return Values.data();
  }
  const int * getLengths() const {
    return Lengths.data();
  }
  const int * getFormats() const {
    return Formats.data();
  }
  int getN() const {
    assert(Values.size() == Lengths.size());
    assert(Values.size() == Formats.size());
    return Values.size();
  }

  void dump() const {
    auto iBinary = BinaryValues.begin();
    for (size_t i = 0; i < getN(); ++i) {
      llvm::errs() << "    ." << i << " = ";
      if (Formats[i] == 0) {
        llvm::errs() << Values[i];
      }
      else {
        llvm::errs() << ntohl(*iBinary);
        ++iBinary;
      }
      llvm::errs() << '\n';
    }
  }
};

class Result {
protected:
  PGresult *PGResult;
public:
  Result(std::unique_ptr<DatabaseImpl> &Impl,
         const char *Q, const Params &P) : PGResult(nullptr) {
    PGResult = PQexecParams(Impl->Connection, Q, P.getN(), nullptr,
                            P.getValues(), P.getLengths(), P.getFormats(), 1);
    assert(PGResult != nullptr);
  }
  ~Result() {
    PQclear(PGResult);
  }

  Result(const Result &) = delete;
  Result operator=(const Result &) = delete;
};

class TupleResult : public Result {
public:
  TupleResult(std::unique_ptr<DatabaseImpl> &Impl,
              const char *Q, const Params &P) : Result(Impl, Q, P) {
    auto Status = PQresultStatus(PGResult);
    if (Status != PGRES_TUPLES_OK) {
      errs() << "TupleResult: " << PQresultErrorMessage(PGResult);
      errs() << "Query: " << Q << '\n';
      P.dump();
    }
    assert(Status == PGRES_TUPLES_OK && "TupleResult failed");
  }

  TupleResult(const TupleResult &) = delete;
  TupleResult operator=(const TupleResult &) = delete;

  int getNumTuples() {
    return PQntuples(PGResult);
  }
  uint32_t getID() {
    assert(getNumTuples() == 1);
    int FieldIndex = PQfnumber(PGResult, "id");
    char *Value = PQgetvalue(PGResult, 0, FieldIndex);
    uint32_t ID = ntohl(*((uint32_t *) Value));
    return ID;
  }
  uint32_t getID(const char *FieldName) {
    assert(getNumTuples() == 1);
    int FieldIndex = PQfnumber(PGResult, FieldName);
    char *Value = PQgetvalue(PGResult, 0, FieldIndex);
    uint32_t ID = ntohl(*((uint32_t *) Value));
    return ID;
  }
  const char *getValue(const char *FieldName) {
    assert(getNumTuples() == 1);
    int FieldIndex = PQfnumber(PGResult, FieldName);
    char *Value = PQgetvalue(PGResult, 0, FieldIndex);
    return Value;
  }
  uint32_t getBinary() {
    assert(getNumTuples() == 1);
    assert(PQnfields(PGResult) == 1);
    char *Value = PQgetvalue(PGResult, 0, 0);
    uint32_t Binary = ntohl(*((uint32_t *) Value));
    return Binary;
  }
};

class CommandResult : public Result {
public:
  CommandResult(std::unique_ptr<DatabaseImpl> &Impl,
		const char *Q, const Params &P) : Result(Impl, Q, P) {
    if (PQresultStatus(PGResult) != PGRES_COMMAND_OK) {
      errs() << "CommandResult: " << PQresultErrorMessage(PGResult);
    }
    assert(PQresultStatus(PGResult) == PGRES_COMMAND_OK);
  }

  CommandResult(const CommandResult &) = delete;
  CommandResult operator=(const CommandResult &) = delete;
};

}

namespace clang {
namespace immutability {

Database::Database(unsigned CompileCommandID) {
  Impl = llvm::make_unique<DatabaseImpl>();

  Impl->CompileCommandID = CompileCommandID;

  Impl->Connection = PQconnectdb("dbname = cpp_doc");
  assert(PQstatus(Impl->Connection) == CONNECTION_OK);

  Params P;

  P.addBinary(CompileCommandID);
  TupleResult CompileCommandSelect(Impl, "SELECT * FROM cpp_doc_compile_command WHERE id = $1", P);
  Impl->PackageID = CompileCommandSelect.getID("package_id");

  P.clear();
  P.addBinary(Impl->PackageID);
  TupleResult PackageSelect(Impl, "SELECT * FROM cpp_doc_package WHERE id = $1", P);
  uint32_t PackageNameID = PackageSelect.getID("package_name_id");

  P.clear();
  P.addBinary(PackageNameID);
  TupleResult PackageNameSelect(Impl, "SELECT * FROM cpp_doc_package_name WHERE id = $1", P);

  std::stringstream ss;
  ss << "/home/jon/src/uwaterloo/immutability-experiments/";
  ss << PackageNameSelect.getValue("slug");
  ss << '/';
  ss << PackageSelect.getValue("version");
  ss << "/src/";
  Impl->SourceDirectory = ss.str();

  P.clear();
  P.addBinary(Impl->PackageID);
  TupleResult RootDeclSelect(Impl, "SELECT get_root_decl($1)", P);
  Impl->RootDeclID = RootDeclSelect.getBinary();

  TupleResult RootFileDescriptorSelect(Impl, "SELECT get_root_file_descriptor($1)", P);
  uint32_t RootFileDescriptorID = RootFileDescriptorSelect.getBinary();
  Impl->FDCache[""] = RootFileDescriptorID;
}

Database::~Database() {
  PQfinish(Impl->Connection);
}

std::string Database::getSourceDirectory() const {
  return Impl->SourceDirectory;
}

uint32_t Database::getCompileCommandID() const {
  return Impl->CompileCommandID;
}

uint32_t Database::getPackageID() const {
  return Impl->PackageID;
}

uint32_t Database::getRootDeclID() const {
  return Impl->RootDeclID;
}
  
uint32_t Database::getFileDescriptorID(StringRef FullPath) {
  std::string AbsolutePath = getAbsolutePath(FullPath);
  char RealPath[PATH_MAX];
  if (realpath(AbsolutePath.c_str(), RealPath) == nullptr) {
    llvm_unreachable("Call to realpath failed");
  }
  StringRef ResolvedPath(RealPath);

  if (!ResolvedPath.startswith(getSourceDirectory())) {
    return 0;
  }

  if (ResolvedPath.contains("..")) {
    errs() << ResolvedPath << '\n';
    llvm_unreachable("Resolved path still contains '..'");
  }

  StringRef Path = ResolvedPath.substr(getSourceDirectory().size());

  return getFileDescriptorIDFromPath(Path);
}

uint32_t Database::getFileDescriptorIDFromPath(StringRef Path) {
  if (Impl->FDCache.count(Path)) {
    return Impl->FDCache[Path];
  }

  std::string Name;
  auto Index = Path.rfind('/');
  uint32_t ParentID;
  if (Index != StringRef::npos) {
    Name = Path.substr(Index + 1).str();
    ParentID = getFileDescriptorIDFromPath(Path.substr(0, Index));
  }
  else {
    Name = Path.str();
    ParentID = getFileDescriptorIDFromPath("");
  }

  Params P;
  P.addBinary(getPackageID());
  P.addBinary(ParentID);
  P.addText(Name.c_str());
  auto S = Path.str();
  P.addText(S.c_str());

  TupleResult FileDescriptorSelect(Impl, "SELECT get_file_descriptor($1, $2, $3, $4)", P);
  uint32_t FileDescriptorID = FileDescriptorSelect.getBinary();
  Impl->FDCache[Path] = FileDescriptorID;
  return FileDescriptorID;
}

std::string Database::getSource() {
  Params P;
  P.addBinary(getCompileCommandID());
  TupleResult CompileCommandSelect(Impl, "SELECT * FROM cpp_doc_compile_command WHERE id = $1", P);
  uint32_t FileID = CompileCommandSelect.getID("file_id");
  uint32_t PackageID = CompileCommandSelect.getID("package_id");

  P.clear();
  P.addBinary(FileID);
  TupleResult FileSelect(Impl, "SELECT * FROM cpp_doc_file_descriptor WHERE id = $1", P);

  std::stringstream ss;
  ss << getSourceDirectory() << FileSelect.getValue("path");
  return ss.str();
}

std::string Database::getDirectory() {
  Params P;

  P.addBinary(getCompileCommandID());
  TupleResult CompileCommandSelect(Impl, "SELECT * FROM cpp_doc_compile_command WHERE id = $1", P);
  uint32_t DirectoryID = CompileCommandSelect.getID("directory_id");
  uint32_t PackageID = CompileCommandSelect.getID("package_id");

  P.clear();
  P.addBinary(DirectoryID);
  TupleResult DirectorySelect(Impl, "SELECT * FROM cpp_doc_file_descriptor WHERE id = $1", P);

  std::stringstream ss;
  ss << getSourceDirectory() << DirectorySelect.getValue("path");
  return ss.str();
}

std::vector<std::string> Database::getCommands() {
  Params P;

  P.addBinary(getCompileCommandID());
  TupleResult CompileCommandSelect(Impl, "SELECT * FROM cpp_doc_compile_command WHERE id = $1", P);
  TupleResult CommandLineUpperSelect(Impl, "SELECT array_upper(command_line, 1) FROM cpp_doc_compile_command WHERE id = $1", P);

  std::vector<std::string> Commands;
  unsigned Upper = CommandLineUpperSelect.getBinary();
  for (unsigned i = 1; i <= Upper; ++i) {
    std::stringstream ss;
    ss << "SELECT command_line[" << i << "] FROM cpp_doc_compile_command WHERE id = $1";
    TupleResult ArgSelect(Impl, ss.str().c_str(), P);
    Commands.push_back(ArgSelect.getValue("command_line"));
  }
  return Commands;
}

ClangDatabase::ClangDatabase(Database &DB,
			     ASTContext &Ctx,
			     SourceManager &SM)
  : Impl(new ClangDatabaseImpl(DB, Ctx, SM)) {
}

ClangDatabase::~ClangDatabase() = default;

std::string ClangDatabase::getMangledName(const CXXMethodDecl *D) {
  std::string Str;
  raw_string_ostream StrOS(Str);
  Impl->Mangler->mangleCXXName(D, StrOS);
  return StrOS.str();
}

std::string ClangDatabase::getSignature(const FunctionDecl *Target,
					bool Qualified) {
  if (!Target)
    return "";
  std::string Signature;

  if (!isa<CXXConstructorDecl>(Target) && !isa<CXXDestructorDecl>(Target) &&
      !isa<CXXConversionDecl>(Target))
    Signature.append(Target->getReturnType().getAsString(Impl->PP)).append(" ");
  if (Qualified)
    Signature.append(Target->getQualifiedNameAsString());
  else
    Signature.append(Target->getNameAsString());
  Signature.append("(");

  for (int i = 0, paramsCount = Target->getNumParams(); i < paramsCount; ++i) {
    if (i)
      Signature.append(", ");
    Signature.append(Target->getParamDecl(i)->getType().getAsString(Impl->PP));
  }

  if (Target->isVariadic())
    Signature.append(", ...");
  Signature.append(")");

  const auto *TargetT =
    llvm::dyn_cast_or_null<clang::FunctionType>(Target->getType().getTypePtr());

  if (!TargetT || !isa<CXXMethodDecl>(Target))
    return Signature;

  if (TargetT->isConst())
    Signature.append(" const");
  if (TargetT->isVolatile())
    Signature.append(" volatile");
  if (TargetT->isRestrict())
    Signature.append(" restrict");

  if (const auto *TargetPT =
          dyn_cast_or_null<FunctionProtoType>(Target->getType().getTypePtr())) {
    switch (TargetPT->getRefQualifier()) {
    case RQ_LValue:
      Signature.append(" &");
      break;
    case RQ_RValue:
      Signature.append(" &&");
      break;
    default:
      break;
    }
  }

  return Signature;
}

std::unique_ptr<DatabaseImpl> &ClangDatabase::getDatabaseImpl() const {
  return Impl->DB.Impl;
}

uint32_t ClangDatabase::getPresumedLocIDPLoc(PresumedLoc PLoc) {
  if (!PLoc.isValid()) { // true for OpenCV 3.2.0
    return 0;
  }

  uint32_t FileID = Impl->DB.getFileDescriptorID(PLoc.getFilename());
  if (FileID == 0) {
    return 0;
  }

  Params P;
  P.addBinary(FileID);
  P.addBinary(PLoc.getLine());
  P.addBinary(PLoc.getColumn());

  TupleResult PresumedLocSelect(getDatabaseImpl(), "SELECT get_presumed_loc($1, $2, $3)", P);
  return PresumedLocSelect.getBinary();
}

uint32_t ClangDatabase::getDeclID(const Decl *D) {
  if (isa<TranslationUnitDecl>(D)) {
    return Impl->DB.getRootDeclID();
  }

  if (auto RD = dyn_cast<RecordDecl>(D)) {
    if (Impl->RecordIDCache.count(RD)) {
      return Impl->RecordIDCache[RD];
    }
  }
  else if (auto NSD = dyn_cast<NamespaceDecl>(D)) {
    if (Impl->NamespaceIDCache.count(NSD)) {
      return Impl->NamespaceIDCache[NSD];
    }
  }
  else if (auto MD = dyn_cast<CXXMethodDecl>(D)) {
    if (Impl->MethodIDCache.count(MD)) {
      return Impl->MethodIDCache[MD];
    }
  }
  // Note: Method is a subclass of Function, so it needs to come after Method
  else if (auto FD = dyn_cast<FunctionDecl>(D)) {
    if (Impl->FunctionIDCache.count(FD)) {
      return Impl->FunctionIDCache[FD];
    }
  }

  const DeclContext *DC = D->getDeclContext();

  if (isa<LinkageSpecDecl>(D)) {
    // If this is a linkage spec, ignore it by using the containing decl context
    return getDeclID(cast<Decl>(DC));
  }

  uint32_t ParentID = getDeclID(cast<Decl>(DC));
  std::string Name;
  std::string Path;

  if (auto FD = dyn_cast<FunctionDecl>(D)) {
    Name = getSignature(FD->getCanonicalDecl(), false);
    Path = getSignature(FD->getCanonicalDecl(), true);
  }
  else {
    Name = cast<NamedDecl>(D)->getNameAsString();
    Path = cast<NamedDecl>(D)->getQualifiedNameAsString();
  }

  uint32_t DeclID;

  Params P;
  P.addBinary(Impl->DB.getPackageID());
  P.addBinary(ParentID);
  P.addText(Name.c_str());
  P.addText(Path.c_str());
  if (auto MD = dyn_cast<CXXMethodDecl>(D)) {
    uint32_t PresumedLocID = 0;
    if (MD->isDefined()) {
      // It can be pure and defined if it's a comment
      // assert (!MD->isPure() && "This should never happen");
      MD = cast<CXXMethodDecl>(MD->getDefinition());
      PresumedLocID = getPresumedLocID(MD);
    }
    else if (MD->isPure()) {
      assert (!MD->isDefined() && "This should never happen");
      PresumedLocID = getPresumedLocID(MD);
    }

    if (PresumedLocID != 0) {
      P.addBinary(PresumedLocID);
      TupleResult Select(getDatabaseImpl(), "SELECT get_decl($1, $2, $3, $4, $5)", P);
      DeclID = Select.getBinary();
    }
    else {
      TupleResult Select(getDatabaseImpl(), "SELECT get_decl($1, $2, $3, $4)", P);
      DeclID = Select.getBinary();
    }
  }
  else if (auto FD = dyn_cast<FieldDecl>(D)) {
    uint32_t PresumedLocID = 0;
    PresumedLocID = getPresumedLocID(FD);
    if (PresumedLocID != 0) {
      P.addBinary(PresumedLocID);
      TupleResult Select(getDatabaseImpl(), "SELECT get_decl($1, $2, $3, $4, $5)", P);
      DeclID = Select.getBinary();
    }
    else {
      TupleResult Select(getDatabaseImpl(), "SELECT get_decl($1, $2, $3, $4)", P);
      DeclID = Select.getBinary();
    }
  }
  else {
    TupleResult Select(getDatabaseImpl(), "SELECT get_decl($1, $2, $3, $4)", P);
    DeclID = Select.getBinary();
  }

  P.clear();
  P.addBinary(DeclID);
  if (auto RD = dyn_cast<RecordDecl>(D)) {
    if (auto CRD = dyn_cast<CXXRecordDecl>(RD)) {
      CRD = CRD->getCanonicalDecl();

      if (!CRD->hasDefinition()) {
        return DeclID;
      }

      P.addBool(CRD->isAbstract());
      P.addBool(CRD->hasAnyDependentBases());
      TupleResult Select(getDatabaseImpl(), "SELECT get_record_decl($1, $2, $3)", P);
      // Only cache the record if it has a defintion, otherwise we'll miss
      // information
      Impl->RecordIDCache[RD] = DeclID;
    }
  }
  else if (auto NSD = dyn_cast<NamespaceDecl>(D)) {
    TupleResult Select(getDatabaseImpl(), "SELECT get_namespace_decl($1)", P);
    Impl->NamespaceIDCache[NSD] = DeclID;
  }
  else if (auto FD = dyn_cast<FieldDecl>(D)) {
    P.addBool(FD->isMutable());
    P.addBinary(FD->getAccess());
    TupleResult Select(getDatabaseImpl(), "SELECT get_field_decl($1, $2, $3)", P);
    Impl->FieldIDCache[FD] = DeclID;
  }
  else if (auto MD = dyn_cast<CXXMethodDecl>(D)) {
    std::string MangledName = getMangledName(MD);
    P.addText(MangledName.c_str());
    P.addBool(MD->isConst());
    P.addBool(MD->isPure());
    P.addBinary(MD->getAccess());
    TupleResult Select(getDatabaseImpl(), "SELECT get_method_decl($1, $2, $3, $4, $5)", P);
    Impl->MethodIDCache[MD] = DeclID;
  }
  // Note: Method is a subclass of Function, so it needs to come after Method
  else if (auto FD = dyn_cast<FunctionDecl>(D)) {
    TupleResult Select(getDatabaseImpl(), "SELECT get_function_decl($1)", P);
    Impl->FunctionIDCache[FD] = DeclID;
  }

  return DeclID;
}

uint32_t ClangDatabase::getPresumedLocID(const Decl *D) {
  PresumedLoc PLoc = Impl->SM.getPresumedLoc(D->getLocation());
  uint32_t PresumedLocID = getPresumedLocIDPLoc(PLoc);
  return PresumedLocID;
}

void ClangDatabase::insertMethod(const CXXMethodDecl *MD) {
  getDeclID(MD);
}

void ClangDatabase::insertPublicMethod(const CXXRecordDecl *RD, const CXXMethodDecl *MD) {
  Params P;
  P.addBinary(getDeclID(RD));
  P.addBinary(getDeclID(MD));
  TupleResult Select(getDatabaseImpl(), "SELECT get_public_view($1, $2)", P);
}

void ClangDatabase::insertPublicField(const CXXRecordDecl *RD, const FieldDecl *FD) {
  Params P;
  P.addBinary(getDeclID(RD));
  P.addBinary(getDeclID(FD));
  TupleResult Select(getDatabaseImpl(), "SELECT get_public_view($1, $2)", P);
}

void ClangDatabase::insertMethodCheck(const CXXMethodDecl *MD, MethodResultTuple Result) {
  uint32_t MethodDeclID = getDeclID(MD);

  Params P;
  P.addBinary(MethodDeclID);
  P.addBinary(static_cast<uint32_t>(Result.mutateResult));
  P.addBinary(static_cast<uint32_t>(Result.returnResult));

  TupleResult Select(getDatabaseImpl(), "SELECT get_clang_immutability_check_method($1, $2, $3)", P);
}

void ClangDatabase::insertFieldCheck(const FieldDecl *FD, bool isExplicit, bool isTransitive) {
    assert(FD);
  uint32_t FieldDeclID = getDeclID(FD);
  Params P;
  P.addBinary(FieldDeclID);
  if (isExplicit) {
    P.addText("true");
  }
  else {
    P.addText("false");
  }
  if (isTransitive) {
    P.addText("true");
  }
  else {
    P.addText("false");
  }
  TupleResult Select(getDatabaseImpl(), "SELECT get_clang_immutability_check_field($1, $2, $3)", P);
}

void ClangDatabase::insertMethodDependence(const CXXMethodDecl *Method, const CXXMethodDecl *Callee) {
  uint32_t MethodID = getDeclID(Method);
  uint32_t CalleeID = getDeclID(Callee);

  Params P;
  P.addBinary(MethodID);
  P.addBinary(CalleeID);

  TupleResult Select(getDatabaseImpl(), "SELECT get_method_dependence($1, $2)", P);
}

bool ClangDatabase::isSkippedMethod(const CXXMethodDecl *MD) {
  if (isa<CXXConstructorDecl>(MD)
      || isa<CXXConversionDecl>(MD)
      || isa<CXXDestructorDecl>(MD)) {
    return true;
  }

  if (MD->isCopyAssignmentOperator()
      || MD->isMoveAssignmentOperator()) {
    return true;
  }

  if (MD->isStatic()) {
    return true;
  }

  switch (MD->getTemplatedKind()) {
  case FunctionDecl::TemplatedKind::TK_NonTemplate:
  case FunctionDecl::TemplatedKind::TK_FunctionTemplate:
    break;
  default: // Specializations
    return true;
  }

  return false;
}

}
}

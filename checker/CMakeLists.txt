add_executable(const-checker
  ConstChecker.cpp
  Exprs.cpp
  Methods.cpp
  Types.cpp
  ValuesMaybeField.cpp
  ValuesMustLiteral.cpp
  VariableKinds.cpp

  Action.cpp
  Consumer.cpp
)
target_link_libraries(const-checker
  clangAnalysis
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  clangConstCheckerDatabase
  LLVM
  pq
)
install(TARGETS const-checker DESTINATION bin)

add_executable(const-rewriter
  ConstRewriter.cpp
)
target_link_libraries(const-rewriter
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
install(TARGETS const-rewriter DESTINATION bin)

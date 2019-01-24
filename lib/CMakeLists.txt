add_library(clangConstCheckerDatabase SHARED
  Database.cpp
)
install(TARGETS clangConstCheckerDatabase DESTINATION lib)

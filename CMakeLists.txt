cmake_minimum_required(VERSION 3.4.3)

project(CLANG_IMMUTABILITY_CHECK)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS} include)
add_definitions(${LLVM_DEFINITIONS})

add_subdirectory(checker)
add_subdirectory(lib)
add_subdirectory(rewriter)

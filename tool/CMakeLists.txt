add_executable(get_me)

target_sources(get_me PRIVATE src/main.cpp)

target_compile_options(get_me PRIVATE -fno-rtti)

target_include_directories(get_me SYSTEM PUBLIC ${CLANG_INCLUDE_DIRS})
message(STATUS "clang includes: " ${CLANG_INCLUDE_DIRS})

# copy clang headers relative to tool location to fix not finding headers

# FIXME: also fix for installing tool
set(CLANG_HEADER_INCLUDE_DIR
    "${LLVM_LIBRARY_DIR}/clang/${LLVM_VERSION}/include")
message(STATUS "CLANG_HEADER_INCLUDE_DIR: " ${CLANG_HEADER_INCLUDE_DIR})
file(COPY ${CLANG_HEADER_INCLUDE_DIR}
     DESTINATION ${CMAKE_BINARY_DIR}/lib/clang/${LLVM_VERSION})

target_link_libraries(get_me PUBLIC project_libraries)
target_link_libraries(get_me PUBLIC project_warnings)
target_link_libraries(get_me PUBLIC project_options)
target_link_libraries(
  get_me
  PRIVATE clangAST
          clangBasic
          clangDriver
          clangFrontend
          clangRewriteFrontend
          clangSerialization
          clangStaticAnalyzerFrontend
          clangTooling
          clangToolingSyntax)

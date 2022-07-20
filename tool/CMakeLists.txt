add_executable(get_me)

find_package(
  Boost
  COMPONENTS graph
  REQUIRED)
find_package(range-v3 REQUIRED)

target_sources(get_me PRIVATE src/main.cpp)

target_compile_options(get_me PRIVATE -fno-rtti)

target_include_directories(get_me PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

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
target_link_libraries(get_me PUBLIC Boost::graph)
target_link_libraries(get_me PUBLIC range-v3::range-v3)
target_link_libraries(
  get_me
  PRIVATE LLVMSupport
          clangAST
          clangBasic
          clangDriver
          clangFrontend
          clangRewriteFrontend
          clangSerialization
          clangStaticAnalyzerFrontend
          clangTooling
          clangToolingSyntax)

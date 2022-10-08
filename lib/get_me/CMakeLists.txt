find_package(
  Boost
  COMPONENTS graph
  REQUIRED)
find_package(range-v3 REQUIRED)

add_library(get_me_lib STATIC)
target_sources(
  get_me_lib
  PRIVATE formatting.cpp
          graph.cpp
          tooling.cpp
          tooling_filters.cpp
          type_set.cpp)

target_compile_options(get_me_lib PUBLIC -fno-rtti)
target_include_directories(get_me_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(get_me_lib PUBLIC project_libraries project_options
                                        project_warnings)
target_link_libraries(get_me_lib PUBLIC Boost::graph range-v3::range-v3)
target_link_libraries(
  get_me_lib
  PUBLIC LLVMSupport
         clangAST
         clangBasic
         clangFrontend
         clangRewriteFrontend
         clangSerialization
         clangTooling)

target_include_directories(get_me_lib SYSTEM PUBLIC ${CLANG_INCLUDE_DIRS})
message(STATUS "clang includes: " ${CLANG_INCLUDE_DIRS})

# copy clang headers relative to tool location to fix not finding headers

string(
  REPLACE "git"
          ""
          LLVM_VERSION
          ${LLVM_VERSION})

# FIXME: also fix for installing tool
set(CLANG_HEADER_INCLUDE_DIR
    "${LLVM_LIBRARY_DIR}/clang/${LLVM_VERSION}/include")
message(STATUS "CLANG_HEADER_INCLUDE_DIR: " ${CLANG_HEADER_INCLUDE_DIR})
file(COPY ${CLANG_HEADER_INCLUDE_DIR}
     DESTINATION ${CMAKE_BINARY_DIR}/lib/clang/${LLVM_VERSION})

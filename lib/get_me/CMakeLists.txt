add_library(get_me_lib STATIC)
target_sources(
  get_me_lib
  PRIVATE src/backwards_path_finding.cpp
          src/config.cpp
          src/formatting.cpp
          src/graph.cpp
          src/propagate_inheritance.cpp
          src/propagate_type_aliasing.cpp
          src/query.cpp
          src/query_all.cpp
          src/tooling.cpp
          src/tooling_filters.cpp
          src/transitions.cpp
          src/type_set.cpp)
target_sources(
  get_me_lib
  PUBLIC FILE_SET
         HEADERS
         TYPE
         HEADERS
         FILES
         include/get_me/backwards_path_finding.hpp
         include/get_me/config.hpp
         include/get_me/direct_type_dependency_propagation.hpp
         include/get_me/formatting.hpp
         include/get_me/graph.hpp
         include/get_me/indexed_set.hpp
         include/get_me/propagate_inheritance.hpp
         include/get_me/propagate_type_aliasing.hpp
         include/get_me/query.hpp
         include/get_me/query_all.hpp
         include/get_me/tooling.hpp
         include/get_me/tooling_filters.hpp
         include/get_me/transitions.hpp
         include/get_me/type_set.hpp)

target_compile_options(get_me_lib PUBLIC -fno-rtti)
target_include_directories(get_me_lib
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(get_me_lib PUBLIC project_libraries project_options)
target_link_libraries(get_me_lib PUBLIC get_me_support)
target_link_libraries(get_me_lib PUBLIC Boost::graph)
target_link_libraries(get_me_lib PUBLIC TBB::tbb)
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

install(TARGETS get_me_lib)
install(TARGETS get_me_lib FILE_SET HEADERS)

string(
  REPLACE "git"
          ""
          LLVM_VERSION
          ${LLVM_VERSION})

set(LLVM_VERSION_USED ${LLVM_VERSION})
if("${LLVM_VERSION_MAJOR}" GREATER_EQUAL 16)
  set(LLVM_VERSION_USED ${LLVM_VERSION_MAJOR})
endif()

# copy clang headers relative to tool location to fix not finding headers
set(CLANG_HEADER_INCLUDE_DIR
    "${LLVM_LIBRARY_DIR}/clang/${LLVM_VERSION_USED}/include")
message(STATUS "CLANG_HEADER_INCLUDE_DIR: " ${CLANG_HEADER_INCLUDE_DIR})
file(COPY ${CLANG_HEADER_INCLUDE_DIR}
     DESTINATION ${CMAKE_BINARY_DIR}/lib/clang/${LLVM_VERSION_USED})
install(DIRECTORY ${CLANG_HEADER_INCLUDE_DIR}
        DESTINATION lib/clang/${LLVM_VERSION_USED})

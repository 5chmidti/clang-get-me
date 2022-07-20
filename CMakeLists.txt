cmake_minimum_required(VERSION 3.18)
project(get_me)

include(cmake/StandardProjectSettings.cmake)
include(cmake/PreventInSourceBuilds.cmake)

add_library(project_options INTERFACE)

# enable cache system
include(cmake/Cache.cmake)

# Add linker configuration
include(cmake/Linker.cmake)
configure_linker(project_options)

# standard compiler warnings
include(cmake/CompilerWarnings.cmake)
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

# sanitizer options if supported by compiler
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)

# enable doxygen
include(cmake/Doxygen.cmake)
enable_doxygen()

# set cxx standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Clang REQUIRED 14)
find_package(LLVM REQUIRED 14)

include(cmake/Conan.cmake)
run_conan()

# project libraries
add_library(project_libraries INTERFACE)

target_link_libraries(project_libraries INTERFACE ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(project_libraries INTERFACE fmt::fmt spdlog::spdlog)

# allow for static analysis options
include(cmake/StaticAnalyzers.cmake)

add_subdirectory(tool)
add_subdirectory(test)

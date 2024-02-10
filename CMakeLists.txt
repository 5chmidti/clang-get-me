cmake_minimum_required(VERSION 3.23)
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
set_project_warnings(project_options)

# sanitizer options if supported by compiler
include(cmake/Sanitizers.cmake)
enable_sanitizers(project_options)
include(cmake/Coverage.cmake)
enable_coverage(project_options)
enable_coverage_targets()

include(cmake/Profiling.cmake)
enable_profiling(project_options)

# enable doxygen
include(cmake/Doxygen.cmake)
enable_doxygen()

# set cxx standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(Clang REQUIRED CONFIG)

include(cmake/ProjectDependencies.cmake)

# project libraries
add_library(project_libraries INTERFACE)

target_link_libraries(project_libraries INTERFACE ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(project_libraries INTERFACE fmt::fmt spdlog::spdlog
                                                  range-v3::range-v3)

# allow for static analysis options
include(cmake/StaticAnalyzers.cmake)

add_subdirectory(lib)
add_subdirectory(tool)
enable_testing()
add_subdirectory(test)
add_subdirectory(benchmark)

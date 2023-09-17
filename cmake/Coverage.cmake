function(enable_coverage project_name)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES
                                             ".*Clang")
    option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" OFF)
  endif()

  if(ENABLE_COVERAGE)
    if(NOT ENABLE_COVERAGE)
      return()
    endif()

    if(WIN32 AND ENABLE_COVERAGE)
      message("Cannot use coverage with WIN32")
      return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
      target_compile_options(
        ${project_name} INTERFACE -fprofile-instr-generate=prof_%p.profraw
                                  -fcoverage-mapping)
      target_link_options(
        ${project_name}
        INTERFACE
        -fprofile-instr-generate=prof_%p.profraw
        -fcoverage-mapping)

      find_package(Python COMPONENTS Interpreter)
      if(NOT Python_Interpreter_FOUND)
        message("Python interpreter not found (required for report-combine)")
        return()
      endif()

      find_program(LLVM_PROFDATA llvm-profdata)
      if(LLVM_PROFDATA MATCHES "-NOTFOUND")
        message(
          "llvm-profdata executable not found (required for report-combine)")
        return()
      endif()

      find_program(LLVM_COV llvm-cov)
      if(LLVM_COV MATCHES "-NOTFOUND")
        message("llvm-cov executable not found (required for coverage-collect)")
        return()
      endif()

      set(COVERAGE_OBJECTS
          ""
          PARENT_SCOPE)

      add_custom_target(
        coverage-collect
        COMMAND
          ${Python_EXECUTABLE}
          ${CMAKE_SOURCE_DIR}/cmake/export-clang-cov-profile.py --llvm-profdata
          ${LLVM_PROFDATA} --llvm-cov=${LLVM_COV}
          --binary-dir=${CMAKE_BINARY_DIR}
          --profiles-dir=${CMAKE_BINARY_DIR}/test
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

      add_custom_target(
        coverage-clean
        COMMAND sh -c "find ${CMAKE_BINARY_DIR} -name 'prof_*.profraw' -delete"
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    else()
      target_compile_options(${project_name} INTERFACE --coverage -O0 -g)
      target_link_options(${project_name} INTERFACE --coverage)

      find_program(LCOV lcov)
      if(LCOV MATCHES "-NOTFOUND")
        message("lcov executable not found (required for coverage-collect)")
        return()
      endif()

      add_custom_target(
        coverage-collect
        COMMAND ${LCOV} --directory ${CMAKE_BINARY_DIR} --capture --output-file
                ${CMAKE_BINARY_DIR}/coverage.info
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        BYPRODUCTS ${CMAKE_BINARY_DIR}/coverage.info)

      add_custom_target(
        coverage-clean
        COMMAND ${LCOV} --directory ${CMAKE_BINARY_DIR} --zerocounters
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

    endif()
  endif()

  set(COVERAGE_REPORT_AVAILABLE
      OFF
      PARENT_SCOPE)

endfunction(enable_coverage)

function(enable_coverage_targets)
  if(NOT ENABLE_COVERAGE)
    return()
  endif()

  if(WIN32 AND ENABLE_COVERAGE)
    message("Cannot use coverage with WIN32")
    return()
  endif()

  find_program(GENHTML genhtml)
  if(GENHTML MATCHES "-NOTFOUND")
    message("genhtml executable not found (required for coverage-report)")
    return()
  endif()

  find_package(Python COMPONENTS Interpreter)
  if(NOT Python_Interpreter_FOUND)
    message("Python interpreter not found (required for coverage-report)")
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
    add_custom_target(
      coverage-report
      COMMAND
        ${Python_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/clang-genhtml.py
        --genhtml ${GENHTML} --source-dir=${CMAKE_SOURCE_DIR}
        --binary-dir=${CMAKE_BINARY_DIR} --profiles-dir=${CMAKE_BINARY_DIR}/test
      DEPENDS coverage-collect
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  else()
    add_custom_target(
      coverage-report
      COMMAND
        ${GENHTML} --dark-mode --branch-coverage --demangle-cpp -o coverage
        --prefix ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/coverage.info
      DEPENDS coverage-collect
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  endif()

  set(COVERAGE_REPORT_AVAILABLE
      ON
      PARENT_SCOPE)

endfunction(enable_coverage_targets)

function(enable_sanitizers project_name)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES
                                             ".*Clang")
    option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" OFF)

    if(ENABLE_COVERAGE)
      target_compile_options(${project_name} INTERFACE --coverage -O0 -g)
      target_link_libraries(${project_name} INTERFACE --coverage)
    endif()

    set(SANITIZERS "")

    option(ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    if(ENABLE_SANITIZER_ADDRESS)
      list(APPEND SANITIZERS "address")
      option(ENABLE_SANITIZER_ADDRESS_RECOVER
             "Enable address sanitizer address revocery" OFF)
    endif()

    option(ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    if(ENABLE_SANITIZER_LEAK)
      list(APPEND SANITIZERS "leak")
    endif()

    option(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR
           "Enable undefined behavior sanitizer" OFF)
    if(ENABLE_SANITIZER_UNDEFINED_BEHAVIOR)
      list(APPEND SANITIZERS "undefined")
    endif()

    option(ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    if(ENABLE_SANITIZER_THREAD)
      if("address" IN_LIST SANITIZERS OR "leak" IN_LIST SANITIZERS)
        message(
          WARNING
            "Thread sanitizer does not work with Address and Leak sanitizer enabled"
        )
      else()
        list(APPEND SANITIZERS "thread")
      endif()
    endif()

    option(ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    if(ENABLE_SANITIZER_MEMORY AND CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
      message(
        WARNING
          "Memory sanitizer requires all the code (including libc++) to be MSan-instrumented otherwise it reports false positives"
      )
      if("address" IN_LIST SANITIZERS
         OR "thread" IN_LIST SANITIZERS
         OR "leak" IN_LIST SANITIZERS)
        message(
          WARNING
            "Memory sanitizer does not work with Address, Thread and Leak sanitizer enabled"
        )
      else()
        list(APPEND SANITIZERS "memory")
      endif()
    endif()

    list(
      JOIN
      SANITIZERS
      ","
      LIST_OF_SANITIZERS)

  endif()

  if(LIST_OF_SANITIZERS)
    if(NOT
       "${LIST_OF_SANITIZERS}"
       STREQUAL
       "")
      target_compile_options(${project_name}
                             INTERFACE -fsanitize=${LIST_OF_SANITIZERS})
      target_link_options(${project_name} INTERFACE
                          -fsanitize=${LIST_OF_SANITIZERS})
      if(ENABLE_SANITIZER_ADDRESS_RECOVER AND "address" IN_LIST SANITIZERS)
        target_compile_options(${project_name}
                               INTERFACE -fsanitize-recover=address)
        target_link_options(${project_name} INTERFACE
                            -fsanitize-recover=address)
      endif()
    endif()
  endif()

endfunction()

set(COVERAGE_REPORT_AVAILABLE OFF)
if(NOT WIN32 AND ENABLE_COVERAGE)
  find_program(LCOV lcov)
  if(NOT
     LCOV
     MATCHES
     "-NOTFOUND")
    add_custom_target(
      coverage-collect
      COMMAND
        ${LCOV} --directory ${CMAKE_BINARY_DIR} --capture --output-file
        ${CMAKE_BINARY_DIR}/coverage.info --exclude "/usr/include/*" --exclude
        "*.conan*"
      VERBATIM
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      BYPRODUCTS ${CMAKE_BINARY_DIR}/coverage.info)

    find_program(GENHTML genhtml)
    if(NOT
       GENHTML
       MATCHES
       "-NOTFOUND")
      add_custom_target(
        coverage-report
        COMMAND ${GENHTML} --demangle-cpp -o coverage
                ${CMAKE_BINARY_DIR}/coverage.info
        DEPENDS coverage-collect
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
      set(COVERAGE_REPORT_AVAILABLE ON)
    elseif()
      message("genhtml executable not found (required for coverage-report)")
    endif()
  elseif()
    message("lcov executable not found (required for coverage-collect)")
  endif()
endif()

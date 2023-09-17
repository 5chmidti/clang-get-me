# add custom check target, builds and tests all tests

# processor count detection and option
include(ProcessorCount)
ProcessorCount(proc_count)
set(CTEST_JOB_COUNT
    "${proc_count}"
    CACHE STRING "number of jobs that run the tests")

set(check_args -j${CTEST_JOB_COUNT} --output-on-failure ${CTEST_EXTRA_ARGS})

add_custom_target(
  check
  ${CMAKE_COMMAND}
  -E
  env
  $<$<BOOL:${ENABLE_SANITIZER_ADDRESS}>:ASAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/sanitizer_files/asan.supp,fast_unwind_on_malloc=0,symbolize=1,allow_user_poisoning=false>
  $<$<BOOL:${ENABLE_SANITIZER_LEAK}>:LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/sanitizer_files/lsan.supp>
  ${CMAKE_CTEST_COMMAND}
  ${check_args}
  USES_TERMINAL)

if(${COVERAGE_REPORT_AVAILABLE})
  add_custom_target(
    check-coverage
    DEPENDS check
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
            coverage-report)

  if(ENABLE_COVERAGE)
    add_dependencies(check coverage-clean)
  endif()
endif()

add_subdirectory(unittests)

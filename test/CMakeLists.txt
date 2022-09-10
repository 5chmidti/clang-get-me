# add_custom_target( check lit -a --path=${CMAKE_BINARY_DIR}/bin
# ${CMAKE_SOURCE_DIR}/test DEPENDS get_me)

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
  COMMAND ${CMAKE_CTEST_COMMAND} ${check_args}
  USES_TERMINAL)

add_subdirectory(unittests)

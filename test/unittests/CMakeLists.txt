find_package(GTest REQUIRED)

add_library(get_me_test_lib STATIC)
target_sources(get_me_test_lib PRIVATE get_me_tests.cpp)
target_link_libraries(get_me_test_lib PUBLIC get_me_lib support)
target_link_libraries(get_me_test_lib PUBLIC GTest::GTest)

function(add_get_me_test name)
  cmake_parse_arguments(
    TEST
    ""
    ""
    "SOURCES"
    ${ARGN})

  add_executable(test_${name})
  target_sources(test_${name} PRIVATE ${TEST_SOURCES})
  target_link_libraries(test_${name} PRIVATE get_me_test_lib)
  add_test(NAME ${name} COMMAND test_${name})
  add_dependencies(check test_${name})
endfunction()

add_get_me_test(basic_tests SOURCES basic_tests.cpp)
add_get_me_test(type_alias_tests SOURCES type_alias_tests.cpp)
add_get_me_test(inheritance_tests SOURCES inheritance_tests.cpp)

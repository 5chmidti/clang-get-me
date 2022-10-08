add_library(support STATIC)
target_sources(support PRIVATE testcase_generation.cpp)
target_link_libraries(support PUBLIC get_me_lib)
target_include_directories(support PUBLIC ${CMAKE_SOURCE_DIR}/include)

add_library(get_me_support STATIC)
target_sources(get_me_support PRIVATE src/testcase_generation.cpp)
target_link_libraries(get_me_support PUBLIC project_libraries project_options)
target_include_directories(get_me_support
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

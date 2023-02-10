add_executable(get-me)
target_sources(get-me PRIVATE get_me.cpp)
target_include_directories(get-me PRIVATE include)
target_link_libraries(get-me PRIVATE get_me_lib get_me_tui)

install(TARGETS get-me DESTINATION bin)

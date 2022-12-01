add_executable(get_me)
target_sources(get_me PRIVATE src/main.cpp)
target_link_libraries(get_me PRIVATE get_me_lib)

install(TARGETS get_me DESTINATION bin)

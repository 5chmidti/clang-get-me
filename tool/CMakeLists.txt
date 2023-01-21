find_package(ftxui REQUIRED)
find_package(rapidfuzz REQUIRED)
find_package(ctre REQUIRED)

add_library(get_me_tui STATIC)
target_sources(
  get_me_tui
  PRIVATE src/tui.cpp
          src/tui/components/query.cpp
          src/tui/components/size_t_input.cpp
          src/tui/components/paths_menu.cpp
          src/tui/components/config_editor.cpp)
target_include_directories(get_me_tui PRIVATE include)
target_link_libraries(get_me_tui PUBLIC get_me_lib)
target_link_libraries(get_me_tui PRIVATE ftxui::ftxui rapidfuzz::rapidfuzz
                                         ctre::ctre)

add_executable(get_me)
target_sources(get_me PRIVATE src/main.cpp)
target_include_directories(get_me PRIVATE include)
target_link_libraries(get_me PRIVATE get_me_lib get_me_tui)

install(TARGETS get_me DESTINATION bin)

add_library(get_me_tui STATIC)
target_sources(
  get_me_tui
  PRIVATE src/tui.cpp
          src/components/query.cpp
          src/components/boolean_toogle.cpp
          src/components/size_t_input.cpp
          src/components/int64_input.cpp
          src/components/paths_menu.cpp
          src/components/config_editor.cpp)
target_include_directories(get_me_tui
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(get_me_tui PUBLIC get_me_lib)
target_link_libraries(get_me_tui PRIVATE ftxui::ftxui rapidfuzz::rapidfuzz
                                         ctre::ctre)

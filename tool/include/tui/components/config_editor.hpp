#ifndef get_me_tool_include_tui_components_config_components_hpp
#define get_me_tool_include_tui_components_config_components_hpp

#include <cstddef>

#include <ftxui/component/component_base.hpp>

#include "get_me/config.hpp"

[[nodiscard]] ftxui::Component configEntry(bool *Flag);

[[nodiscard]] ftxui::Component configEntry(std::size_t *Value);

[[nodiscard]] ftxui::Component buildConfigComponent(ftxui::Ref<Config> Conf);

#endif

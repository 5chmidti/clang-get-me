#ifndef get_me_tool_include_tui_components_config_components_hpp
#define get_me_tool_include_tui_components_config_components_hpp

#include <ftxui/component/component_base.hpp>

#include "get_me/config.hpp"

[[nodiscard]] ftxui::Component buildConfigComponent(ftxui::Ref<Config> Conf);

#endif

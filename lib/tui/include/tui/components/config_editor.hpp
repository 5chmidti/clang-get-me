#ifndef get_me_tool_include_tui_components_config_components_hpp
#define get_me_tool_include_tui_components_config_components_hpp

#include <memory>

#include <ftxui/component/component_base.hpp>
#include <ftxui/util/ref.hpp>

#include "get_me/config.hpp"

[[nodiscard]] ftxui::Component
buildConfigComponent(ftxui::Ref<std::shared_ptr<Config>> Conf);

#endif

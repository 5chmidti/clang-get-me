#ifndef get_me_tool_include_tui_components_paths_component_hpp
#define get_me_tool_include_tui_components_paths_component_hpp

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>

[[nodiscard]] ftxui::Component
buildPathsComponent(std::vector<std::string> *Paths);

#endif

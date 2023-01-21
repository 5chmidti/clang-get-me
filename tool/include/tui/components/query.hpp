#ifndef get_me_tool_include_tool_tui_components_query_hpp
#define get_me_tool_include_tool_tui_components_query_hpp

#include <functional>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

[[nodiscard]] ftxui::Component
buildQueryComponent(std::string *QueriedName,
                    const std::vector<std::string> *Entries,
                    std::function<void()> Callback);

#endif

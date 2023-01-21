#ifndef get_me_tool_include_tui_components_size_t_input_component_hpp
#define get_me_tool_include_tui_components_size_t_input_component_hpp

#include <fmt/format.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>

void parser(std::size_t &Value, std::string_view Str);

class SizeTInputComponent : public ftxui::ComponentBase {
public:
  explicit SizeTInputComponent(std::size_t *const Val)
      : ValueStr_{fmt::format("{}", *Val)},
        Component_{ftxui::Input(&ValueStr_, &ValueStr_,
                                ftxui::InputOption{.on_change = [this, Val] {
                                  parser(*Val, ValueStr_);
                                }})} {
    Add(Component_);
  }

private:
  std::string ValueStr_{};
  ftxui::Component Component_;
};

#endif

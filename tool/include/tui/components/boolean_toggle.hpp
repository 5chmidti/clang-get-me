#ifndef get_me_tool_include_tui_components_boolean_toggle_component_hpp
#define get_me_tool_include_tui_components_boolean_toggle_component_hpp

#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

class BooleanToggle : public ftxui::ComponentBase {
public:
  explicit BooleanToggle(bool *const Flag)
      : Selector_{static_cast<int>(*Flag)} {
    Add(Menu(&OffOn, &Selector_, [this, Flag] {
      auto Option = ftxui::MenuOption::Toggle();
      Option.on_change = [this, Flag]() mutable { *Flag = Selector_ == 1; };
      Option.focused_entry = Selector_;
      return Option;
    }()));
  }

private:
  static inline const std::vector<std::string> OffOn{"Off", "On"};

  int Selector_{0};
};

#endif

#include <string>
#include <vector>

#include "tui/components/boolean_toggle.hpp"

namespace {
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
} // namespace

ftxui::Component configEntry(bool *const Flag) {
  return ftxui::Make<BooleanToggle>(Flag);
}

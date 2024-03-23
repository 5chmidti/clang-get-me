#include "tui/components/size_t_input.hpp"

#include <cstddef>
#include <string>
#include <string_view>

#include <ctre.hpp> // IWYU pragma: keep
#include <fmt/core.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

namespace {
void parser(std::size_t &Value, std::string_view Str) {
  if (const auto Mat = ctre::match<"\\d+">(Str); Mat) {
    Value = Mat.get<0>().to_number<std::size_t>();
  }
}

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
  std::string ValueStr_;
  ftxui::Component Component_;
};

} // namespace

ftxui::Component configEntry(std::size_t *const Value) {
  return ftxui::Make<SizeTInputComponent>(Value);
}

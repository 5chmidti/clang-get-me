#include "tui/components/int64_input.hpp"

#include <cstdint>
#include <string>
#include <string_view>

#include <ctre.hpp>
#include <fmt/core.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>

namespace {
void parser(std::int64_t &Value, std::string_view Str) {
  if (const auto Mat = ctre::match<"\\d+">(Str); Mat) {
    Value = Mat.get<0>().to_number<std::int64_t>();
  }
}

class Int64InputComponent : public ftxui::ComponentBase {
public:
  explicit Int64InputComponent(std::int64_t *const Val)
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

} // namespace

ftxui::Component configEntry(std::int64_t *const Value) {
  return ftxui::Make<Int64InputComponent>(Value);
}

#ifndef get_me_lib_tui_include_tui_components_enum_selection_hpp
#define get_me_lib_tui_include_tui_components_enum_selection_hpp

#include <cstddef>
#include <string>
#include <type_traits>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <range/v3/functional/compose.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

#include "support/enum_mappings.hpp"
#include "support/ranges/functional.hpp"

namespace detail {
template <typename T>
  requires std::is_enum_v<T> && std::same_as<std::underlying_type_t<T>, int>
class EnumComponent : public ftxui::ComponentBase {
public:
  explicit EnumComponent(T *const Val)
      : Val_{Val},
        Component_{ftxui::Radiobox(
            &OptionsStrings_, &Selection_,
            ftxui::RadioboxOption{.on_change = [this]() {
              *Val_ = Value(Mappings[static_cast<size_t>(Selection_)]);
            }})} {
    Add(Component_);
  }

private:
  static inline const auto Mappings = enumeration<T>();
  std::vector<std::string> OptionsStrings_{
      Mappings |
      ranges::views::transform(
          ranges::compose(Construct<std::string>, Element<0>)) |
      ranges::to_vector};

  int Selection_{0};
  T *Val_{};

  ftxui::Component Component_;
};
} // namespace detail

template <typename T>
  requires std::is_enum_v<T>
[[nodiscard]] ftxui::Component configEntry(T *const Val) {
  return ftxui::Make<detail::EnumComponent<T>>(Val);
}

#endif

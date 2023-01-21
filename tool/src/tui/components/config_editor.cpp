#include "tui/components/config_editor.hpp"

#include <fmt/format.h>
#include <ftxui/component/component.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/transform.hpp>

#include "tui/components/boolean_toggle.hpp"
#include "tui/components/size_t_input.hpp"

ftxui::Component configEntry(bool *const Flag) {
  return ftxui::Make<BooleanToggle>(Flag);
}

ftxui::Component configEntry(std::size_t *const Value) {
  return ftxui::Make<SizeTInputComponent>(Value);
}

ftxui::Component buildConfigComponent(ftxui::Ref<Config> Conf) {
  const auto [BooleanMapping, SizeTMapping] = Config::getConfigMapping();

  const auto CreateEntry =
      [&Conf]<typename ValueType>(
          const Config::MappingType<ValueType> &MappingValue) {
        const auto Flag1 =
            configEntry(&std::invoke(std::get<1>(MappingValue), Conf));
        return Renderer(Flag1,
                        [Name = std::string{std::get<0>(MappingValue)}, Flag1] {
                          return hbox(ftxui::text(fmt::format("{:35}: ", Name)),
                                      Flag1->Render());
                        });
      };

  const auto ConfigElements = ftxui::Container::Vertical(
      ranges::views::concat(
          BooleanMapping | ranges::views::transform(CreateEntry),
          SizeTMapping | ranges::views::transform(CreateEntry)) |
      ranges::to_vector);
  return Renderer(ConfigElements, [ConfigElements] {
    return ftxui::vbox({
        ftxui::text("Config:"),
        ftxui::text(""),
        ConfigElements->Render(),
    });
  });
}

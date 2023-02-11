#include "tui/components/config_editor.hpp"

#include <memory>

#include <fmt/format.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/util/ref.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/config.hpp"
#include "tui/components/boolean_toggle.hpp"
#include "tui/components/size_t_input.hpp"

ftxui::Component buildConfigComponent(ftxui::Ref<Config> Conf) {
  const auto [BooleanMapping, SizeTMapping] = Config::getConfigMapping();

  const auto CreateEntry =
      [&Conf]<typename ValueType>(
          const Config::MappingType<ValueType> &MappingValue) {
        const auto &[Name, MemberAddress] = MappingValue;
        const auto Flag1 = configEntry(&std::invoke(MemberAddress, Conf));
        return Renderer(Flag1, [Name, Flag1] {
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

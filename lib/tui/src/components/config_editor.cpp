#include "tui/components/config_editor.hpp"

#include <memory>

#include <fmt/format.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/util/ref.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/utility/tuple_algorithm.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/empty.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/config.hpp"
#include "tui/components/boolean_toggle.hpp"
#include "tui/components/enum_seletion.hpp"
#include "tui/components/int64_input.hpp"
#include "tui/components/size_t_input.hpp"

ftxui::Component
buildConfigComponent(ftxui::Ref<std::shared_ptr<Config>> Conf) {
  const auto ToEntry = [&Conf]<typename ValueType>(
                           const Config::MappingType<ValueType> &MappingValue) {
    const auto &[Name, MemberAddress] = MappingValue;
    const auto Flag1 = configEntry(&std::invoke(MemberAddress, *Conf));
    return Renderer(Flag1, [Name, Flag1] {
      return hbox(ftxui::text(fmt::format("{:35}: ", Name)), Flag1->Render());
    });
  };

  const auto TransformToEntriesAndConcat = [&ToEntry](const auto Range,
                                                      auto Mappings) {
    return ranges::views::concat(Range,
                                 Mappings | ranges::views::transform(ToEntry));
  };

  const auto ConfigElements = ftxui::Container::Vertical(
      ranges::tuple_foldl(Config::getConfigMapping(),
                          ranges::views::empty<ftxui::Component>,
                          TransformToEntriesAndConcat) |
      ranges::to_vector);
  return Renderer(ConfigElements, [ConfigElements] {
    return ftxui::vbox({
        ftxui::text("Config:"),
        ftxui::text(""),
        ConfigElements->Render(),
    });
  });
}

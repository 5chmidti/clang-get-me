#ifndef get_me_include_support_testcase_generation_hpp
#define get_me_include_support_testcase_generation_hpp

#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

template <typename Generator>
[[nodiscard]] auto generateFromTemplate(std::string QueriedType,
                                        std::string InitializerCode,
                                        Generator &&CodeTemplateGenerator)
  requires std::is_invocable_r_v<std::string, Generator, size_t>
{
  return
      [QueriedType = std::move(QueriedType),
       InitializerCode = std::move(InitializerCode),
       CodeTemplateGenerator = std::forward<Generator>(CodeTemplateGenerator)](
          const size_t NumRepetitions) -> std::pair<std::string, std::string> {
        return {
            QueriedType,
            fmt::format(
                "{}\n{}", InitializerCode,
                fmt::join(ranges::views::iota(size_t{0U}, NumRepetitions) |
                              ranges::views::transform(CodeTemplateGenerator),
                          "\n"))};
      };
}

inline const auto GenerateStraightPath =
    generateFromTemplate("A0", "struct A0;", [](const size_t Iter) {
      return fmt::format("struct A{0}; A{1} getA{1}(A{0});", Iter + 1, Iter);
    });

inline const auto GenerateForkingPath =
    generateFromTemplate("A0", "struct A0;", [](const size_t Iter) {
      return fmt::format("struct A{0}; struct A{1}; A{2} getA{2}(A{0}, A{1});",
                         (Iter + 1) * 2 - 1, (Iter + 1) * 2, Iter);
    });

#endif

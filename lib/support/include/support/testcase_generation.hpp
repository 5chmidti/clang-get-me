#ifndef get_me_lib_support_include_support_testcase_generation_hpp
#define get_me_lib_support_include_support_testcase_generation_hpp

#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/indices.hpp>
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
        return {QueriedType,
                fmt::format("{}\n{}", InitializerCode,
                            fmt::join(ranges::views::indices(NumRepetitions) |
                                          ranges::views::transform(
                                              CodeTemplateGenerator),
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

inline const auto GenerateMultiForkingPath =
    generateFromTemplate("A0", "struct A0 {};", [](const size_t Iter) {
      return fmt::format(
          R"(
struct A{1};
struct B{0};
struct C{0};
struct D{0};
struct E{0};

A{0} getA{0}(B{0});
A{0} getA{0}(C{0});
B{0} getB{0}(D{0});
C{0} getC{0}(D{0});
C{0} getC{0}(E{0});
D{0} getD{0}(A{1});
E{0} getE{0}();
    )",
          Iter, Iter + 1);
    });

#endif

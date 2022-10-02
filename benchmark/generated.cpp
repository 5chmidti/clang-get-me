#include <string_view>

#include <benchmark/benchmark.h>
#include <boost/algorithm/string/replace.hpp>
#include <fmt/ranges.h>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me_benchmarks.hpp"

[[nodiscard]] static std::pair<std::string, std::string>
generateStraightPath(const size_t Length) {
  ranges::views::iota(static_cast<size_t>(0U), Length);
  return {
      "A0",
      fmt::format("struct A0{{}};\n{}",
                  fmt::join(ranges::views::iota(size_t{1U}, Length) |
                                ranges::views::transform([](const size_t Iter) {
                                  return fmt::format(
                                      "struct A{0} {{}}; A{1} getA{1}(A{0});",
                                      Iter, Iter - 1);
                                }),
                            "\n"))};
}

[[nodiscard]] static std::pair<std::string, std::string>
generateForkingPath(const size_t NumStructs) {
  ranges::views::iota(static_cast<size_t>(0U), NumStructs);
  return {
      "A0",
      fmt::format("struct A0{{}};\n{}",
                  fmt::join(ranges::views::iota(size_t{0U}, NumStructs / 2) |
                                ranges::views::transform([](const size_t Iter) {
                                  return fmt::format(
                                      "struct A{0} {{}}; struct A{1} {{}}; "
                                      "A{2} getA{2}(A{0}, A{1});",
                                      (Iter + 1) * 2 - 1, (Iter + 1) * 2, Iter);
                                }),
                            "\n"))};
}

// NOLINTNEXTLINE
GENERATE_GENERATED_BENCHMARKS(
    straightPath,
    generateStraightPath, ->Range(1, size_t{1U} << size_t{12U})->Complexity());

// NOLINTNEXTLINE
GENERATE_GENERATED_BENCHMARKS(
    forkingPath, generateForkingPath, ->DenseRange(1, 10, 2)->Complexity());

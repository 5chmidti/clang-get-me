#include <string_view>

#include <benchmark/benchmark.h>
#include <fmt/ranges.h>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me_benchmarks.hpp"

[[nodiscard]] inline std::string generateCode() { return ""; }
[[nodiscard]] inline std::pair<std::string, std::string>
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

template <typename T>
void BM_generator(benchmark::State &State, T &&Generator) {
  const auto [QueriedTypeAsString, Code] =
      std::forward<T>(Generator)(State.range(0));
  std::unique_ptr<clang::ASTUnit> Ast =
      clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});
  setupCounters(State, *Ast, QueriedTypeAsString);
  const auto Config = getConfig();
  for (auto _ : State) {
    TransitionCollector TypeSetTransitionData{};
    auto Consumer = GetMe{Config, TypeSetTransitionData, Ast->getSema()};
    Consumer.HandleTranslationUnit(Ast->getASTContext());
    benchmark::DoNotOptimize(TypeSetTransitionData.begin());
    const auto [Graph, Data] =
        createGraph(TypeSetTransitionData, QueriedTypeAsString, Config);
    benchmark::DoNotOptimize(Data.VertexData.data());
    benchmark::DoNotOptimize(Data.Edges.data());
    benchmark::DoNotOptimize(Data.EdgeIndices.data());
    benchmark::DoNotOptimize(Data.EdgeWeights.data());
    const auto SourceVertex =
        getSourceVertexMatchingQueriedType(Data, QueriedTypeAsString);
    if (!SourceVertex) [[unlikely]] {
      spdlog::error("QueriedType not found");
      return;
    }
    const auto FoundPaths = pathTraversal(Graph, Data, Config, *SourceVertex);
    benchmark::DoNotOptimize(FoundPaths.data());
    benchmark::ClobberMemory();
  }
  State.SetComplexityN(State.range(0));
}
// NOLINTNEXTLINE
BENCHMARK_CAPTURE(BM_generator, straightPath, generateStraightPath)
    ->Range(1, size_t{1U} << size_t{12U})
    ->Complexity();

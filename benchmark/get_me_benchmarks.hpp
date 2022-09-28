#ifndef get_me_benchmark_get_me_benchmarks_hpp
#define get_me_benchmark_get_me_benchmarks_hpp
#include <string_view>

#include <benchmark/benchmark.h>
#include <boost/hana.hpp>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <spdlog/spdlog.h>

#include "get_me/graph.hpp"
#include "get_me/tooling.hpp"

constexpr Config getConfig() {
  return Config{
      .EnableFilterOverloads = false,
      .EnablePropagateInheritance = true,
      .EnablePropagateTypeAlias = true,
      .EnableTruncateArithmetic = false,
      .EnableFilterStd = false,
      .MaxGraphDepth = std::nullopt,
      .MaxPathLength = std::nullopt,
      .MinPathCount = std::nullopt,
      .MaxPathCount = std::nullopt,
  };
}

inline void setupCounters(benchmark::State &State, clang::ASTUnit &Ast,
                          const std::string &QueriedTypeAsString) {
  TransitionCollector TypeSetTransitionData{};
  auto Consumer = GetMe{getConfig(), TypeSetTransitionData, Ast.getSema()};
  Consumer.HandleTranslationUnit(Ast.getASTContext());
  State.counters["transitions"] =
      static_cast<double>(TypeSetTransitionData.size());
  const auto [Graph, Data] =
      createGraph(TypeSetTransitionData, QueriedTypeAsString, getConfig());
  State.counters["vertices"] = static_cast<double>(Data.VertexData.size());
  State.counters["edges"] = static_cast<double>(Data.Edges.size());
  const auto SourceVertex =
      getSourceVertexMatchingQueriedType(Data, QueriedTypeAsString);
  if (!SourceVertex) {
    spdlog::error("QueriedType not found");
    return;
  }
  const auto FoundPaths =
      pathTraversal(Graph, Data, getConfig(), *SourceVertex);
  State.counters["paths"] = static_cast<double>(FoundPaths.size());
}

template <boost::hana::string Code, boost::hana::string QueriedType>
class GetMeFixture : public benchmark::Fixture {
public:
  void SetUp(::benchmark::State &State) override {
    setupCounters(State, getAst(), std::string{getQueriedType()});
  }

protected:
  [[nodiscard]] std::string_view getQueriedType() const {
    return QueriedType.c_str();
  }
  [[nodiscard]] const clang::ASTUnit &getAst() const { return *Ast; }
  [[nodiscard]] clang::ASTUnit &getAst() { return *Ast; }

private:
  Config Conf = getDefaultConfig();
  std::unique_ptr<clang::ASTUnit> Ast =
      clang::tooling::buildASTFromCodeWithArgs(Code.c_str(), {"-std=c++20"});
};

#define GENERATE_BENCHMARKS(Name, Code, QueriedType)                           \
  using Name =                                                                 \
      GetMeFixture<BOOST_HANA_STRING(Code), BOOST_HANA_STRING(QueriedType)>;   \
  BENCHMARK_DEFINE_F(Name, full)                                               \
  (benchmark::State & State) {                                                 \
    const auto QueriedTypeAsString = std::string{getQueriedType()};            \
    const auto Config = getConfig();                                           \
    for (auto _ : State) {                                                     \
      TransitionCollector TypeSetTransitionData{};                             \
      auto Consumer =                                                          \
          GetMe{Config, TypeSetTransitionData, getAst().getSema()};            \
      Consumer.HandleTranslationUnit(getAst().getASTContext());                \
      benchmark::DoNotOptimize(TypeSetTransitionData.begin());                 \
      const auto [Graph, Data] =                                               \
          createGraph(TypeSetTransitionData, QueriedTypeAsString, Config);     \
      benchmark::DoNotOptimize(Data.VertexData.data());                        \
      benchmark::DoNotOptimize(Data.Edges.data());                             \
      benchmark::DoNotOptimize(Data.EdgeIndices.data());                       \
      benchmark::DoNotOptimize(Data.EdgeWeights.data());                       \
      const auto SourceVertex =                                                \
          getSourceVertexMatchingQueriedType(Data, QueriedTypeAsString);       \
      if (!SourceVertex) [[unlikely]] {                                        \
        spdlog::error("QueriedType not found");                                \
        return;                                                                \
      }                                                                        \
      const auto FoundPaths =                                                  \
          pathTraversal(Graph, Data, Config, *SourceVertex);                   \
      benchmark::DoNotOptimize(FoundPaths.data());                             \
      benchmark::ClobberMemory();                                              \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, full);                                            \
  BENCHMARK_DEFINE_F(Name, transitions)                                        \
  (benchmark::State & State) {                                                 \
    const auto Config = getConfig();                                           \
    for (auto _ : State) {                                                     \
      TransitionCollector TypeSetTransitionData{};                             \
      auto Consumer =                                                          \
          GetMe{Config, TypeSetTransitionData, getAst().getSema()};            \
      Consumer.HandleTranslationUnit(getAst().getASTContext());                \
      benchmark::DoNotOptimize(TypeSetTransitionData.begin());                 \
      benchmark::ClobberMemory();                                              \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, transitions);                                     \
  BENCHMARK_DEFINE_F(Name, graph)                                              \
  (benchmark::State & State) {                                                 \
    const auto QueriedTypeAsString = std::string{getQueriedType()};            \
    const auto Config = getConfig();                                           \
    TransitionCollector TypeSetTransitionData{};                               \
    auto Consumer = GetMe{Config, TypeSetTransitionData, getAst().getSema()};  \
    Consumer.HandleTranslationUnit(getAst().getASTContext());                  \
    benchmark::DoNotOptimize(TypeSetTransitionData.begin());                   \
    for (auto _ : State) {                                                     \
      const auto [Graph, Data] =                                               \
          createGraph(TypeSetTransitionData, QueriedTypeAsString, Config);     \
      benchmark::DoNotOptimize(Graph);                                         \
      benchmark::DoNotOptimize(Data.VertexData.data());                        \
      benchmark::DoNotOptimize(Data.Edges.data());                             \
      benchmark::DoNotOptimize(Data.EdgeIndices.data());                       \
      benchmark::DoNotOptimize(Data.EdgeWeights.data());                       \
      benchmark::ClobberMemory();                                              \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, graph);                                           \
  BENCHMARK_DEFINE_F(Name, pathTraversal)                                      \
  (benchmark::State & State) {                                                 \
    const auto QueriedTypeAsString = std::string{getQueriedType()};            \
    const auto Config = getConfig();                                           \
    TransitionCollector TypeSetTransitionData{};                               \
    auto Consumer = GetMe{Config, TypeSetTransitionData, getAst().getSema()};  \
    Consumer.HandleTranslationUnit(getAst().getASTContext());                  \
    benchmark::DoNotOptimize(TypeSetTransitionData.begin());                   \
    const auto [Graph, Data] =                                                 \
        createGraph(TypeSetTransitionData, QueriedTypeAsString, Config);       \
    benchmark::DoNotOptimize(Graph);                                           \
    benchmark::DoNotOptimize(Data.VertexData.data());                          \
    benchmark::DoNotOptimize(Data.Edges.data());                               \
    benchmark::DoNotOptimize(Data.EdgeIndices.data());                         \
    benchmark::DoNotOptimize(Data.EdgeWeights.data());                         \
    const auto SourceVertex =                                                  \
        getSourceVertexMatchingQueriedType(Data, QueriedTypeAsString);         \
    if (!SourceVertex) [[unlikely]] {                                          \
      spdlog::error("QueriedType not found");                                  \
      return;                                                                  \
    }                                                                          \
    for (auto _ : State) {                                                     \
      const auto FoundPaths =                                                  \
          pathTraversal(Graph, Data, Config, *SourceVertex);                   \
      benchmark::DoNotOptimize(FoundPaths.data());                             \
      benchmark::ClobberMemory();                                              \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, pathTraversal);

#endif

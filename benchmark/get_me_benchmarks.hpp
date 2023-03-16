#ifndef get_me_benchmark_get_me_benchmarks_hpp
#define get_me_benchmark_get_me_benchmarks_hpp

#include <memory>
#include <string_view>

#include <benchmark/benchmark.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <spdlog/spdlog.h>

#include "get_me/graph.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/query.hpp"
#include "get_me/tooling.hpp"

inline void setupCounters(benchmark::State &State, clang::ASTUnit &Ast,
                          const std::string &QueriedTypeAsString) {
  const auto Conf = Config{};
  auto Transitions = std::make_shared<TransitionCollector>();
  collectTransitions(Transitions, Ast, Conf);
  State.counters["transitions"] = static_cast<double>(Transitions->size());
  const auto Query = getQueriedTypeForInput(*Transitions, QueriedTypeAsString);
  const auto Data = createGraph(*Transitions, Query, Conf);
  State.counters["vertices"] = static_cast<double>(Data.VertexData.size());
  State.counters["edges"] = static_cast<double>(Data.Edges.size());
  const auto SourceVertex = getSourceVertexMatchingQueriedType(Data, Query);
  const auto FoundPaths = pathTraversal(Data, Conf, SourceVertex);
  State.counters["paths"] = static_cast<double>(FoundPaths.size());
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SETUP_BENCHMARK(Code, QueriedType)                                     \
  const auto QueriedTypeAsString = std::string{QueriedType};                   \
  const auto Conf = Config{};                                                  \
  std::unique_ptr<clang::ASTUnit> Ast =                                        \
      clang::tooling::buildASTFromCodeWithArgs(Code, {"-std=c++20"});          \
  setupCounters(State, *Ast, QueriedTypeAsString);

#define BENCHMARK_TRANSITIONS                                                  \
  auto Transitions = std::make_shared<TransitionCollector>();                  \
  collectTransitions(Transitions, *Ast, Conf);

#define BENCHMARK_GRAPH                                                        \
  const auto Query =                                                           \
      getQueriedTypeForInput(*Transitions, QueriedTypeAsString);               \
  const auto Data = createGraph(*Transitions, Query, Conf);

#define BENCHMARK_PATHTRAVERSAL                                                \
  const auto FoundPaths = pathTraversal(Data, Conf, SourceVertex);

#define BENCHMARK_GET_SOURCE_VERTEX                                            \
  const auto SourceVertex = getSourceVertexMatchingQueriedType(Data, Query);

#define BENCHMARK_BODY_TRANSITIONS                                             \
  BENCHMARK_TRANSITIONS                                                        \
  benchmark::DoNotOptimize(Transitions->begin());                              \
  benchmark::ClobberMemory();

#define BENCHMARK_BODY_GRAPH                                                   \
  BENCHMARK_GRAPH                                                              \
  benchmark::DoNotOptimize(Data.Graph);                                        \
  benchmark::DoNotOptimize(Data.VertexData.data());                            \
  benchmark::DoNotOptimize(Data.Edges.data());                                 \
  benchmark::DoNotOptimize(Data.EdgeIndices.data());                           \
  benchmark::DoNotOptimize(Data.EdgeTransitions.data());                       \
  benchmark::ClobberMemory();

#define BENCHMARK_BODY_PATHTRAVERSAL                                           \
  BENCHMARK_PATHTRAVERSAL                                                      \
  benchmark::DoNotOptimize(FoundPaths.data());                                 \
  benchmark::ClobberMemory();

#define BENCHMARK_BODY_FULL                                                    \
  BENCHMARK_BODY_TRANSITIONS                                                   \
  BENCHMARK_BODY_GRAPH                                                         \
  BENCHMARK_GET_SOURCE_VERTEX                                                  \
  BENCHMARK_BODY_PATHTRAVERSAL

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GENERATE_GENERATED_BENCHMARKS(Name, Generator, Args)                   \
  /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                             \
  using Name = benchmark::Fixture;                                             \
  BENCHMARK_DEFINE_F(Name, full)                                               \
  (benchmark::State & State) {                                                 \
    const auto [QueriedType, Code] =                                           \
        Generator(static_cast<size_t>(State.range(0)));                        \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_FULL                                                      \
    }                                                                          \
    State.SetComplexityN(                                                      \
        static_cast<std::int64_t>(State.counters["transitions"]));             \
  }                                                                            \
  /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                             \
  BENCHMARK_REGISTER_F(Name, full) Args;                                       \
  BENCHMARK_DEFINE_F(Name, transitions)                                        \
  (benchmark::State & State) {                                                 \
    const auto [QueriedType, Code] =                                           \
        Generator(static_cast<size_t>(State.range(0)));                        \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_TRANSITIONS                                               \
    }                                                                          \
    State.SetComplexityN(                                                      \
        static_cast<std::int64_t>(State.counters["transitions"]));             \
  }                                                                            \
  /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                             \
  BENCHMARK_REGISTER_F(Name, transitions) Args;                                \
  BENCHMARK_DEFINE_F(Name, graph)                                              \
  (benchmark::State & State) {                                                 \
    const auto [QueriedType, Code] =                                           \
        Generator(static_cast<size_t>(State.range(0)));                        \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    BENCHMARK_TRANSITIONS                                                      \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_GRAPH                                                     \
    }                                                                          \
    State.SetComplexityN(                                                      \
        static_cast<std::int64_t>(State.counters["transitions"]));             \
  }                                                                            \
  /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                             \
  BENCHMARK_REGISTER_F(Name, graph) Args;                                      \
  BENCHMARK_DEFINE_F(Name, pathTraversal)                                      \
  (benchmark::State & State) {                                                 \
    const auto [QueriedType, Code] =                                           \
        Generator(static_cast<size_t>(State.range(0)));                        \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    BENCHMARK_TRANSITIONS                                                      \
    BENCHMARK_GRAPH                                                            \
    BENCHMARK_GET_SOURCE_VERTEX                                                \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_PATHTRAVERSAL                                             \
    }                                                                          \
    State.SetComplexityN(                                                      \
        static_cast<std::int64_t>(State.counters["transitions"]));             \
  }                                                                            \
  /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                             \
  BENCHMARK_REGISTER_F(Name, pathTraversal) Args;

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define GENERATE_BENCHMARKS(Name, Code, QueriedType)                           \
  /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                             \
  using Name = benchmark::Fixture;                                             \
  BENCHMARK_DEFINE_F(Name, full)                                               \
  (benchmark::State & State) {                                                 \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_FULL                                                      \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, full);                                            \
  BENCHMARK_DEFINE_F(Name, transitions)                                        \
  (benchmark::State & State) {                                                 \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_TRANSITIONS                                               \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, transitions);                                     \
  BENCHMARK_DEFINE_F(Name, graph)                                              \
  (benchmark::State & State) {                                                 \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    BENCHMARK_TRANSITIONS                                                      \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_GRAPH                                                     \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, graph);                                           \
  BENCHMARK_DEFINE_F(Name, pathTraversal)                                      \
  (benchmark::State & State) {                                                 \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    BENCHMARK_TRANSITIONS                                                      \
    BENCHMARK_GRAPH                                                            \
    BENCHMARK_GET_SOURCE_VERTEX                                                \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_PATHTRAVERSAL                                             \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, pathTraversal);

#endif

#ifndef get_me_benchmark_get_me_benchmarks_hpp
#define get_me_benchmark_get_me_benchmarks_hpp

#include <memory>
#include <string>

#include <benchmark/benchmark.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>

#include "get_me/backwards_path_finding.hpp"
#include "get_me/graph.hpp"
#include "get_me/query.hpp"
#include "get_me/tooling.hpp"
#include "get_me/transitions.hpp"

inline void setupCounters(benchmark::State &State, clang::ASTUnit &Ast,
                          const std::string &QueriedTypeAsString) {
  const auto Conf = Config{};
  auto Transitions = std::make_shared<TransitionCollector>();
  collectTransitions(Transitions, Ast, Conf);
  State.counters["transitions"] = static_cast<double>(Transitions->Data.size());
  const auto Query =
      getQueriedTypeForInput(Transitions->Data, QueriedTypeAsString);
  auto Data = runGraphBuilding(Transitions, Query, Conf);
  State.counters["vertices"] = static_cast<double>(Data.VertexData.size());
  State.counters["edges"] = static_cast<double>(Data.Edges.size());
  State.counters["paths"] = static_cast<double>(Data.Paths.size());
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
      getQueriedTypeForInput(Transitions->Data, QueriedTypeAsString);          \
  auto Data = runGraphBuilding(Transitions, Query, Conf);

#define BENCHMARK_PATH_FINDING runPathFinding(Data);

#define BENCHMARK_BODY_TRANSITIONS                                             \
  BENCHMARK_TRANSITIONS                                                        \
  benchmark::DoNotOptimize(Transitions->Data.begin());                         \
  benchmark::ClobberMemory();

#define BENCHMARK_BODY_GRAPH                                                   \
  BENCHMARK_GRAPH                                                              \
  benchmark::DoNotOptimize(Data.VertexData.data());                            \
  benchmark::DoNotOptimize(Data.Edges.begin());                                \
  benchmark::ClobberMemory();

#define BENCHMARK_BODY_PATH_FINDING                                            \
  BENCHMARK_PATH_FINDING                                                       \
  benchmark::DoNotOptimize(Data.Paths.begin());                                \
  benchmark::ClobberMemory();

#define BENCHMARK_BODY_FULL                                                    \
  BENCHMARK_BODY_TRANSITIONS                                                   \
  BENCHMARK_BODY_GRAPH                                                         \
  BENCHMARK_BODY_PATH_FINDING

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
  BENCHMARK_DEFINE_F(Name, path_finding)(benchmark::State & State) {           \
    const auto [QueriedType, Code] =                                           \
        Generator(static_cast<size_t>(State.range(0)));                        \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    BENCHMARK_TRANSITIONS                                                      \
    BENCHMARK_GRAPH                                                            \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_PATH_FINDING                                              \
    }                                                                          \
    State.SetComplexityN(                                                      \
        static_cast<std::int64_t>(State.counters["transitions"]));             \
  }                                                                            \
  /* NOLINTNEXTLINE(bugprone-macro-parentheses) */                             \
  BENCHMARK_REGISTER_F(Name, path_finding) Args;

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
  BENCHMARK_DEFINE_F(Name, path_finding)                                       \
  (benchmark::State & State) {                                                 \
    SETUP_BENCHMARK(Code, QueriedType);                                        \
    BENCHMARK_TRANSITIONS                                                      \
    BENCHMARK_GRAPH                                                            \
    for (auto _ : State) {                                                     \
      BENCHMARK_BODY_PATH_FINDING                                              \
    }                                                                          \
  }                                                                            \
  BENCHMARK_REGISTER_F(Name, path_finding);

#endif

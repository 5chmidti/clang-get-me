#include "get_me/query_all.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/task_arena.h>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/projections.hpp"

namespace {
void buildGraphAndFindPaths(std::shared_ptr<TransitionCollector> Transitions,
                            const std::string_view QueriedType,
                            const Config &Conf) {
  const auto Query =
      QueryType{std::move(Transitions), std::string{QueriedType}, Conf};
  const auto Data = createGraph(Query);
  const auto SourceVertex =
      getSourceVertexMatchingQueriedType(Data, Query.getQueriedType());
  const auto VertexDataSize = Data.VertexData.size();
  GetMeException::verify(VertexDataSize != 0,
                         "Error finding source vertex for queried type {}",
                         Query.getQueriedTypeAsString());
  GetMeException::verify(SourceVertex < VertexDataSize,
                         "Source vertex for queried type {} is out of range",
                         Query.getQueriedTypeAsString());

  // return because querying all might query a type with no
  // edges/transitions that acquire it
  if (Data.Edges.empty()) {
    return;
  }

  std::ignore = pathTraversal(Data, Conf, SourceVertex);
}
} // namespace

void queryAll(const std::shared_ptr<TransitionCollector> &Transitions,
              const Config &Conf) {
  const auto Run = [Transitions, &Conf](const auto &QueriedType) {
    buildGraphAndFindPaths(Transitions, fmt::format("{}", QueriedType), Conf);
  };

  spdlog::info("Running with {} threads",
               tbb::this_task_arena::max_concurrency());
  tbb::parallel_for_each(*Transitions | ranges::views::transform(ToAcquired),
                         Run);
}

#include "get_me/query_all.hpp"

#include <memory>

#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/task_arena.h>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/graph.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"

void queryAll(const std::shared_ptr<TransitionCollector> &Transitions,
              const Config &Conf) {
  const auto Run = [Transitions, &Conf](const auto &QueriedType) {
    const auto Query = getQueriedTypeForInput(Transitions->Data,
                                              fmt::format("{}", QueriedType));
    auto Data = runGraphBuildingAndPathFinding(Transitions, Query, Conf);
  };

  spdlog::info("Running with {} threads",
               tbb::this_task_arena::max_concurrency());
  tbb::parallel_for_each(
      Transitions->Data | ranges::views::transform(ToAcquired), Run);
}

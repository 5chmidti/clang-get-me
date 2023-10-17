#include "get_me/query_all.hpp"

#include <memory>

#include <oneapi/tbb/parallel_for_each.h>
#include <oneapi/tbb/task_arena.h>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/backwards_path_finding.hpp"
#include "get_me/graph.hpp"
#include "get_me/query.hpp"
#include "get_me/transitions.hpp"

void queryAll(const std::shared_ptr<TransitionData> &Transitions,
              const std::shared_ptr<Config> Conf) {
  const auto Run = [Transitions, &Conf](const auto &QueriedType) {
    const auto Query =
        getQueriedTypesForInput(*Transitions, fmt::format("{}", QueriedType));
    auto Data = runGraphBuilding(Transitions, Query, Conf);
    runPathFinding(Data);
  };

  spdlog::trace("Running with {} threads",
                tbb::this_task_arena::max_concurrency());
  tbb::parallel_for_each(
      Transitions->Data | ranges::views::transform(ToAcquired), Run);
}

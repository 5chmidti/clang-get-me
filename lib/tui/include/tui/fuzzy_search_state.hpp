#ifndef get_me_tool_include_tool_tui_fuzzy_search_state_hpp
#define get_me_tool_include_tool_tui_fuzzy_search_state_hpp

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <range/v3/action/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>
#include <rapidfuzz/fuzz.hpp>

#include "support/ranges/functional.hpp"

class FuzzySearchState {
public:
  explicit FuzzySearchState(const std::vector<std::string> *const Values)
      : Original_{Values} {}

  void scoreAndSort(const auto &Query) {
    auto ScoredValues =
        *Original_ |
        ranges::views::transform([&Query](const auto &SearchListElement) {
          return std::pair{SearchListElement,
                           rapidfuzz::fuzz::ratio(Query, SearchListElement)};
        }) |
        ranges::to_vector | ranges::actions::sort(std::greater{}, Element<1>);
    Values_ = ScoredValues | ranges::views::move |
              ranges::views::transform(Element<0>) | ranges::to_vector;
  }

  [[nodiscard]] std::vector<std::string> &getValues() { return Values_; }
  [[nodiscard]] const std::vector<std::string> &getValues() const {
    return Values_;
  }

private:
  const std::vector<std::string> *Original_;
  std::vector<std::string> Values_;
};

#endif

#include "get_me/query.hpp"

#include <memory>
#include <string>
#include <utility>
#include <variant>

#include <fmt/ranges.h>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/variant.hpp"

QueryType::QueryType(std::shared_ptr<TransitionCollector> Transitions,
                     std::string Query, Config Conf)
    : Transitions_{std::move(Transitions)},
      QueriedTypeAsString_{std::move(Query)},
      QueriedType_{getQueriedTypeForInput()},
      Conf_{Conf} {}

TransitionCollector QueryType::getTransitionsForQuery() const {
  const auto QueriedTypeIsSubset = [this](const auto &Required) {
    return isSubset(Required, QueriedType_);
  };

  return *Transitions_ |
         ranges::views::transform(
             [&QueriedTypeIsSubset](const BundeledTransitionType &Transition) {
               return std::pair{
                   Transition.first,
                   Transition.second |
                       ranges::views::filter(
                           ranges::not_fn(QueriedTypeIsSubset), ToRequired) |
                       ranges::to<StrippedTransitionsSet>};
             }) |
         ranges::to<TransitionCollector>;
}

bool QueryType::matchesQueriedTypeName(const TypeSetValueType &Val) const {
  return std::visit(
      Overloaded{
          [this](const clang::Type *const Type) {
            const auto TypeAsString = toString(Type);
            const auto EquivalentName = TypeAsString == QueriedTypeAsString_;
            if (!EquivalentName && (TypeAsString.find(QueriedTypeAsString_) !=
                                    std::string::npos)) {
              spdlog::trace("QueryType::matchesQueriedTypeName: no match for "
                            "close match: {} != {}",
                            TypeAsString, QueriedTypeAsString_);
            }
            return EquivalentName;
          },
          [](const ArithmeticType &) { return false; }},
      Val);
}

TypeSetValueType QueryType::getQueriedTypeForInput() {
  if (Transitions_->empty()) {
    throw GetMeException(
        "QueryType::getQueriedTypeForInput(): Transitions are empty");
  }
  const auto FilteredTypes =
      *Transitions_ | ranges::views::transform(ToAcquired) |
      ranges::views::filter([this](const TypeSetValueType &Acquired) {
        return matchesQueriedTypeName(Acquired);
      }) |
      ranges::to_vector;

  GetMeException::verify(
      !FilteredTypes.empty(),
      "QueryType::getQueriedTypeForInput(): no type matching {}",
      QueriedTypeAsString_);

  spdlog::trace("{}", FilteredTypes);

  return ranges::front(FilteredTypes);
}

#include "get_me/query.hpp"

#include <memory>
#include <string>
#include <string_view>
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
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/variant.hpp"

namespace {
[[nodiscard]] bool
matchesQueriedTypeName(const TypeSetValueType &Val,
                       const std::string_view QueriedTypeAsString) {
  return std::visit(
      Overloaded{
          [QueriedTypeAsString](const clang::Type *const Type) {
            const auto TypeAsString = toString(Type);
            const auto EquivalentName = TypeAsString == QueriedTypeAsString;
            if (!EquivalentName &&
                (TypeAsString.find(QueriedTypeAsString) != std::string::npos)) {
              spdlog::trace("matchesQueriedTypeName: no match for "
                            "close match: {} != {}",
                            TypeAsString, QueriedTypeAsString);
            }
            return EquivalentName;
          },
          [&](const ArithmeticType &) {
            return QueriedTypeAsString == fmt::format("{}", ArithmeticType{});
          }},
      Val);
}
} // namespace

TypeSetValueType
getQueriedTypeForInput(const TransitionCollector &Transitions,
                       const std::string_view QueriedTypeAsString) {
  if (Transitions.empty()) {
    GetMeException::fail("getQueriedTypeForInput(): Transitions are empty");
  }
  const auto FilteredTypes =
      Transitions | ranges::views::transform(ToAcquired) |
      ranges::views::filter(
          [QueriedTypeAsString](const TypeSetValueType &Acquired) {
            return matchesQueriedTypeName(Acquired, QueriedTypeAsString);
          }) |
      ranges::to_vector;

  GetMeException::verify(!FilteredTypes.empty(),
                         "getQueriedTypeForInput(): no type matching {}",
                         QueriedTypeAsString);

  spdlog::trace("{}", FilteredTypes);

  return ranges::front(FilteredTypes);
}

TransitionCollector
getTransitionsForQuery(const TransitionCollector &Transitions,
                       const TypeSetValueType &QueriedType) {
  const auto QueriedTypeIsSubset = [&QueriedType](const auto &Required) {
    return Required.contains(QueriedType);
  };

  return Transitions |
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

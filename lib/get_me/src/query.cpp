#include "get_me/query.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include <fmt/ranges.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/ranges.hpp"
#include "support/variant.hpp"

namespace {
[[nodiscard]] bool
matchesQueriedTypeName(const TypeSetValueType &Val,
                       const std::string_view QueriedTypeAsString) {
  return std::visit(
      Overloaded{
          [QueriedTypeAsString](const clang::QualType &QType) {
            const auto TypeAsString = fmt::format("{}", QType);
            const auto EquivalentName = TypeAsString == QueriedTypeAsString;
            if (!EquivalentName &&
                (TypeAsString.find(QueriedTypeAsString) != std::string::npos)) {
              spdlog::trace("matchesQueriedTypeName: no match for "
                            "close match: {} != {}",
                            TypeAsString, QueriedTypeAsString);
            }
            return EquivalentName;
          },
          [QueriedTypeAsString](const ArithmeticType &) {
            return QueriedTypeAsString == fmt::format("{}", ArithmeticType{});
          }},
      Val);
}
} // namespace

TypeSet getQueriedTypesForInput(const TransitionData &Transitions,
                                const std::string_view QueriedTypeAsString) {
  auto FilteredTypes =
      Transitions.ConversionMap | ranges::views::keys |
      ranges::views::filter(
          [QueriedTypeAsString](const TypeSetValueType &Acquired) {
            return matchesQueriedTypeName(Acquired, QueriedTypeAsString);
          }) |
      ranges::to<TypeSet>;

  GetMeException::verify(!FilteredTypes.empty(),
                         "getQueriedTypeForInput(): no type matching {}",
                         QueriedTypeAsString);

  return FilteredTypes;
}

TransitionData::associative_container_type getTransitionsForQuery(
    const TransitionData::associative_container_type &Transitions,
    const TypeSet &Query) {
  const auto QueriedTypeIsSubset = [&Query](const auto &Required) {
    return ranges::any_of(Query, [&Required](const auto &QueriedType) {
      return Required.contains(QueriedType);
    });
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
         ranges::to<TransitionData::associative_container_type>;
}

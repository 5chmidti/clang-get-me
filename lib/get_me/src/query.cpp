#include "get_me/query.hpp"

#include <string>
#include <string_view>
#include <variant>

#include <clang/AST/Type.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>
#include <spdlog/spdlog.h>

#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "support/ranges/functional.hpp"
#include "support/variant.hpp"

namespace {
[[nodiscard]] bool
matchesQueriedTypeName(const TypeSetValueType &Val,
                       const std::string_view QueriedTypeAsString) {
  const auto MatchesName = Overloaded{
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
      }};
  return std::visit(MatchesName, Val.Actual);
}
} // namespace

TypeSet getQueriedTypesForInput(const TransitionData &Transitions,
                                const std::string_view QueriedTypeAsString) {
  const auto QueriedTypesIter = ranges::find_if(
      Transitions.ConversionMap,
      ranges::bind_back(ranges::any_of,
                        [QueriedTypeAsString](const TypeSetValueType &Type) {
                          return matchesQueriedTypeName(Type,
                                                        QueriedTypeAsString);
                        }),
      Value);

  GetMeException::verify(QueriedTypesIter != Transitions.ConversionMap.end(),
                         "getQueriedTypeForInput(): no type matching {} in {}",
                         QueriedTypeAsString, Transitions.ConversionMap);

  return QueriedTypesIter->second;
}

TransitionData::associative_container_type getTransitionsForQuery(
    const TransitionData::associative_container_type &Transitions,
    const TypeSet &Query) {
  const auto QueriedTypeIsSubset = [&Query](const auto &Required) {
    return ranges::any_of(Query,
                          [&Required](const TypeSetValueType &QueriedType) {
                            return Required.contains(QueriedType);
                          });
  };

  return Transitions |
         ranges::views::filter(ranges::not_fn(QueriedTypeIsSubset),
                               ToRequired) |
         ranges::to<TransitionData::associative_container_type>;
}

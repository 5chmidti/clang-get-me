#include "get_me/query.hpp"

#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/filter.hpp>
#include <spdlog/spdlog.h>

#include "get_me/formatting.hpp"
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
            const auto QType = clang::QualType(Type, 0);
            const auto TypeAsString = [&QType]() {
              auto QTypeAsString = QType.getAsString();
              boost::erase_all(QTypeAsString, "struct");
              boost::erase_all(QTypeAsString, "class");
              boost::trim(QTypeAsString);
              return QTypeAsString;
            }();
            const auto EquivalentName = TypeAsString == QueriedTypeAsString_;
            if (!EquivalentName && (TypeAsString.find(QueriedTypeAsString_) !=
                                    std::string::npos)) {
              spdlog::trace("matchesName(QualType): no match for "
                            "close match: {} vs {}",
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
#ifndef get_me_include_get_me_query_hpp
#define get_me_include_get_me_query_hpp

#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/functional/not_fn.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/transitions.hpp"
#include "get_me/utility.hpp"

class QueryType {
public:
  QueryType(TransitionCollector Transitions, std::string Query)
      : Transitions{std::move(Transitions)},
        QueriedTypeAsString{std::move(Query)}, QueriedType{
                                                   getQueriedTypeForInput()} {}

  [[nodiscard]] TransitionCollector getTransitionsForQuery() const {
    const auto QueriedTypeIsSubset = [this](const auto &Required) {
      return isSubset(Required, QueriedType);
    };

    return Transitions |
           ranges::views::filter(ranges::not_fn(QueriedTypeIsSubset),
                                 required) |
           ranges::to<TransitionCollector>;
  }

  // getSourceVertexMatchingQueriedType

  [[nodiscard]] const TransitionCollector &getTransitions() const {
    return Transitions;
  }
  [[nodiscard]] const std::string &getQueriedTypeAsString() const {
    return QueriedTypeAsString;
  }
  [[nodiscard]] const TypeSet &getQueriedType() const { return QueriedType; }

private:
  [[nodiscard]] auto matchesQueriedTypeName() const {
    return [this](const TypeSetValueType &Val) {
      return std::visit(
          Overloaded{[this](const clang::Type *const Type) {
                       const auto QType = clang::QualType(Type, 0);
                       const auto TypeAsString = [&QType]() {
                         auto QTypeAsString = QType.getAsString();
                         boost::erase_all(QTypeAsString, "struct");
                         boost::erase_all(QTypeAsString, "class");
                         boost::trim(QTypeAsString);
                         return QTypeAsString;
                       }();
                       const auto EquivalentName =
                           TypeAsString == QueriedTypeAsString;
                       if (!EquivalentName &&
                           (TypeAsString.find(QueriedTypeAsString) !=
                            std::string::npos)) {
                         spdlog::trace("matchesName(QualType): no match for "
                                       "close match: {} vs {}",
                                       TypeAsString, QueriedTypeAsString);
                       }
                       return EquivalentName;
                     },
                     [](const ArithmeticType &) { return false; }},
          Val);
    };
  }

  [[nodiscard]] TypeSet getQueriedTypeForInput() {
    if (Transitions.empty()) {
      spdlog::error(
          "QueryType::getQueriedTypeForInput(): Transitions are empty");
      return {};
    }
    const auto FilteredTypes =
        Transitions | ranges::views::transform(acquired) |
        ranges::views::filter([this](const TypeSet &Acquired) {
          return ranges::any_of(Acquired, matchesQueriedTypeName());
        }) |
        ranges::to_vector;

    if (FilteredTypes.empty()) {
      spdlog::error("QueryType::getQueriedTypeForInput(): no type matching {}",
                    QueriedTypeAsString);
      return {};
    }

    return ranges::front(FilteredTypes);
  }

  TransitionCollector Transitions{};
  std::string QueriedTypeAsString{};
  TypeSet QueriedType{};
};

#endif

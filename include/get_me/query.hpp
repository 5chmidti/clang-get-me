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
#include "get_me/type_set.hpp"
#include "support/variant.hpp"

class QueryType {
public:
  QueryType(TransitionCollector Transitions, std::string Query, Config Conf)
      : Transitions_{std::move(Transitions)},
        QueriedTypeAsString_{std::move(Query)},
        QueriedType_{getQueriedTypeForInput()},
        Conf_{Conf} {}

  [[nodiscard]] TransitionCollector getTransitionsForQuery() const {
    const auto QueriedTypeIsSubset = [this](const auto &Required) {
      return isSubset(Required, QueriedType_);
    };

    return Transitions_ |
           ranges::views::transform(
               [&QueriedTypeIsSubset](
                   const BundeledTransitionType &Transition) {
                 return std::pair{
                     Transition.first,
                     Transition.second |
                         ranges::views::filter(
                             ranges::not_fn(QueriedTypeIsSubset), ToRequired) |
                         ranges::to<StrippedTransitionsSet>};
               }) |
           ranges::to<TransitionCollector>;
  }

  // getSourceVertexMatchingQueriedType

  [[nodiscard]] const TransitionCollector &getTransitions() const {
    return Transitions_;
  }
  [[nodiscard]] const std::string &getQueriedTypeAsString() const {
    return QueriedTypeAsString_;
  }
  [[nodiscard]] const TypeSet &getQueriedType() const { return QueriedType_; }
  [[nodiscard]] const Config &getConfig() const { return Conf_; }

private:
  [[nodiscard]] auto matchesQueriedTypeName(const TypeSetValueType &Val) const {
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

  [[nodiscard]] TypeSetValueType getQueriedTypeForInput() {
    if (Transitions_.empty()) {
      spdlog::error(
          "QueryType::getQueriedTypeForInput(): Transitions are empty");
      return {};
    }
    const auto FilteredTypes =
        Transitions_ | ranges::views::transform(ToAcquired) |
        ranges::views::filter([this](const TypeSetValueType &Acquired) {
          return matchesQueriedTypeName(Acquired);
        }) |
        ranges::to_vector;

    if (FilteredTypes.empty()) {
      spdlog::error("QueryType::getQueriedTypeForInput(): no type matching {}",
                    QueriedTypeAsString_);
      return {};
    }

    return ranges::front(FilteredTypes);
  }

  TransitionCollector Transitions_{};
  std::string QueriedTypeAsString_{};
  TypeSet QueriedType_{};
  Config Conf_{};
};

#endif

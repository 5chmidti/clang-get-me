#ifndef get_me_lib_get_me_include_get_me_transitions_hpp
#define get_me_lib_get_me_include_get_me_transitions_hpp

#include <cstddef>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <fmt/core.h>

#include "get_me/indexed_set.hpp"
#include "get_me/type_set.hpp"
#include "support/concepts.hpp"
#include "support/ranges/functional.hpp"

using TransitionDataType =
    std::variant<const clang::FunctionDecl *, const clang::FieldDecl *,
                 const clang::VarDecl *>;

[[nodiscard]] std::string getTransitionName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionAcquiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionRequiredTypeNames(const TransitionDataType &Data);

template <> class fmt::formatter<TransitionDataType> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(const TransitionDataType &Val,
                                                format_context &Ctx) const {
    return fmt::format_to(
        Ctx.out(), "{} {}({})", getTransitionAcquiredTypeNames(Val),
        getTransitionName(Val), getTransitionRequiredTypeNames(Val));
  }
};

using TransitionType =
    indexed_value<std::tuple<TypeSetValueType, TransitionDataType, TypeSet>>;

using StrippedTransitionType =
    indexed_value<std::pair<TransitionDataType, TypeSet>>;

using StrippedTransitionsSet =
    boost::container::flat_set<StrippedTransitionType>;

struct TransitionData {
  using associative_container_type =
      boost::container::flat_map<TypeSetValueType, StrippedTransitionsSet>;
  using value_type = associative_container_type::value_type;
  using flat_container_type = std::vector<TransitionType>;

  void commit();

  associative_container_type Data{};
  flat_container_type FlatData{};
};

using BundeledTransitionType = TransitionData::value_type;

inline constexpr auto ToAcquired =
    []<typename T>(T &&Transition) -> decltype(auto)
  requires IsAnyOf<std::remove_cvref_t<T>, TransitionType,
                   BundeledTransitionType>
{
  using BaseType = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<BaseType, TransitionType>) {
    return std::get<0>(Value(std::forward<T>(Transition)));
  } else if constexpr (std::is_same_v<BaseType, BundeledTransitionType>) {
    return std::get<0>(std::forward<T>(Transition));
  }
};

inline constexpr auto ToRequired =
    []<typename T>(T &&Transition) -> decltype(auto)
  requires IsAnyOf<std::remove_cvref_t<T>, TransitionType,
                   StrippedTransitionType, StrippedTransitionType::second_type>
{
  using BaseType = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<BaseType, TransitionType>) {
    return std::get<2>(Value(std::forward<T>(Transition)));
  } else if constexpr (std::is_same_v<BaseType, StrippedTransitionType>) {
    return std::get<1>(Value(std::forward<T>(Transition)));
  } else if constexpr (std::is_same_v<BaseType,
                                      StrippedTransitionType::second_type>) {
    return std::get<1>(std::forward<T>(Transition));
  }
};

inline constexpr auto ToTransition =
    []<typename T>(T &&Transition) -> decltype(auto)
  requires IsAnyOf<std::remove_cvref_t<T>, TransitionType,
                   StrippedTransitionType, StrippedTransitionType::second_type>
{
  using BaseType = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<BaseType, TransitionType>) {
    return std::get<1>(Value(std::forward<T>(Transition)));
  } else if constexpr (std::is_same_v<BaseType, StrippedTransitionType>) {
    return std::get<0>(Value(std::forward<T>(Transition)));
  } else if constexpr (std::is_same_v<BaseType,
                                      StrippedTransitionType::second_type>) {
    return std::get<0>(std::forward<T>(Transition));
  }
};

[[nodiscard]] inline bool independent(const TransitionType &Lhs,
                                      const TransitionType &Rhs) {
  return !ToRequired(Rhs).contains(ToAcquired(Lhs)) &&
         !ToRequired(Lhs).contains(ToAcquired(Rhs));
}

[[nodiscard]] inline auto independentOf(const TransitionType &Transition) {
  return [&Transition](const TransitionType &OtherTransition) {
    return independent(Transition, OtherTransition);
  };
}

#endif

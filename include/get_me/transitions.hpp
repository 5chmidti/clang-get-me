#ifndef get_me_include_get_me_transitions_hpp
#define get_me_include_get_me_transitions_hpp

#include <type_traits>

#include <boost/container/container_fwd.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

#include "get_me/type_set.hpp"
#include "support/concepts.hpp"

using TransitionDataType =
    std::variant<const clang::FunctionDecl *, const clang::FieldDecl *,
                 const clang::VarDecl *>;

using TransitionType =
    std::tuple<TypeSetValueType, TransitionDataType, TypeSet>;

using StrippedTransitionType = std::pair<TransitionDataType, TypeSet>;

using StrippedTransitionsSet =
    boost::container::flat_set<StrippedTransitionType>;
using TransitionCollector =
    boost::container::flat_map<TypeSetValueType, StrippedTransitionsSet>;

using BundeledTransitionType = TransitionCollector::value_type;

inline constexpr auto ToAcquired =
    []<typename T>(T &&Transition) -> decltype(auto) {
  return std::get<0>(std::forward<T>(Transition));
};

inline constexpr auto ToRequired =
    []<typename T>(T && Transition) -> decltype(auto)
  requires IsAnyOf<std::remove_cvref_t<T>, TransitionType,
                   StrippedTransitionType>
{
  using BaseType = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<BaseType, TransitionType>) {
    return std::get<2>(std::forward<T>(Transition));
  } else if constexpr (std::is_same_v<BaseType, StrippedTransitionType>) {
    return std::get<1>(std::forward<T>(Transition));
  }
};

inline constexpr auto ToTransition =
    []<typename T>(T && Transition) -> decltype(auto)
  requires IsAnyOf<std::remove_cvref_t<T>, TransitionType,
                   StrippedTransitionType>
{
  using BaseType = std::remove_cvref_t<T>;
  if constexpr (std::is_same_v<BaseType, TransitionType>) {
    return std::get<1>(std::forward<T>(Transition));
  } else if constexpr (std::is_same_v<BaseType, StrippedTransitionType>) {
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

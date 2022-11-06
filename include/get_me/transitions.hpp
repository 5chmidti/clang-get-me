#ifndef get_me_include_get_me_transitions_hpp
#define get_me_include_get_me_transitions_hpp

#include "get_me/type_set.hpp"

using TransitionDataType =
    std::variant<const clang::FunctionDecl *, const clang::FieldDecl *,
                 const clang::VarDecl *>;

using TransitionType = std::tuple<TypeSet, TransitionDataType, TypeSet>;

[[nodiscard]] inline const TypeSet &acquired(const TransitionType &Transition) {
  return std::get<0>(Transition);
}

[[nodiscard]] inline const TransitionDataType &
transition(const TransitionType &Transition) {
  return std::get<1>(Transition);
}

[[nodiscard]] inline const TypeSet &required(const TransitionType &Transition) {
  return std::get<2>(Transition);
}

using TransitionCollector = std::set<TransitionType>;

[[nodiscard]] inline bool independent(const TransitionType &Lhs,
                                      const TransitionType &Rhs) {
  return setIntersectionIsEmpty(acquired(Lhs), required(Rhs)) &&
         setIntersectionIsEmpty(required(Lhs), acquired(Rhs));
}

[[nodiscard]] inline auto independentOf(const TransitionType &Transition) {
  return [&Transition](const TransitionType &OtherTransition) {
    return independent(Transition, OtherTransition);
  };
}

#endif

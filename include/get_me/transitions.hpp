#ifndef get_me_include_get_me_transitions_hpp
#define get_me_include_get_me_transitions_hpp

#include "get_me/type_set.hpp"

struct DefaultedConstructor {
  const clang::CXXRecordDecl *Record;

  [[nodiscard]] friend auto operator<=>(const DefaultedConstructor &,
                                        const DefaultedConstructor &) = default;
};

using TransitionDataType =
    std::variant<std::monostate, const clang::FunctionDecl *,
                 const clang::FieldDecl *, const clang::VarDecl *,
                 DefaultedConstructor>;

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

#endif

#ifndef get_me_include_support_concepts_hpp
#define get_me_include_support_concepts_hpp

#include <concepts>

template <typename T, typename... Ts>
inline constexpr bool IsAnyOf = (std::same_as<T, Ts> || ...);

#endif

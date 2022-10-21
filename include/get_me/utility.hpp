#ifndef get_me_include_get_me_utility_hpp
#define get_me_include_get_me_utility_hpp

#include <concepts>
#include <utility>

template <class... Ts> struct Overloaded : public Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <std::size_t I>
inline const auto Element = []<typename T>(T &&Tuple) -> decltype(auto) {
  return std::get<I>(std::forward<T>(Tuple));
};

#endif

#ifndef get_me_include_get_me_utility_hpp
#define get_me_include_get_me_utility_hpp

#include <concepts>
#include <utility>

template <class... Ts> struct Overloaded : public Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <typename T> class FunctionToClosure {
public:
  explicit FunctionToClosure(T Function) : Func{Function} {}

  template <typename... Ts>
  [[nodiscard]] auto operator()(Ts &&...Args) const
    requires std::invocable<T, Ts...>
  {
    return Func(std::forward<Ts>(Args)...);
  }

private:
  T Func;
};

#endif

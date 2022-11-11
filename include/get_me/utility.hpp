#ifndef get_me_include_get_me_utility_hpp
#define get_me_include_get_me_utility_hpp

#include <concepts>
#include <utility>

#include <range/v3/range/concepts.hpp>
#include <range/v3/view/subrange.hpp>

namespace llvm {
template <typename T> class iterator_range;
template <typename T> class ArrayRef;
} // namespace llvm

namespace ranges {
template <typename T>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<llvm::iterator_range<T>> = true;

template <typename T>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<llvm::ArrayRef<T>> = true;
} // namespace ranges

template <class... Ts> struct Overloaded : public Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <std::size_t I>
inline constexpr auto Element = []<typename T>(T &&Tuple) -> decltype(auto) {
  return std::get<I>(std::forward<T>(Tuple));
};

[[nodiscard]] ranges::viewable_range auto toRange(auto Pair) {
  auto [first, second] = Pair;
  return ranges::subrange{std::move(first), std::move(second)};
}

#endif

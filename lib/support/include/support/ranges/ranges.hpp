#ifndef get_me_lib_support_include_support_ranges_hpp
#define get_me_lib_support_include_support_ranges_hpp

#include <range/v3/range/concepts.hpp>
#include <range/v3/view/subrange.hpp>

namespace llvm {
template <typename T> class iterator_range;
template <typename T> class ArrayRef;
} // namespace llvm

template <typename T>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    ranges::enable_borrowed_range<llvm::iterator_range<T>> = true;

template <typename T>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    ranges::enable_borrowed_range<llvm::ArrayRef<T>> = true;

[[nodiscard]] constexpr ranges::viewable_range auto toRange(auto Pair) {
  auto [first, second] = Pair;
  return ranges::subrange{std::move(first), std::move(second)};
}

#endif

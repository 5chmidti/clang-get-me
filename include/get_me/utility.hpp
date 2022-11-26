#ifndef get_me_include_get_me_utility_hpp
#define get_me_include_get_me_utility_hpp

#include <concepts>
#include <type_traits>
#include <utility>

#include <clang/AST/Decl.h>
#include <range/v3/detail/range_access.hpp>
#include <range/v3/iterator/default_sentinel.hpp>
#include <range/v3/range/access.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/view/facade.hpp>
#include <range/v3/view/subrange.hpp>
#include <range/v3/view/view.hpp>

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

template <typename T, typename... Ts>
inline constexpr bool IsAnyOf = (std::same_as<T, Ts> || ...);

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <std::size_t I>
inline constexpr auto Element = []<typename T>(T &&Tuple) -> decltype(auto) {
  return std::get<I>(std::forward<T>(Tuple));
};

[[nodiscard]] ranges::viewable_range auto toRange(auto Pair) {
  auto [first, second] = Pair;
  return ranges::subrange{std::move(first), std::move(second)};
}

inline constexpr auto ToQualType = [](const clang::ValueDecl *const VDecl) {
  return VDecl->getType();
};

template <ranges::viewable_range Rng>
class ConditionalView : public ranges::view_facade<ConditionalView<Rng>> {
public:
  using value_type = ranges::range_value_t<Rng>;

  ConditionalView() = default;
  ConditionalView(bool Flag, Rng Range)
      : flag_(Flag),
        Cursor_(ranges::begin(Range)),
        Sentinel_(ranges::end(Range)) {}

private:
  friend ranges::range_access;

  [[nodiscard]] value_type read() const { return *Cursor_; }

  void next() { ++Cursor_; }

  [[nodiscard]] bool equal(ranges::default_sentinel_t /*unused*/) const {
    return !flag_ || Cursor_ == Sentinel_;
  }

  bool flag_{};
  ranges::iterator_t<Rng> Cursor_{};
  ranges::iterator_t<Rng> Sentinel_{};
};

class ConditionalFn {
public:
  template <ranges::viewable_range Rng>
  [[nodiscard]] auto operator()(bool Flag, Rng &&Range) const {
    return ConditionalView<Rng>{Flag, std::forward<Rng>(Range)};
  }
};

inline constexpr ConditionalFn Conditional{};

#endif

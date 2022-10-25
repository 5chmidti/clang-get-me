#ifndef get_me_include_get_me_utility_hpp
#define get_me_include_get_me_utility_hpp

#include <concepts>
#include <utility>

#include <clang/AST/DeclCXX.h>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/subrange.hpp>

namespace ranges {
template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<typename clang::CXXRecordDecl::base_class_range> =
        true;
template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<
        typename clang::CXXRecordDecl::base_class_const_range> = true;

template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<typename clang::CXXRecordDecl::method_range> = true;

template <>
inline constexpr bool
    // NOLINTNEXTLINE(readability-identifier-naming)
    enable_borrowed_range<typename clang::CXXRecordDecl::ctor_range> = true;
} // namespace ranges

static_assert(
    ranges::viewable_range<typename clang::CXXRecordDecl::base_class_range>);
static_assert(ranges::viewable_range<
              typename clang::CXXRecordDecl::base_class_const_range>);
static_assert(
    ranges::viewable_range<typename clang::CXXRecordDecl::method_range>);
static_assert(
    ranges::viewable_range<typename clang::CXXRecordDecl::ctor_range>);

template <class... Ts> struct Overloaded : public Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <std::size_t I>
inline const auto Element = []<typename T>(T &&Tuple) -> decltype(auto) {
  return std::get<I>(std::forward<T>(Tuple));
};

[[nodiscard]] ranges::viewable_range auto toRange(auto Pair) {
  auto [first, second] = Pair;
  return ranges::subrange{std::move(first), std::move(second)};
}

#endif

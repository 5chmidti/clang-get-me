#ifndef get_me_include_support_ranges_functional_hpp
#define get_me_include_support_ranges_functional_hpp

#pragma once

#include <cmath>
#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>

#include <range/v3/functional/arithmetic.hpp>
#include <range/v3/range/traits.hpp>

inline constexpr auto Plus = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::plus<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Minus = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::minus<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Multiplies = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::multiplies<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Divides = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::divides<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Modulus = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::modulus<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Negate = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::negate<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LogicalAnd = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::logical_and<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LogicalOr = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::logical_or<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LogicalNot = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::logical_not<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitAnd = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::bit_and<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitOr = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::bit_or<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitXor = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::bit_xor<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitNot = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::bit_not<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto EqualTo = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::ranges::equal_to{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto NotEqualTo = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::ranges::not_equal_to{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Greater = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::ranges::greater{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Less = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::ranges::less{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto GreaterEqual = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::ranges::greater_equal{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LessEqual = []<typename T>(T &&Rhs) {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) {
    return std::ranges::less_equal{}(Lhs, CapturedRhs);
  };
};

template <typename T>
inline constexpr auto CastAs = []<typename ValueType>(ValueType && Val)
  requires std::convertible_to<ValueType, T>
{
  return ranges::convert_to<T>{}(std::forward<ValueType>(Val));
};

template <typename T>
inline constexpr auto Construct = []<typename ValueType>(ValueType && Val)
  requires std::constructible_from<T, ValueType>
{
  return static_cast<T>(std::forward<ValueType>(Val));
};

inline constexpr auto Lookup =
    []<std::ranges::random_access_range T, typename Projection = std::identity>(
        T && Range, Projection Proj = {})
  requires std::is_trivially_copy_constructible_v<Projection>
{
  if constexpr (std::is_const_v<T>) {
    return [ Range = std::forward<T>(Range), Proj ]<typename IndexType>(
               const IndexType &Index) -> ranges::range_reference_t<T>
             requires std::regular_invocable<Projection, IndexType>
    {
      return Range[std::invoke(Proj, Index)];
    };
  } else {
    return [ Range = std::forward<T>(Range), Proj ]<typename IndexType>(
               const IndexType &Index) mutable -> ranges::range_reference_t<T>
             requires std::regular_invocable<Projection, IndexType>
    {
      return Range[std::invoke(Proj, Index)];
    };
  }
};

#endif

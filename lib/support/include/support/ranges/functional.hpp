#ifndef get_me_lib_support_include_support_ranges_functional_hpp
#define get_me_lib_support_include_support_ranges_functional_hpp

#pragma once

#include <cmath>
#include <concepts>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>

#include <range/v3/functional/arithmetic.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/traits.hpp>

inline constexpr auto Plus = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::plus<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Minus = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::minus<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Multiplies = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::multiplies<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Divides = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::divides<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Modulus = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::modulus<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Negate = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::negate<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LogicalAnd = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::logical_and<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LogicalOr = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::logical_or<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LogicalNot = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::logical_not<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitAnd = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::bit_and<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitOr = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::bit_or<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitXor = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::bit_xor<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto BitNot = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return std::bit_not<T>{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto EqualTo = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return ranges::equal_to{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto NotEqualTo = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return ranges::not_equal_to{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Greater = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return ranges::greater{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto Less = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return ranges::less{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto GreaterEqual = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return ranges::greater_equal{}(Lhs, CapturedRhs);
  };
};

inline constexpr auto LessEqual = []<typename T>(T &&Rhs) constexpr {
  return [CapturedRhs = std::forward<T>(Rhs)](const auto &Lhs) constexpr {
    return ranges::less_equal{}(Lhs, CapturedRhs);
  };
};

template <typename T>
inline constexpr auto CastAs =
    []<typename ValueType>(ValueType &&Val) constexpr {
      return static_cast<T>(std::forward<ValueType>(Val));
    };

template <typename T>
inline constexpr auto Construct = []<typename ValueType>(ValueType && Val)
  requires std::constructible_from<T, ValueType>
{
  return T{std::forward<ValueType>(Val)};
};

inline constexpr auto Lookup =
    []<ranges::random_access_range T, typename Projection = std::identity>(
        T && LookupRangeRef, Projection Proj = {}) constexpr
  requires std::is_trivially_copy_constructible_v<Projection> &&
           std::is_lvalue_reference_v<T>
{
  if constexpr (std::is_const_v<T>) {
    return [&LookupRangeRef, Proj]<typename IndexType>(
               const IndexType &Index) constexpr -> decltype(auto) {
      return LookupRangeRef[std::invoke(Proj, Index)];
    };
  } else {
    return [&LookupRangeRef, Proj]<typename IndexType>(
               const IndexType &Index) constexpr mutable -> decltype(auto) {
      return LookupRangeRef[std::invoke(Proj, Index)];
    };
  }
};

#endif

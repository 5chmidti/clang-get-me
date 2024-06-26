#ifndef get_me_lib_support_include_support_ranges_functional_hpp
#define get_me_lib_support_include_support_ranges_functional_hpp

#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

#include <range/v3/functional/comparisons.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/traits.hpp>

template <std::size_t I>
inline constexpr auto Element = []<typename T>
  requires(std::tuple_size_v<std::remove_cvref_t<T>> > I)
(T &&Tuple) -> decltype(auto) { return std::get<I>(std::forward<T>(Tuple)); };

constexpr auto Index = Element<0>;
constexpr auto Value = Element<1>;

template <std::size_t I, typename ResultType>
inline constexpr auto ConstrainedElement = []<typename T>
  requires(std::tuple_size_v<std::remove_cvref_t<T>> > I) &&
              std::same_as<std::remove_cvref_t<
                               std::invoke_result_t<decltype(std::get<I>), T>>,
                           ResultType>
(T &&Tuple) -> decltype(auto) { return std::get<I>(std::forward<T>(Tuple)); };

inline constexpr auto Copy = [](std::copyable auto Val) {
  return std::move(Val);
};

inline constexpr auto SafePlus = []<std::integral T>(const T Lhs,
                                                     const T Rhs) constexpr {
  if (const auto Mid = std::midpoint(Lhs, Rhs);
      Mid > std::midpoint(T{}, std::numeric_limits<T>::max())) {
    return std::numeric_limits<T>::max();
  }
  return std::plus<T>{}(Lhs, Rhs);
};

// add binary and unary support via binary facade or smt

inline constexpr auto Plus = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::plus<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Minus = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::minus<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Multiplies = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::multiplies<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Divides = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::divides<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Modulus = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::modulus<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Negate = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::negate<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto LogicalAnd = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::logical_and<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto LogicalOr = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::logical_or<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto LogicalNot = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::logical_not<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitAnd = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::bit_and<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitOr = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::bit_or<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitXor = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::bit_xor<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitNot = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>(const U &Lhs) constexpr {
    return std::bit_not<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto EqualTo = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>
    requires std::equality_comparable_with<U, T>
  (const U &Lhs) constexpr { return ranges::equal_to{}(Lhs, Rhs); };
};

inline constexpr auto NotEqualTo = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>
    requires std::equality_comparable_with<U, T>
  (const U &Lhs) constexpr { return ranges::not_equal_to{}(Lhs, Rhs); };
};

inline constexpr auto Greater = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>
    requires std::totally_ordered_with<U, T>
  (const U &Lhs) constexpr { return ranges::greater{}(Lhs, Rhs); };
};

inline constexpr auto Less = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>
    requires std::totally_ordered_with<U, T>
  (const U &Lhs) constexpr { return ranges::less{}(Lhs, Rhs); };
};

inline constexpr auto GreaterEqual = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>
    requires std::totally_ordered_with<U, T>
  (const U &Lhs) constexpr { return ranges::greater_equal{}(Lhs, Rhs); };
};

inline constexpr auto LessEqual = []<typename T>(T Rhs) constexpr {
  return [Rhs]<typename U>
    requires std::totally_ordered_with<U, T>
  (const U &Lhs) constexpr { return ranges::less_equal{}(Lhs, Rhs); };
};

template <typename T>
inline constexpr auto Construct = []<typename... ValueType>(ValueType &&...Val)
  requires std::constructible_from<T, ValueType...>
{ return T{std::forward<ValueType>(Val)...}; };

template <typename RangeType, typename Projection, typename IndexType>
concept integral_index_subscriptable =
    std::ranges::random_access_range<RangeType> &&
    std::regular_invocable<Projection, IndexType> &&
    std::integral<
        std::remove_cvref_t<std::invoke_result_t<Projection, IndexType>>> &&
    requires(RangeType Range, Projection Proj, IndexType Idx) {
      {
        Range[std::invoke(Proj, Idx)]
      } -> std::same_as<ranges::range_reference_t<RangeType>>;
    };

inline constexpr auto Lookup =
    []<ranges::random_access_range T, typename Projection = std::identity>(
        T &&LookupRangeRef, Projection Proj = {}) constexpr
  requires std::is_trivially_copy_constructible_v<Projection> &&
           ranges::borrowed_range<T>
{
  if constexpr (std::is_const_v<T>) {
    return [&LookupRangeRef, Proj]<typename IndexType>
      requires integral_index_subscriptable<T, Projection, IndexType>
    (const IndexType &Idx) constexpr -> decltype(auto) {
      return LookupRangeRef[std::invoke(Proj, Idx)];
    };
  } else {
    return [&LookupRangeRef, Proj]<typename IndexType>
      requires integral_index_subscriptable<T, Projection, IndexType>
    (const IndexType &Idx) constexpr mutable -> decltype(auto) {
      return LookupRangeRef[std::invoke(Proj, Idx)];
    };
  }
};

#endif

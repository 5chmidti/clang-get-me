#ifndef get_me_lib_support_include_support_ranges_functional_hpp
#define get_me_lib_support_include_support_ranges_functional_hpp

#include <functional>
#include <type_traits>
#include <utility>

#include <range/v3/functional/comparisons.hpp>
#include <range/v3/range/concepts.hpp>

inline constexpr auto Plus = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::plus<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Minus = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::minus<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Multiplies = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::multiplies<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Divides = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::divides<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Modulus = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::modulus<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto Negate = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::negate<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto LogicalAnd = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::logical_and<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto LogicalOr = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::logical_or<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto LogicalNot = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::logical_not<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitAnd = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::bit_and<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitOr = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::bit_or<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitXor = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::bit_xor<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto BitNot = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>(const U &Lhs) constexpr {
    return std::bit_not<T>{}(Lhs, Rhs);
  };
};

inline constexpr auto EqualTo =
    []<std::equality_comparable T>(T Rhs) constexpr {
      return [Rhs]<std::regular U>
        requires std::equality_comparable_with<U, T>(const U &Lhs)
      constexpr {
        return ranges::equal_to{}(Lhs, Rhs);
      };
    };

inline constexpr auto NotEqualTo = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>
    requires std::equality_comparable_with<U, T>(const U &Lhs)
  constexpr {
    return ranges::not_equal_to{}(Lhs, Rhs);
  };
};

inline constexpr auto Greater = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>
    requires std::totally_ordered_with<U, T>(const U &Lhs)
  constexpr {
    return ranges::greater{}(Lhs, Rhs);
  };
};

inline constexpr auto Less = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>
    requires std::totally_ordered_with<U, T>(const U &Lhs)
  constexpr {
    return ranges::less{}(Lhs, Rhs);
  };
};

inline constexpr auto GreaterEqual = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>
    requires std::totally_ordered_with<U, T>(const U &Lhs)
  constexpr {
    return ranges::greater_equal{}(Lhs, Rhs);
  };
};

inline constexpr auto LessEqual = []<std::regular T>(T Rhs) constexpr {
  return [Rhs]<std::regular U>
    requires std::totally_ordered_with<U, T>(const U &Lhs)
  constexpr {
    return ranges::less_equal{}(Lhs, Rhs);
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
    return [&LookupRangeRef, Proj ]<typename IndexType>
      requires std::regular_invocable<Projection, IndexType>(
                   const IndexType &Index)
    constexpr->decltype(auto) {
      return LookupRangeRef[std::invoke(Proj, Index)];
    };
  } else {
    return [&LookupRangeRef, Proj ]<typename IndexType>
      requires std::regular_invocable<Projection, IndexType>(
                   const IndexType &Index)
    constexpr mutable->decltype(auto) {
      return LookupRangeRef[std::invoke(Proj, Index)];
    };
  }
};

#endif

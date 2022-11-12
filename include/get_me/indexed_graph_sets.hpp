#ifndef get_me_include_get_me_indexed_graph_sets_hpp
#define get_me_include_get_me_indexed_graph_sets_hpp

#include <range/v3/action/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/type_set.hpp"
#include "get_me/utility.hpp"

template <typename ValueType>
using indexed_value_type = std::pair<size_t, ValueType>;

constexpr auto Value = Element<1>;
constexpr auto Index = Element<0>;

template <typename ValueType> struct IndexedSetComparator {
  using is_transparent = void;

  [[nodiscard]] bool
  operator()(const indexed_value_type<ValueType> &Lhs,
             const indexed_value_type<ValueType> &Rhs) const {
    if (Value(Lhs) < Value(Rhs)) {
      return true;
    }
    return Index(Lhs) < Index(Rhs);
  }
  [[nodiscard]] bool operator()(const indexed_value_type<ValueType> &Lhs,
                                const ValueType &Rhs) const {
    return Value(Lhs) < Rhs;
  }
  [[nodiscard]] bool
  operator()(const ValueType &Lhs,
             const indexed_value_type<ValueType> &Rhs) const {
    return Lhs < Value(Rhs);
  }
};

template <typename ValueType>
using indexed_set =
    std::set<indexed_value_type<ValueType>, IndexedSetComparator<ValueType>>;

template <ranges::range RangeType>
[[nodiscard]] auto getIndexedSetSortedByIndex(RangeType &&Range) {
  auto Sorted = std::forward<RangeType>(Range) | ranges::to_vector |
                ranges::actions::sort(std::less{}, Index);
  return Sorted | ranges::views::move | ranges::views::transform(Value) |
         ranges::to_vector;
}

#endif
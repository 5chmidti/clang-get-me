#ifndef get_me_lib_get_me_include_get_me_indexed_set_hpp
#define get_me_lib_get_me_include_get_me_indexed_set_hpp

#include <cstddef>
#include <functional>
#include <set>
#include <utility>

#include <range/v3/action/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include "support/ranges/functional.hpp"

template <typename ValueType>
using indexed_value = std::pair<size_t, ValueType>;

template <typename ValueType>
  requires std::relation<std::less<>, ValueType, ValueType>
struct IndexedSetComparator {
  using is_transparent = void;
  using indexed_value_type = indexed_value<ValueType>;

  [[nodiscard]] bool operator()(const indexed_value_type &Lhs,
                                const indexed_value_type &Rhs) const {
    if (Value(Lhs) < Value(Rhs)) {
      return true;
    }
    return Index(Lhs) < Index(Rhs);
  }
  [[nodiscard]] bool operator()(const indexed_value_type &Lhs,
                                const ValueType &Rhs) const {
    return Value(Lhs) < Rhs;
  }
  [[nodiscard]] bool operator()(const ValueType &Lhs,
                                const indexed_value_type &Rhs) const {
    return Lhs < Value(Rhs);
  }
};

template <typename Comparator>
concept indexed_set_comparator =
    std::predicate<Comparator, typename Comparator::indexed_value_type,
                   typename Comparator::indexed_value_type> &&
    std::predicate<Comparator, typename Comparator::indexed_value_type,
                   typename Comparator::indexed_value_type::second_type> &&
    std::predicate<Comparator,
                   typename Comparator::indexed_value_type::second_type,
                   typename Comparator::indexed_value_type>;

template <typename ValueType,
          indexed_set_comparator Comparator = IndexedSetComparator<ValueType>>
using indexed_set = std::set<indexed_value<ValueType>, Comparator>;

template <ranges::range RangeType>
[[nodiscard]] auto getIndexedSetSortedByIndex(RangeType &&Range) {
  auto Sorted = std::forward<RangeType>(Range) | ranges::to_vector |
                ranges::actions::sort(std::less{}, Index);
  return Sorted | ranges::views::move | ranges::views::transform(Value) |
         ranges::to_vector;
}

#endif

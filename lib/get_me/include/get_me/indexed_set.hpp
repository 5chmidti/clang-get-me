#ifndef get_me_lib_get_me_include_get_me_indexed_set_hpp
#define get_me_lib_get_me_include_get_me_indexed_set_hpp

#include <compare>
#include <concepts>
#include <cstddef>
#include <functional>
#include <set>
#include <utility>

#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/equal.hpp>
#include <range/v3/algorithm/lexicographical_compare.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/range/traits.hpp>
#include <range/v3/view/indices.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>

#include "support/get_me_exception.hpp"
#include "support/ranges/functional.hpp"

template <typename ValueType>
using indexed_value = std::pair<size_t, ValueType>;

struct IndexedSetComparator {
  using is_transparent = void;

  template <std::three_way_comparable ValueType>
  [[nodiscard]] bool operator()(const indexed_value<ValueType> &Lhs,
                                const indexed_value<ValueType> &Rhs) const {
    using ordering = std::compare_three_way_result_t<ValueType>;
    const auto Cmp = Value(Lhs) <=> Value(Rhs);
    if (Cmp == ordering::less) {
      return true;
    }
    if (Cmp == ordering::equal) {
      return Index(Lhs) < Index(Rhs);
    }
    return false;
  }

  template <std::three_way_comparable ValueType>
  [[nodiscard]] bool operator()(const indexed_value<ValueType> &Lhs,
                                const ValueType &Rhs) const {
    return Value(Lhs) < Rhs;
  }

  template <std::three_way_comparable ValueType>
  [[nodiscard]] bool operator()(const ValueType &Lhs,
                                const indexed_value<ValueType> &Rhs) const {
    return Lhs < Value(Rhs);
  }

  template <ranges::range ValueType>
    requires std::three_way_comparable<ranges::range_value_t<ValueType>>
  [[nodiscard]] bool operator()(const indexed_value<ValueType> &Lhs,
                                const indexed_value<ValueType> &Rhs) const {
    using ordering =
        std::compare_three_way_result_t<ranges::range_value_t<ValueType>>;
    const auto Cmp = std::lexicographical_compare_three_way(
        ranges::begin(Value(Lhs)), ranges::end(Value(Lhs)),
        ranges::begin(Value(Rhs)), ranges::end(Value(Rhs)));
    if (Cmp == ordering::less) {
      return true;
    }
    if (Cmp == ordering::equal) {
      return Index(Lhs) < Index(Rhs);
    }
    return false;
  }
  template <ranges::range ValueType>
    requires std::three_way_comparable<ranges::range_value_t<ValueType>>
  [[nodiscard]] bool operator()(const indexed_value<ValueType> &Lhs,
                                const ValueType &Rhs) const {
    return Value(Lhs) < Rhs;
  }
  template <ranges::range ValueType>
    requires std::three_way_comparable<ranges::range_value_t<ValueType>>
  [[nodiscard]] bool operator()(const ValueType &Lhs,
                                const indexed_value<ValueType> &Rhs) const {
    return Lhs < Value(Rhs);
  }
};

template <typename Comparator, typename ValueType>
concept indexed_set_comparator =
    std::predicate<Comparator, indexed_value<ValueType>,
                   indexed_value<ValueType>> &&
    std::predicate<Comparator, indexed_value<ValueType>, ValueType> &&
    std::predicate<Comparator, ValueType, indexed_value<ValueType>>;

template <typename ValueType,
          indexed_set_comparator<ValueType> Comparator = IndexedSetComparator>
using indexed_set = std::set<indexed_value<ValueType>, Comparator>;

template <ranges::range RangeType>
[[nodiscard]] auto getIndexedSetSortedByIndex(RangeType &&Range) {
  auto Sorted = std::forward<RangeType>(Range) | ranges::to_vector |
                ranges::actions::sort(std::less{}, Index);
  GetMeException::verify(
      ranges::equal(ranges::views::indices(ranges::size(Sorted)),
                    Sorted | ranges::views::transform(Index)),
      "getIndexedSetSortedByIndex received an input that did not contain "
      "indexed values with unique indices sorted and with difference 1");
  return Sorted | ranges::views::move | ranges::views::values |
         ranges::to_vector;
}

#endif

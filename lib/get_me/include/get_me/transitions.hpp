#ifndef get_me_lib_get_me_include_get_me_transitions_hpp
#define get_me_lib_get_me_include_get_me_transitions_hpp

#include <cstddef>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <fmt/core.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/functional/compose.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/indexed_set.hpp"
#include "get_me/type_conversion_map.hpp"
#include "get_me/type_set.hpp"
#include "support/ranges/functional.hpp"

using TransitionDataType =
    std::variant<const clang::FunctionDecl *, const clang::FieldDecl *,
                 const clang::VarDecl *>;

[[nodiscard]] std::string getTransitionName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionAcquiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionRequiredTypeNames(const TransitionDataType &Data);

template <> class fmt::formatter<TransitionDataType> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &Ctx) {
    return Ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(const TransitionDataType &Val,
                                                format_context &Ctx) const {
    return fmt::format_to(
        Ctx.out(), "{} {}({})", getTransitionAcquiredTypeNames(Val),
        getTransitionName(Val), getTransitionRequiredTypeNames(Val));
  }
};

using StrippedTransitionType = indexed_value<TransitionDataType>;

using StrippedTransitionsSet =
    boost::container::flat_set<StrippedTransitionType>;

using TransitionMap =
    boost::container::flat_map<std::pair<TypeSetValueType, TypeSet>,
                               indexed_value<StrippedTransitionsSet>>;

using TransitionType = TransitionMap::value_type;
using BundeledTransitionType =
    std::pair<std::pair<TypeSetValueType, TypeSet>, StrippedTransitionsSet>;

using FlatTransitionType =
    std::tuple<TypeSetValueType, TransitionDataType, TypeSet>;

struct TransitionData {
  using associative_container_type = TransitionMap;
  using value_type = associative_container_type::value_type;
  using bundeled_container_type = std::vector<BundeledTransitionType>;
  using flat_container_type = std::vector<FlatTransitionType>;

  void commit();

  associative_container_type Data{};
  bundeled_container_type BundeledData{};
  flat_container_type FlatData{};
  TypeConversionMap ConversionMap{};
};

namespace detail {
struct ToAcquiredFn {
  [[nodiscard]] constexpr TypeSetValueType &&
  operator()(TransitionType &&Val) const {
    return std::move(Val).first.first;
  }
  [[nodiscard]] constexpr const TypeSetValueType &
  operator()(const TransitionType &Val) const {
    return Val.first.first;
  }
  [[nodiscard]] constexpr TypeSetValueType &
  operator()(TransitionType &Val) const {
    return Val.first.first;
  }

  [[nodiscard]] constexpr TypeSetValueType &&
  operator()(TransitionType::first_type &&Val) const {
    return std::move(Val).first;
  }
  [[nodiscard]] constexpr const TypeSetValueType &
  operator()(const TransitionType::first_type &Val) const {
    return Val.first;
  }
  [[nodiscard]] constexpr TypeSetValueType &
  operator()(TransitionType::first_type &Val) const {
    return Val.first;
  }

  [[nodiscard]] constexpr TypeSetValueType &&
  operator()(BundeledTransitionType &&Val) const {
    return std::move(Val).first.first;
  }
  [[nodiscard]] constexpr const TypeSetValueType &
  operator()(const BundeledTransitionType &Val) const {
    return Val.first.first;
  }
  [[nodiscard]] constexpr TypeSetValueType &
  operator()(BundeledTransitionType &Val) const {
    return Val.first.first;
  }

  [[nodiscard]] constexpr TypeSetValueType &&
  operator()(FlatTransitionType &&Val) const {
    return Element<0>(std::move(Val));
  }
  [[nodiscard]] constexpr const TypeSetValueType &
  operator()(const FlatTransitionType &Val) const {
    return Element<0>(Val);
  }
  [[nodiscard]] constexpr TypeSetValueType &
  operator()(FlatTransitionType &Val) const {
    return Element<0>(Val);
  }
};

struct ToRequiredFn {
  [[nodiscard]] constexpr TypeSet &&operator()(TransitionType &&Val) const {
    return std::move(Val).first.second;
  }
  [[nodiscard]] constexpr const TypeSet &
  operator()(const TransitionType &Val) const {
    return Val.first.second;
  }
  [[nodiscard]] constexpr TypeSet &operator()(TransitionType &Val) const {
    return Val.first.second;
  }

  [[nodiscard]] constexpr TypeSet &&
  operator()(TransitionType::first_type &&Val) const {
    return std::move(Val).second;
  }
  [[nodiscard]] constexpr const TypeSet &
  operator()(const TransitionType::first_type &Val) const {
    return Val.second;
  }
  [[nodiscard]] constexpr TypeSet &
  operator()(TransitionType::first_type &Val) const {
    return Val.second;
  }

  [[nodiscard]] constexpr TypeSet &&
  operator()(BundeledTransitionType &&Val) const {
    return std::move(Val).first.second;
  }
  [[nodiscard]] constexpr const TypeSet &
  operator()(const BundeledTransitionType &Val) const {
    return Val.first.second;
  }
  [[nodiscard]] constexpr TypeSet &
  operator()(BundeledTransitionType &Val) const {
    return Val.first.second;
  }

  [[nodiscard]] constexpr TypeSet &&operator()(FlatTransitionType &&Val) const {
    return Element<2>(std::move(Val));
  }
  [[nodiscard]] constexpr const TypeSet &
  operator()(const FlatTransitionType &Val) const {
    return Element<2>(Val);
  }
  [[nodiscard]] constexpr TypeSet &operator()(FlatTransitionType &Val) const {
    return Element<2>(Val);
  }
};

struct ToTransitionsFn {
  [[nodiscard]] constexpr StrippedTransitionsSet &&
  operator()(TransitionType &&Val) const {
    return Value(std::move(Val).second);
  }
  [[nodiscard]] constexpr const StrippedTransitionsSet &
  operator()(const TransitionType &Val) const {
    return Value(Val.second);
  }
  [[nodiscard]] constexpr StrippedTransitionsSet &
  operator()(TransitionType &Val) const {
    return Value(Val.second);
  }

  [[nodiscard]] constexpr StrippedTransitionsSet &&
  operator()(BundeledTransitionType &&Val) const {
    return Element<1>(std::move(Val));
  }
  [[nodiscard]] constexpr const StrippedTransitionsSet &
  operator()(const BundeledTransitionType &Val) const {
    return Element<1>(Val);
  }
  [[nodiscard]] constexpr StrippedTransitionsSet &
  operator()(BundeledTransitionType &Val) const {
    return Element<1>(Val);
  }
};

struct ToTransitionFn {
  [[nodiscard]] constexpr TransitionDataType &&
  operator()(StrippedTransitionType &&Val) const {
    return Value(std::move(Val));
  }
  [[nodiscard]] constexpr const TransitionDataType &
  operator()(const StrippedTransitionType &Val) const {
    return Value(Val);
  }
  [[nodiscard]] constexpr TransitionDataType &
  operator()(StrippedTransitionType &Val) const {
    return Value(Val);
  }
  [[nodiscard]] constexpr TransitionDataType &&
  operator()(FlatTransitionType &&Val) const {
    return Element<1>(std::move(Val));
  }
  [[nodiscard]] constexpr const TransitionDataType &
  operator()(const FlatTransitionType &Val) const {
    return Element<1>(Val);
  }
  [[nodiscard]] constexpr TransitionDataType &
  operator()(FlatTransitionType &Val) const {
    return Element<1>(Val);
  }
};

struct ToBundeledTransitionIndexFn {
  [[nodiscard]] constexpr size_t &&operator()(TransitionType &&Val) const {
    return Index(std::move(Val).second);
  }
  [[nodiscard]] constexpr const size_t &
  operator()(const TransitionType &Val) const {
    return Index(Val.second);
  }
  [[nodiscard]] constexpr size_t &operator()(TransitionType &Val) const {
    return Index(Val.second);
  }
};
struct ToTransitionIndexFn {
  [[nodiscard]] constexpr size_t &&
  operator()(StrippedTransitionType &&Val) const {
    return Index(std::move(Val));
  }
  [[nodiscard]] constexpr const size_t &
  operator()(const StrippedTransitionType &Val) const {
    return Index(Val);
  }
  [[nodiscard]] constexpr size_t &
  operator()(StrippedTransitionType &Val) const {
    return Index(Val);
  }
};
} // namespace detail

inline constexpr detail::ToAcquiredFn ToAcquired{};
inline constexpr detail::ToRequiredFn ToRequired{};
inline constexpr detail::ToTransitionsFn ToTransitions{};
inline constexpr detail::ToTransitionFn ToTransition{};
inline constexpr detail::ToBundeledTransitionIndexFn
    ToBundeledTransitionIndex{};
inline constexpr detail::ToTransitionIndexFn ToTransitionIndex{};

[[nodiscard]] boost::container::flat_set<TransitionType>
getSmallestIndependentTransitions(const ranges::range auto &Transitions) {
  auto IndependentTransitions = boost::container::flat_set<TransitionType>{};
  auto Dependencies =
      Transitions |
      ranges::views::transform([&Transitions](const auto &Transition) {
        const auto DependsOn = [](const auto &Dependee) {
          return [&Dependee](const auto &Val) {
            return ToRequired(Val).contains(ToAcquired(Dependee));
          };
        };
        return std::pair{Transition,
                         Transitions |
                             ranges::views::filter(DependsOn(Transition)) |
                             ranges::to<boost::container::flat_set>};
      }) |
      ranges::to_vector |
      ranges::actions::sort(std::less<>{},
                            ranges::compose(ranges::size, Element<1>));

  ranges::for_each(
      Dependencies, [&IndependentTransitions](auto &DependenciesPair) {
        auto &[Transition, DependentsOfTransition] = DependenciesPair;
        if (ranges::empty(ranges::views::set_intersection(
                IndependentTransitions, DependentsOfTransition))) {
          IndependentTransitions.emplace(std::move(Transition));
        }
      });

  return IndependentTransitions;
}

#endif

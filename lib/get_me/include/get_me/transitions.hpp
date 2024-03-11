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
  // NOLINTBEGIN(readability-convert-member-functions-to-static)
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
  // NOLINTEND(readability-convert-member-functions-to-static)
};

using StrippedTransitionType = indexed_value<TransitionDataType>;

using StrippedTransitionsSet =
    boost::container::flat_set<StrippedTransitionType>;

using TransitionMap =
    boost::container::flat_map<std::pair<TransparentType, TypeSet>,
                               indexed_value<StrippedTransitionsSet>>;

using TransitionType = TransitionMap::value_type;
using BundeledTransitionType =
    std::pair<std::pair<TransparentType, TypeSet>, StrippedTransitionsSet>;

using FlatTransitionType =
    std::tuple<TransparentType, TransitionDataType, TypeSet>;

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
  [[nodiscard]] static constexpr TransparentType &&
  operator()(TransitionType &&Val) {
    return std::move(Val).first.first;
  }
  [[nodiscard]] static constexpr const TransparentType &
  operator()(const TransitionType &Val) {
    return Val.first.first;
  }
  [[nodiscard]] static constexpr TransparentType &
  operator()(TransitionType &Val) {
    return Val.first.first;
  }

  [[nodiscard]] static constexpr TransparentType &&
  operator()(TransitionType::first_type &&Val) {
    return std::move(Val).first;
  }
  [[nodiscard]] static constexpr const TransparentType &
  operator()(const TransitionType::first_type &Val) {
    return Val.first;
  }
  [[nodiscard]] static constexpr TransparentType &
  operator()(TransitionType::first_type &Val) {
    return Val.first;
  }

  [[nodiscard]] static constexpr TransparentType &&
  operator()(BundeledTransitionType &&Val) {
    return std::move(Val).first.first;
  }
  [[nodiscard]] static constexpr const TransparentType &
  operator()(const BundeledTransitionType &Val) {
    return Val.first.first;
  }
  [[nodiscard]] static constexpr TransparentType &
  operator()(BundeledTransitionType &Val) {
    return Val.first.first;
  }

  [[nodiscard]] static constexpr TransparentType &&
  operator()(FlatTransitionType &&Val) {
    return Element<0>(std::move(Val));
  }
  [[nodiscard]] static constexpr const TransparentType &
  operator()(const FlatTransitionType &Val) {
    return Element<0>(Val);
  }
  [[nodiscard]] static constexpr TransparentType &
  operator()(FlatTransitionType &Val) {
    return Element<0>(Val);
  }
};

struct ToRequiredFn {
  [[nodiscard]] static constexpr TypeSet &&operator()(TransitionType &&Val) {
    return std::move(Val).first.second;
  }
  [[nodiscard]] static constexpr const TypeSet &
  operator()(const TransitionType &Val) {
    return Val.first.second;
  }
  [[nodiscard]] static constexpr TypeSet &operator()(TransitionType &Val) {
    return Val.first.second;
  }

  [[nodiscard]] static constexpr TypeSet &&
  operator()(TransitionType::first_type &&Val) {
    return std::move(Val).second;
  }
  [[nodiscard]] static constexpr const TypeSet &
  operator()(const TransitionType::first_type &Val) {
    return Val.second;
  }
  [[nodiscard]] static constexpr TypeSet &
  operator()(TransitionType::first_type &Val) {
    return Val.second;
  }

  [[nodiscard]] static constexpr TypeSet &&
  operator()(BundeledTransitionType &&Val) {
    return std::move(Val).first.second;
  }
  [[nodiscard]] static constexpr const TypeSet &
  operator()(const BundeledTransitionType &Val) {
    return Val.first.second;
  }
  [[nodiscard]] static constexpr TypeSet &
  operator()(BundeledTransitionType &Val) {
    return Val.first.second;
  }

  [[nodiscard]] static constexpr TypeSet &&
  operator()(FlatTransitionType &&Val) {
    return Element<2>(std::move(Val));
  }
  [[nodiscard]] static constexpr const TypeSet &
  operator()(const FlatTransitionType &Val) {
    return Element<2>(Val);
  }
  [[nodiscard]] static constexpr TypeSet &operator()(FlatTransitionType &Val) {
    return Element<2>(Val);
  }
};

struct ToTransitionsFn {
  [[nodiscard]] static constexpr StrippedTransitionsSet &&
  operator()(TransitionType &&Val) {
    return Value(std::move(Val).second);
  }
  [[nodiscard]] static constexpr const StrippedTransitionsSet &
  operator()(const TransitionType &Val) {
    return Value(Val.second);
  }
  [[nodiscard]] static constexpr StrippedTransitionsSet &
  operator()(TransitionType &Val) {
    return Value(Val.second);
  }

  [[nodiscard]] static constexpr StrippedTransitionsSet &&
  operator()(BundeledTransitionType &&Val) {
    return Element<1>(std::move(Val));
  }
  [[nodiscard]] static constexpr const StrippedTransitionsSet &
  operator()(const BundeledTransitionType &Val) {
    return Element<1>(Val);
  }
  [[nodiscard]] static constexpr StrippedTransitionsSet &
  operator()(BundeledTransitionType &Val) {
    return Element<1>(Val);
  }
};

struct ToTransitionFn {
  [[nodiscard]] static constexpr TransitionDataType &&
  operator()(StrippedTransitionType &&Val) {
    return Value(std::move(Val));
  }
  [[nodiscard]] static constexpr const TransitionDataType &
  operator()(const StrippedTransitionType &Val) {
    return Value(Val);
  }
  [[nodiscard]] static constexpr TransitionDataType &
  operator()(StrippedTransitionType &Val) {
    return Value(Val);
  }
  [[nodiscard]] static constexpr TransitionDataType &&
  operator()(FlatTransitionType &&Val) {
    return Element<1>(std::move(Val));
  }
  [[nodiscard]] static constexpr const TransitionDataType &
  operator()(const FlatTransitionType &Val) {
    return Element<1>(Val);
  }
  [[nodiscard]] static constexpr TransitionDataType &
  operator()(FlatTransitionType &Val) {
    return Element<1>(Val);
  }
};

struct ToBundeledTransitionIndexFn {
  [[nodiscard]] static constexpr size_t &&operator()(TransitionType &&Val) {
    return Index(std::move(Val).second);
  }
  [[nodiscard]] static constexpr const size_t &
  operator()(const TransitionType &Val) {
    return Index(Val.second);
  }
  [[nodiscard]] static constexpr size_t &operator()(TransitionType &Val) {
    return Index(Val.second);
  }
};

struct ToTransitionIndexFn {
  [[nodiscard]] static constexpr size_t &&
  operator()(StrippedTransitionType &&Val) {
    return Index(std::move(Val));
  }
  [[nodiscard]] static constexpr const size_t &
  operator()(const StrippedTransitionType &Val) {
    return Index(Val);
  }
  [[nodiscard]] static constexpr size_t &
  operator()(StrippedTransitionType &Val) {
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
  const auto Dependencies =
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

// front action -> optional<value_type>
// front_or action -> value_type

#include <optional>

#include <range/v3/action/action.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/range/access.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/range/traits.hpp>

namespace actions {
struct FrontOrFn {
  template <ranges::forward_range RangeType>
  [[nodiscard]] constexpr ranges::range_value_t<RangeType>
  operator()(RangeType &&Range,
             ranges::range_value_t<RangeType> Default) const {
    if (ranges::empty(Range)) {
      return Default;
    }
    return *ranges::begin(Range);
  }

  [[nodiscard]] constexpr auto operator()(auto Default) const {
    return ranges::make_action_closure(
        ranges::bind_back(FrontOrFn{}, std::move(Default)));
  }
};

struct FrontOptFn {
  template <ranges::forward_range RangeType>
  [[nodiscard]] constexpr std::optional<ranges::range_value_t<RangeType>>
  operator()(RangeType &&Range) const {
    if (ranges::empty(Range)) {
      return std::nullopt;
    }
    return *ranges::begin(Range);
  }

  [[nodiscard]] constexpr auto operator()() const {
    return ranges::make_action_closure(FrontOptFn{});
  }
};

inline constexpr ranges::actions::action_closure<actions::FrontOrFn> FrontOr{};
inline constexpr ranges::actions::action_closure<actions::FrontOptFn>
    FrontOpt{};
} // namespace actions

#ifndef get_me_include_support_ranges_views_conditional_hpp
#define get_me_include_support_ranges_views_conditional_hpp

#include <range/v3/functional/bind_back.hpp>
#include <range/v3/view/facade.hpp>
#include <range/v3/view/view.hpp>

template <ranges::viewable_range Rng>
class ConditionalView : public ranges::view_facade<ConditionalView<Rng>> {
public:
  using value_type = ranges::range_value_t<Rng>;

  constexpr ConditionalView() = default;
  constexpr ConditionalView(Rng Range, bool Flag)
      : flag_(Flag),
        Cursor_(ranges::begin(Range)),
        Sentinel_(ranges::end(Range)) {}

private:
  friend ranges::range_access;

  [[nodiscard]] constexpr value_type read() const { return *Cursor_; }

  constexpr void next() { ++Cursor_; }

  [[nodiscard]] constexpr bool
  equal(ranges::default_sentinel_t /*unused*/) const {
    return !flag_ || Cursor_ == Sentinel_;
  }

  bool flag_{};
  ranges::iterator_t<Rng> Cursor_{};
  ranges::iterator_t<Rng> Sentinel_{};
};

class ConditionalBaseFn {
public:
  template <ranges::viewable_range Rng>
  [[nodiscard]] constexpr auto operator()(Rng &&Range, bool Flag) const {
    return ConditionalView<Rng>{std::forward<Rng>(Range), Flag};
  }
};

class ConditionalFn : public ConditionalBaseFn {
public:
  using ConditionalBaseFn::operator();

  constexpr auto operator()(bool Flag) const {
    return ranges::make_view_closure(
        ranges::bind_back(ConditionalBaseFn{}, Flag));
  }
};

inline constexpr ConditionalFn Conditional{};

#endif

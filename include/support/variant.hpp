#ifndef get_me_include_support_variant_hpp
#define get_me_include_support_variant_hpp

template <class... Ts> struct Overloaded : public Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

#endif

#include "get_me/type_conversion_map.hpp"

#include <range/v3/algorithm/for_each.hpp>

void combine(TypeConversionMap &Lhs, TypeConversionMap &&Rhs) {
  ranges::for_each(std::move(Rhs), [&Lhs](auto &Conversion) {
    auto &[Key, ConversionSet] = Conversion;
    Lhs[Key].merge(ConversionSet);
  });
}

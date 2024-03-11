#include "get_me/propagate_type_aliasing.hpp"

#include <utility>
#include <vector>

#include <range/v3/action/sort.hpp>
#include <range/v3/functional/comparisons.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/type_conversion_map.hpp"
#include "get_me/type_set.hpp"
#include "support/ranges/functional.hpp"

void propagateTypeAliasing(
    TypeConversionMap &ConversionMap,
    const std::vector<TransparentType> &TypedefNameDecls) {
  auto Sorted = Copy(TypedefNameDecls) | ranges::actions::sort(ranges::less{});

  const auto EqualDesugaredTypes = [](const auto &Lhs, const auto &Rhs) {
    return Lhs.Desugared == Rhs.Desugared;
  };

  combine(ConversionMap,
          Sorted | ranges::views::chunk_by(EqualDesugaredTypes) |
              ranges::views::transform(
                  [](const auto &Group) -> std::pair<Type, TypeSet> {
                    const auto &KeyType = ranges::front(Group);
                    const auto &Desugared = KeyType.Desugared;
                    const auto DesugaredIdentity =
                        TransparentType{Desugared, Desugared};
                    return std::pair{
                        Desugared,
                        ranges::views::concat(
                            ranges::views::single(DesugaredIdentity), Group) |
                            ranges::to<TypeSet>};
                  }) |
              ranges::to<TypeConversionMap>);
}

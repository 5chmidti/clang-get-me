#include "get_me/propagate_type_conversions.hpp"

#include <functional>
#include <utility>
#include <variant>

#include <boost/container/flat_set.hpp>
#include <clang/AST/Type.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/functional/bind_back.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indirect.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/single.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/transitions.hpp"
#include "get_me/type_conversion_map.hpp"
#include "get_me/type_set.hpp"
#include "support/ranges/functional.hpp"
#include "support/variant.hpp"

namespace {
[[nodiscard]] Type stripPointerRef(const Type &SourceType) {
  const auto HandleQualType = [](const clang::QualType &QType) -> Type {
    if (QType->isReferenceType()) {
      return QType.getNonReferenceType();
    }
    if (QType->isPointerType()) {
      return QType->getPointeeType();
    }
    return QType;
  };
  const auto HandleDefault = [](const auto &T) -> Type { return T; };
  return std::visit(Overloaded{HandleQualType, HandleDefault}, SourceType);
}
[[nodiscard]] TransparentType
stripPointerRef(const TransparentType &SourceType) {
  return {.Desugared = stripPointerRef(SourceType.Desugared),
          .Actual = stripPointerRef(SourceType.Actual)};
}
} // namespace

void propagateTypeConversions(TransitionData &Transitions) {
  const auto AllTypes =
      ranges::views::concat(
          Transitions.Data | ranges::views::transform(ToAcquired),
          Transitions.Data | ranges::views::for_each(ToRequired)) |
      ranges::to_vector;
  const auto FlatPointerRefConversions =
      AllTypes | ranges::views::transform([](const TransparentType &Type) {
        return std::pair{stripPointerRef(Type), Type};
      }) |
      ranges::to_vector | ranges::actions::sort(std::less<>{}, Element<0>);

  const auto EqualDesugaredTypes = [](const auto &Lhs, const auto &Rhs) {
    return Element<0>(Lhs).Desugared == Element<0>(Rhs).Desugared;
  };
  const auto PointerRefConversions =
      FlatPointerRefConversions | ranges::views::chunk_by(EqualDesugaredTypes) |
      ranges::views::transform([](const auto &Group) {
        const auto KeyType = Element<0>(ranges::front(Group));
        return std::pair{KeyType.Desugared,
                         ranges::views::concat(
                             ranges::views::single(KeyType),
                             Group | ranges::views::transform(Element<1>)) |
                             ranges::to<TypeSet>};
      }) |
      ranges::views::values | ranges::to_vector;

  const auto PtrRefConversionSet =
      AllTypes |
      ranges::views::transform(
          [&PointerRefConversions](const TransparentType &Type) {
            return PointerRefConversions |
                   ranges::views::filter(
                       ranges::bind_back(ranges::contains, Type)) |
                   ranges::views::join | ranges::to<TypeSet>;
          }) |
      ranges::to<boost::container::flat_set>;

  auto &ConversionMap = Transitions.ConversionMap;

  combine(ConversionMap,
          PtrRefConversionSet |
              ranges::views::for_each([](const TypeSet &ConversionSet) {
                return ConversionSet |
                       ranges::views::transform(
                           [&ConversionSet](const TransparentType &Type) {
                             return TypeConversionMap::value_type{
                                 Type.Desugared, ConversionSet};
                           });
              }) |
              ranges::to<TypeConversionMap>);

  ranges::for_each(ConversionMap | ranges::views::values, [&ConversionMap](
                                                              TypeSet &Val) {
    Val.merge(Val | ranges::views::transform([](const TransparentType &TType) {
                const auto RemoveConst = Overloaded{
                    [](const clang::QualType &QType) -> Type {
                      auto NewType = QType;
                      NewType.removeLocalConst();
                      return NewType;
                    },
                    [](const auto &Default) -> Type { return Default; }};

                return TransparentType{
                    .Desugared = std::visit(RemoveConst, TType.Desugared),
                    .Actual = std::visit(RemoveConst, TType.Actual)};
              }) |
              ranges::to<TypeSet>);

    Val.merge(
        Val |
        ranges::views::transform(
            [&ConversionMap](const TypeSet::value_type &Type) {
              return ConversionMap.find(Type.Desugared);
            }) |
        ranges::views::filter(ranges::not_fn(EqualTo(ConversionMap.end()))) |
        ranges::views::indirect | ranges::views::values | ranges::views::join |
        ranges::to<TypeSet>);
  });
}

#include "get_me/propagate_type_conversions.hpp"

#include <variant>

#include <clang/AST/Type.h>
#include <fmt/ranges.h>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/operations.hpp>
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indirect.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>
#include <spdlog/spdlog.h>

#include "get_me/transitions.hpp"
#include "get_me/type_conversion_map.hpp"
#include "get_me/type_set.hpp"
#include "support/variant.hpp"

namespace {
[[nodiscard]] TypeSetValueType
stripPointerRef(const TypeSetValueType &SourceType) {
  const auto HandleQualType =
      [](const clang::QualType &Type) -> TypeSetValueType {
    if (Type->isReferenceType()) {
      return Type.getNonReferenceType();
    }
    if (Type->isPointerType()) {
      return Type->getPointeeType();
    }
    return Type;
  };
  const auto HandleDefault = [](const auto &Type) -> TypeSetValueType {
    return Type;
  };
  return std::visit(Overloaded{HandleQualType, HandleDefault}, SourceType);
}
} // namespace

void propagateTypeConversions(TransitionData &Transitions) {
  const auto AllTypes =
      ranges::views::concat(Transitions.Data | ranges::views::keys,
                            Transitions.Data | ranges::views::values |
                                ranges::views::join |
                                ranges::views::for_each(ToRequired)) |
      ranges::to_vector;
  const auto FlatPointerRefConversions =
      AllTypes | ranges::views::transform([](const TypeSetValueType &Type) {
        return std::pair{stripPointerRef(Type), Type};
      }) |
      ranges::to_vector | ranges::actions::sort(std::less<>{}, Element<0>);

  const auto PointerRefConversions =
      FlatPointerRefConversions |
      ranges::views::chunk_by([](const auto &Lhs, const auto &Rhs) {
        return Element<0>(Lhs) == Element<0>(Rhs);
      }) |
      ranges::views::transform([](const auto &Val) {
        const auto KeyType = Element<0>(ranges::front(Val));
        return std::pair{
            KeyType,
            ranges::views::concat(ranges::views::single(KeyType),
                                  Val | ranges::views::transform(Element<1>)) |
                ranges::to<TypeSet>};
      }) |
      ranges::views::values | ranges::to_vector;

  const auto PtrRefConversionSet =
      AllTypes |
      ranges::views::transform(
          [&PointerRefConversions](const TypeSetValueType &Type) {
            return PointerRefConversions |
                   ranges::views::filter(
                       ranges::bind_back(ranges::contains, Type)) |
                   ranges::views::join | ranges::to<TypeSet>;
          }) |
      ranges::to<boost::container::flat_set>;

  auto &ConversionMap = Transitions.ConversionMap;

  combine(
      ConversionMap,
      PtrRefConversionSet |
          ranges::views::for_each([](const TypeSet &ConversionSet) {
            return ConversionSet |
                   ranges::views::transform([&ConversionSet](
                                                const TypeSetValueType &Type) {
                     return TypeConversionMap::value_type{Type, ConversionSet};
                   });
          }) |
          ranges::to<TypeConversionMap>);

  ranges::for_each(ConversionMap | ranges::views::values, [&ConversionMap](
                                                              TypeSet &Val) {
    Val.merge(
        Val | ranges::views::transform([](const TypeSetValueType &Type) {
          return std::visit(
              Overloaded{[](const clang::QualType &QType) -> TypeSetValueType {
                           auto NewType = QType;
                           NewType.removeLocalConst();
                           return NewType;
                         },
                         [](const auto &Default) -> TypeSetValueType {
                           return Default;
                         }},
              Type);
        }) |
        ranges::to<TypeSet>);

    Val.merge(Val |
              ranges::views::transform([&ConversionMap](const auto &Type) {
                return ConversionMap.find(Type);
              }) |
              ranges::views::filter([&ConversionMap](const auto Iter) {
                return Iter != ConversionMap.end();
              }) |
              ranges::views::indirect | ranges::views::values |
              ranges::views::join | ranges::to<TypeSet>);
  });

  spdlog::error("PtrRefConversionSet : {}", PtrRefConversionSet);
  spdlog::error("PointerRefConversions : {}", PointerRefConversions);
  spdlog::error("Transitions.ConversionMap : {}", Transitions.ConversionMap);
  spdlog::error("Transitions.Data : {}", Transitions.Data);
}

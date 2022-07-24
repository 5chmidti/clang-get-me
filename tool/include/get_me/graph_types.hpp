#ifndef get_me_graph_types_hpp
#define get_me_graph_types_hpp

#include <variant>

#include <boost/graph/adjacency_list.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>

#include "get_me/graph.hpp"

namespace clang {
struct FunctionDecl;
struct FieldDecl;
} // namespace clang

using TransitionDataType =
    std::variant<const clang::FunctionDecl *, const clang::FieldDecl *>;
using TransitionSourceType = clang::Type;

using GraphType = boost::adjacency_list<
    boost::listS, boost::vecS, boost::directedS, boost::no_property,
    boost::property<boost::edge_weight_t, TransitionDataType>>;

struct TypeValue {
  using meta_type = std::variant<clang::QualType, const clang::NamedDecl *>;
  const clang::Type *Value;
  meta_type MetaValue;

  [[nodiscard]] friend auto operator==(const TypeValue &Lhs,
                                       const TypeValue &Rhs) {
    return Lhs.Value == Rhs.Value;
  }

  [[nodiscard]] friend auto operator<=>(const TypeValue &Lhs,
                                        const TypeValue &Rhs) {
    return Lhs.Value <=> Rhs.Value;
  }
};

using TypeSetValueType = TypeValue;
using TypeSet = std::set<TypeSetValueType>;

using GetMeGraphData =
    GraphData<GraphType, TransitionDataType, TypeSet, TransitionDataType>;

using weight_map_type =
    typename boost::property_map<GraphType, boost::edge_weight_t>::type;

// [[nodiscard]] inline TypeSetValueType
// toNamedDeclOrQualType(const clang::QualType &QType) {
//   if (const auto *RDecl = QType->getAsCXXRecordDecl()) {
//     return RDecl;
//   }
//   if (const auto *RDecl = QType->getAsRecordDecl()) {
//     return RDecl;
//   }
//   if (const auto *TDecl = QType->getAsTagDecl()) {
//     return TDecl;
//   }
//   return QType;
// }

#endif

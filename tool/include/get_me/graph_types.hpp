#ifndef get_me_graph_types_hpp
#define get_me_graph_types_hpp

#include <variant>

#include <boost/graph/adjacency_list.hpp>
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

using TypeSetValueType = std::variant<const clang::QualType *, clang::QualType,
                                      const clang::NamedDecl *>;
using TypeSet = std::set<TypeSetValueType>;

using GetMeGraphData =
    GraphData<GraphType, TransitionDataType, TypeSet, TransitionDataType>;

using weight_map_type =
    typename boost::property_map<GraphType, boost::edge_weight_t>::type;

#endif

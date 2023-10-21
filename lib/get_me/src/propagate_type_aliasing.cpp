#include "get_me/propagate_type_aliasing.hpp"

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/graph_selectors.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/access.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/primitives.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include "get_me/direct_type_dependency_propagation.hpp"
#include "get_me/graph.hpp"
#include "get_me/indexed_set.hpp"
#include "get_me/type_conversion_map.hpp"
#include "get_me/type_set.hpp"
#include "support/ranges/functional.hpp"

namespace {
using TypeAliasingGroup = TypeSet;
using TypeAliasingGroups = std::vector<TypeAliasingGroup>;

using TypeAliasingGraph =
    boost::adjacency_list<boost::listS, boost::vecS, boost::undirectedS>;

class TypeAliasingGraphBuilder {
public:
  void visit(const TypeAlias &Typedef) {
    addEdge(
        DTDGraphData::EdgeType{addType(Typedef.Alias), addType(Typedef.Base)});
  };

  [[nodiscard]] DTDGraphData getResult() {
    return {getIndexedSetSortedByIndex(std::move(Vertices_)),
            getIndexedSetSortedByIndex(std::move(Edges_))};
  }

private:
  [[nodiscard]] VertexDescriptor addType(const TypeSetValueType &Type) {
    const auto BaseTypeIter = Vertices_.find(Type);
    const auto BaseTypeExists = BaseTypeIter != Vertices_.end();
    const auto BaseVertexIndex =
        BaseTypeExists ? Index(*BaseTypeIter) : Vertices_.size();
    if (!BaseTypeExists) {
      Vertices_.emplace(BaseVertexIndex, Type);
    }
    return BaseVertexIndex;
  }

  void addEdge(const DTDGraphData::EdgeType &EdgeToAdd) {
    const auto EdgesIter = Edges_.lower_bound(EdgeToAdd);
    if (const auto ContainsEdgeToAdd =
            EdgesIter != ranges::end(Edges_) && Value(*EdgesIter) == EdgeToAdd;
        !ContainsEdgeToAdd) {
      Edges_.emplace_hint(EdgesIter, Edges_.size(), EdgeToAdd);
    }
  }

  indexed_set<TypeSetValueType> Vertices_{};
  indexed_set<DTDGraphData::EdgeType> Edges_{};
};

[[nodiscard]] TypeAliasingGraph createGraph(const DTDGraphData &Data) {
  return {Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
          Data.VertexData.size()};
}

[[nodiscard]] TypeAliasingGroups
createTypeAliasingGroups(const std::vector<TypeAlias> &TypedefNameDecls) {
  auto Builder = TypeAliasingGraphBuilder{};

  const auto Visitor = [&Builder](const auto &Value) { Builder.visit(Value); };

  ranges::for_each(TypedefNameDecls, Visitor);

  const auto GraphData = Builder.getResult();
  const auto Graph = createGraph(GraphData);

  auto ComponentMap = std::vector<size_t>(GraphData.VertexData.size());
  boost::connected_components(Graph, ComponentMap.data());

  const auto SortedComponentMap = ComponentMap | ranges::views::enumerate |
                                  ranges::to_vector |
                                  ranges::actions::sort(std::less{}, Value);
  const auto IsSameGroup = [](const auto &Lhs, const auto &Rhs) {
    return Value(Lhs) == Value(Rhs);
  };
  return SortedComponentMap | ranges::views::chunk_by(IsSameGroup) |
         ranges::views::filter(Greater(1), ranges::size) |
         ranges::views::transform(
             ranges::views::transform(Lookup(GraphData.VertexData, Index))) |
         ranges::to<TypeAliasingGroups>;
}
} // namespace

void propagateTypeAliasing(TypeConversionMap &ConversionMap,
                           const std::vector<TypeAlias> &TypedefNameDecls) {
  const auto Groups = createTypeAliasingGroups(TypedefNameDecls);

  combine(ConversionMap,
          Groups | ranges::views::for_each([](const auto &Group) {
            return ranges::views::zip(Group, ranges::views::repeat(Group));
          }) | ranges::to<TypeConversionMap>);
}

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
#include <range/v3/view/cache1.hpp>
#include <range/v3/view/chunk_by.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/for_each.hpp>
#include <range/v3/view/indirect.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/direct_type_dependency_propagation.hpp"
#include "get_me/graph.hpp"
#include "get_me/indexed_set.hpp"
#include "get_me/transitions.hpp"
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

[[nodiscard]] auto requiredContainsGroupMember(const TypeAliasingGroup &Group) {
  return [&Group](const TypeSet &Required) {
    return !ranges::empty(Group | ranges::views::set_intersection(Required));
  };
}

[[nodiscard]] StrippedTransitionsSet getTransitionsToPropagateForAcquired(
    const TransitionCollector::associative_container_type &Transitions,
    const TypeAliasingGroup &Group) {
  return Group |
         ranges::views::transform([&Transitions](const TypeSetValueType &Type) {
           return Transitions.find(Type);
         }) |
         ranges::views::cache1 |
         ranges::views::filter(NotEqualTo(Transitions.end())) |
         ranges::views::indirect | ranges::views::values | ranges::views::join |
         ranges::to<StrippedTransitionsSet>;
}

[[nodiscard]] auto propagateAcquiredTransitions(
    TransitionCollector::associative_container_type &Transitions) {
  return [&Transitions](const TypeAliasingGroup &Group) {
    const auto PairTypeWithGroupsTransitions =
        [TransitionsOfGroupMembers = getTransitionsToPropagateForAcquired(
             Transitions, Group)](const TypeSetValueType &Type) {
          return BundeledTransitionType{Type, TransitionsOfGroupMembers};
        };
    return Group | ranges::views::transform(PairTypeWithGroupsTransitions) |
           ranges::to<TransitionCollector::associative_container_type>;
  };
}

[[nodiscard]] auto
substituteGroupMembersFromRequired(const TypeAliasingGroup &Group) {
  return [&Group](const StrippedTransitionType &Transition) {
    return Group | ranges::views::transform(
                       [TransitionWithoutGroupMembers = StrippedTransitionType{
                            Index(Transition),
                            {ToTransition(Transition),
                             ToRequired(Transition) |
                                 ranges::views::set_difference(Group) |
                                 ranges::to<TypeSet>}}](const auto &Type) {
                         auto NewTransition = TransitionWithoutGroupMembers;
                         Value(NewTransition).second.emplace(Type);
                         return NewTransition;
                       });
  };
}

[[nodiscard]] auto propagateRequiredTransitions(
    TransitionCollector::associative_container_type &Transitions) {
  return [&Transitions](const TypeAliasingGroup &Group) {
    return Transitions |
           ranges::views::transform(
               [&Group](const BundeledTransitionType &BundeledTransitions) {
                 return BundeledTransitionType{
                     BundeledTransitions.first,
                     BundeledTransitions.second |
                         ranges::views::filter(
                             requiredContainsGroupMember(Group), ToRequired) |
                         ranges::views::for_each(
                             substituteGroupMembersFromRequired(Group)) |
                         ranges::to<StrippedTransitionsSet>};
               }) |
           ranges::to<TransitionCollector::associative_container_type>;
  };
}

void propgagateAcquiredTransitions(const TypeAliasingGroups &Groups,
                                   TransitionCollector &Transitions) {
  const auto TransitionsVector =
      Groups |
      ranges::views::transform(propagateAcquiredTransitions(Transitions.Data)) |
      ranges::to_vector;

  const auto PairWithExistingForAcquired =
      [&Transitions](const BundeledTransitionType &New) {
        return std::pair{std::ref(Transitions.Data[ToAcquired(New)]), New};
      };
  const auto MergeExistingWithNew = [](auto Pair) {
    auto &[Current, New] = Pair;
    Current.get().merge(New.second);
  };
  ranges::for_each(TransitionsVector | ranges::views::join |
                       ranges::views::transform(PairWithExistingForAcquired),
                   MergeExistingWithNew);
}

void propgagateRequiredTransitions(const TypeAliasingGroups &Groups,
                                   TransitionCollector &Transitions) {
  const auto TransitionsVector =
      Groups |
      ranges::views::transform(propagateRequiredTransitions(Transitions.Data)) |
      ranges::to_vector;

  const auto MergeExistingWithNew =
      [&Transitions](BundeledTransitionType Transition) {
        Transitions.Data[ToAcquired(Transition)].merge(Transition.second);
      };
  ranges::for_each(TransitionsVector | ranges::views::join |
                       ranges::views::move,
                   MergeExistingWithNew);
}
} // namespace

void propagateTypeAliasing(TransitionCollector &Transitions,
                           const std::vector<TypeAlias> &TypedefNameDecls) {
  const auto Groups = createTypeAliasingGroups(TypedefNameDecls);

  propgagateAcquiredTransitions(Groups, Transitions);
  propgagateRequiredTransitions(Groups, Transitions);
}

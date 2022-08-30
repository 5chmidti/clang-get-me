#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <type_traits>
#include <variant>

#include <boost/algorithm/string/predicate.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/Type.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>

#include "get_me/formatting.hpp"
#include "get_me/graph.hpp"
#include "get_me/tooling.hpp"
#include "get_me/utility.hpp"

// NOLINTBEGIN
using namespace ranges;
using namespace llvm::cl;

static OptionCategory ToolCategory("get_me");
static opt<std::string> TypeName("t", desc("Name of the type to get"), Required,
                                 ValueRequired, cat(ToolCategory));
// NOLINTEND

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  auto OptionsParser =
      clang::tooling::CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!OptionsParser) {
    llvm::errs() << OptionsParser.takeError();
    return 1;
  }
  const auto &Sources = OptionsParser->getSourcePathList();
  clang::tooling::ClangTool Tool(
      OptionsParser->getCompilations(),
      Sources.empty() ? OptionsParser->getCompilations().getAllFiles()
                      : Sources);

  TransitionCollector Collector{};
  std::vector<std::unique_ptr<clang::ASTUnit>> ASTs{};
  if (const auto BuildASTsResult = Tool.buildASTs(ASTs); BuildASTsResult != 0) {
    spdlog::error("error building ASTs");
    return 1;
  }
  auto Consumer = GetMe{Collector};
  for (const auto &AST : ASTs) {
    Consumer.HandleTranslationUnit(AST->getASTContext());
  }

  spdlog::info("{}", Collector.Data);
  // build graph from gathered graph data
  const auto TypeSetTransitionData = getTypeSetTransitionData(Collector);
  spdlog::info("TypeSetTransitionData: {}", TypeSetTransitionData);

  auto Data =
      generateVertexAndEdgeWeigths(TypeSetTransitionData, TypeName.getValue());

  // FIXME: apply greedy transition traversal strategy

  GraphType Graph(Data.Edges.data(), Data.Edges.data() + Data.Edges.size(),
                  Data.EdgeWeights.data(), Data.EdgeWeights.size());

  const auto &QueriedType = TypeName.getValue();

  // FIXME: improve queried type matching:
  // - better matching of names
  // - allow matching mutiple to get around QualType vs NamedDecl problem
  // - better: fix QualType vs NamedDecl problem
  // FIXME: only getting the 'A' type, not the & qualified
  const auto SourceVertex = find_if(Data.VertexData, [&QueriedType](
                                                         const TypeSet &TSet) {
    return TSet.end() !=
           find_if(
               TSet,
               [&QueriedType](
                   const typename TypeSetValueType::meta_type &MetaVal) {
                 return std::visit(
                     Overloaded{[&QueriedType](clang::QualType QType) {
                                  return includes(QType.getAsString(),
                                                  QueriedType);
                                },
                                [&QueriedType](const clang::NamedDecl *NDecl) {
                                  return NDecl->getName().contains(QueriedType);
                                },
                                [](std::monostate) { return false; }},
                     MetaVal);
               },
               &TypeSetValueType::MetaValue);
  });

  if (SourceVertex == Data.VertexData.end()) {
    spdlog::error("found no type matching {}", QueriedType);
    return 1;
  }
  const auto SourceVertexDesc =
      static_cast<size_t>(std::distance(Data.VertexData.begin(), SourceVertex));

  const auto Paths = pathTraversal(Graph, SourceVertexDesc);

  for (const auto [Path, Number] : views::zip(Paths, views::iota(0U))) {
    spdlog::info(
        "path #{}: {}", Number,
        fmt::join(
            Path | views::transform([&Graph, &Data](const auto &Transition) {
              const auto Edge = std::pair{source(Transition, Graph),
                                          target(Transition, Graph)};
              const auto Weight = Data.EdgeWeightMap.at(Edge);
              const auto TargetName = getTransitionTargetTypeName(Weight);
              const auto SourceName = getTransitionSourceTypeName(Weight);
              return Weight;
            }),
            ", "));
  }

  std::ofstream DotFile("graph.dot");
  std::string Res{};
  Res += fmt::format("digraph D {{\n");

  for (const auto &Edge : toRange(edges(Graph))) {
    const auto SourceNode = source(Edge, Graph);
    const auto TargetNode = target(Edge, Graph);
    const auto EdgeWeight = Data.EdgeWeightMap.at({SourceNode, TargetNode});
    const auto TargetName = getTransitionTargetTypeName(EdgeWeight);
    const auto SourceName = getTransitionSourceTypeName(EdgeWeight);
    Res += fmt::format(
        R"(  "{}" -> "{}"[label="{}"]
)",
        SourceName, TargetName, EdgeWeight);
  }

  Res += fmt::format("}}");
  DotFile << Res;
}

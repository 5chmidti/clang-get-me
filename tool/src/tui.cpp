#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <ctre.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <ftxui/component/animation.hpp>
#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/deprecated.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <range/v3/action/erase.hpp>
#include <range/v3/action/insert.hpp>
#include <range/v3/action/join.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/move.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <rapidfuzz/fuzz.hpp>

#include "get_me/config.hpp"
#include "get_me/formatting.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/tooling.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "tui/components/config_editor.hpp"
#include "tui/components/paths_menu.hpp"
#include "tui/components/query.hpp"

class TransitionCollectionState {
public:
  TransitionCollectionState(Config &Conf, clang::tooling::ClangTool &Tool,
                            std::shared_ptr<TransitionCollector> Transitions =
                                std::make_shared<TransitionCollector>())
      : Conf_(Conf),
        Tool_(Tool),
        Transitions_{std::move(Transitions)} {}

  void update() {
    collectTransitions();

    AcquiredTypeNames_ =
        *Transitions_ | ranges::views::transform(ToAcquired) |
        ranges::views::transform([](const TypeSetValueType Acquired) {
          return fmt::format("{}", Acquired);
        }) |
        ranges::to_vector;
    AcquiredTypeNames_.emplace_back("asdf");
  }

  [[nodiscard]] std::vector<std::string> &getAqcuiredTypeNames() {
    return AcquiredTypeNames_;
  }

  [[nodiscard]] const std::vector<std::string> &getAqcuiredTypeNames() const {
    return AcquiredTypeNames_;
  }

  [[nodiscard]] std::shared_ptr<TransitionCollector> &getTransitionsPtr() {
    return Transitions_;
  }
  [[nodiscard]] TransitionCollector &getTransitions() { return *Transitions_; }

  [[nodiscard]] const TransitionCollector &getTransitions() const {
    return *Transitions_;
  }

private:
  void collectTransitions() {
    Transitions_->clear();

    const auto BuildASTsResult = Tool_.buildASTs(ASTs_);
    GetMeException::verify(BuildASTsResult == 0, "Error building ASTs");

    for (const auto &AST : ASTs_) {
      GetMe{Conf_, *Transitions_, AST->getSema()}.HandleTranslationUnit(
          AST->getASTContext());
    }
  }

  Config &Conf_;
  clang::tooling::ClangTool &Tool_;
  std::vector<std::unique_ptr<clang::ASTUnit>> ASTs_;
  std::shared_ptr<TransitionCollector> Transitions_{};
  std::vector<std::string> AcquiredTypeNames_{};
};

void runTui(Config &Conf, clang::tooling::ClangTool &Tool) {
  auto CollectionState = TransitionCollectionState{Conf, Tool};

  auto Screen = ftxui::ScreenInteractive::Fullscreen();

  auto TabSelection = 0;

  std::string QueriedName{};
  std::vector<std::string> PathsStr{};
  // FIXME: this early init runs into an error state bc transitions is not yet
  // initialized (aka filled)
  const auto CommitCallback = [&Conf, &PathsStr, &CollectionState,
                               &QueriedName]() {
    const auto Query =
        QueryType{CollectionState.getTransitionsPtr(), QueriedName, Conf};
    // workaround for lmdba captures in clang
    const auto GraphAndData = createGraph(Query);
    const auto &Graph = GraphAndData.first;
    const auto &Data = GraphAndData.second;

    const auto SourceVertexDesc =
        getSourceVertexMatchingQueriedType(Data, Query.getQueriedType());
    const auto Paths =
        pathTraversal(Graph, Data, Conf, SourceVertexDesc) |
        ranges::actions::sort([&Data](const PathType &Lhs,
                                      const PathType &Rhs) {
          if (const auto Comp = Lhs.size() <=> Rhs.size(); std::is_neq(Comp)) {
            return std::is_lt(Comp);
          }
          if (Lhs.empty()) {
            return true;
          }
          return Data.VertexData[Target(Lhs.back())].size() <
                 Data.VertexData[Target(Rhs.back())].size();
        });
    const auto IndexMap = boost::get(boost::edge_index, Graph);
    PathsStr =
        Paths | ranges::views::enumerate |
        ranges::views::transform([&Data, &IndexMap](const auto IndexedPath) {
          const auto &[Number, Path] = IndexedPath;
          return fmt::format(
              "path #{}: {} -> remaining: {}", Number,
              fmt::join(
                  Path |
                      ranges::views::transform(
                          [&Data, &IndexMap](const EdgeDescriptor &Edge) {
                            return toString(
                                Data.EdgeWeights[boost::get(IndexMap, Edge)]);
                          }),
                  ", "),
              Data.VertexData[Target(Path.back())]);
        }) |
        ranges::to_vector;
  };

  const auto TabNames = std::vector<std::string>{"Config", "Query", "Paths"};
  const auto TabToggle = ftxui::Toggle(&TabNames, &TabSelection);
  const auto Tabs = ftxui::Container::Tab(
      {
          buildConfigComponent(&Conf),
          buildQueryComponent(&QueriedName,
                              &CollectionState.getAqcuiredTypeNames(),
                              CommitCallback),
          buildPathsComponent(&PathsStr),
      },
      &TabSelection);
  const auto ComponentsContainer = ftxui::Container::Vertical({
      TabToggle,
      Tabs,
  });
  const auto EventCallback = [&Screen, &CollectionState](ftxui::Event Event) {
    if (Event.character() == "q") {
      Screen.ExitLoopClosure()();
      return true;
    }
    if (Event.character() == "c") {
      CollectionState.update();
    }
    return false;
  };
  Screen.Loop(
      ftxui::Renderer(
          ComponentsContainer,
          [TabToggle, Tabs] {
            return ftxui::vbox(
                TabToggle->Render(), ftxui::separator(), Tabs->Render(),
                ftxui::filler(), ftxui::separator(),
                ftxui::hbox(ftxui::text("[c] collect transitions"),
                            ftxui::separator(), ftxui::text("[b] build graph"),
                            ftxui::separator(), ftxui::text("[f] find paths")));
          }) |
      ftxui::CatchEvent(EventCallback));
}

#include <compare>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/indirect.hpp>
#include <range/v3/view/transform.hpp>

#include "get_me/backwards_path_finding.hpp"
#include "get_me/config.hpp"
#include "get_me/graph.hpp"
#include "get_me/query.hpp"
#include "get_me/tooling.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/get_me_exception.hpp"
#include "tui/components/config_editor.hpp"
#include "tui/components/paths_menu.hpp"
#include "tui/components/query.hpp"

class TransitionCollectionState {
public:
  TransitionCollectionState(std::shared_ptr<Config> Conf,
                            clang::tooling::ClangTool &Tool,
                            std::shared_ptr<TransitionData> Transitions =
                                std::make_shared<TransitionData>())
      : Conf_(std::move(Conf)),
        Tool_(Tool),
        Transitions_{std::move(Transitions)} {}

  void update() {
    collectTransitions();

    AcquiredTypeNames_ =
        Transitions_->Data | ranges::views::transform(ToAcquired) |
        ranges::views::transform([](const TypeSetValueType Acquired) {
          return fmt::format("{}", Acquired);
        }) |
        ranges::to_vector;
  }

  [[nodiscard]] std::vector<std::string> &getAqcuiredTypeNames() {
    return AcquiredTypeNames_;
  }

  [[nodiscard]] const std::vector<std::string> &getAqcuiredTypeNames() const {
    return AcquiredTypeNames_;
  }

  [[nodiscard]] std::shared_ptr<TransitionData> &getTransitionsPtr() {
    return Transitions_;
  }
  [[nodiscard]] TransitionData &getTransitions() { return *Transitions_; }

  [[nodiscard]] const TransitionData &getTransitions() const {
    return *Transitions_;
  }

private:
  void collectTransitions() {
    const auto BuildASTsResult = Tool_.buildASTs(ASTs_);
    GetMeException::verify(BuildASTsResult == 0, "Error building ASTs");

    Transitions_ = ::collectTransitions(*ASTs_.front(), Conf_);
  }

  std::shared_ptr<Config> Conf_;
  clang::tooling::ClangTool &Tool_;
  std::vector<std::unique_ptr<clang::ASTUnit>> ASTs_;
  std::shared_ptr<TransitionData> Transitions_{};
  std::vector<std::string> AcquiredTypeNames_{};
};

void runTui(const std::shared_ptr<Config> Conf,
            clang::tooling::ClangTool &Tool) {
  auto CollectionState = TransitionCollectionState{Conf, Tool};

  auto Screen = ftxui::ScreenInteractive::Fullscreen();

  auto TabSelection = 0;

  std::string QueriedName{};
  std::vector<std::string> PathsStr{};
  // FIXME: this early init runs into an error state bc transitions is not yet
  // initialized (aka filled)
  const auto CommitCallback = [&Conf, &PathsStr, &CollectionState,
                               &QueriedName]() {
    const auto Query = getQueriedTypesForInput(
        CollectionState.getTransitionsPtr()->Data, QueriedName);

    auto Data =
        runGraphBuilding(CollectionState.getTransitionsPtr(), Query, Conf);
    runPathFinding(Data);

    const auto Paths =
        Data.Paths | ranges::to_vector |
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
    PathsStr =
        Paths | ranges::views::enumerate |
        ranges::views::transform([&Data](const auto IndexedPath) {
          const auto &[Number, Path] = IndexedPath;
          return fmt::format(
              "path #{}: {} -> remaining: {}", Number,
              fmt::join(
                  Path | ranges::views::transform(
                             [&Data](const TransitionEdgeType &Edge) {
                               return fmt::format(
                                   "{}",
                                   ToTransition(
                                       Data.Transitions
                                           ->FlatData[Edge.TransitionIndex]));
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
          buildConfigComponent(Conf),
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
  // NOLINTNEXTLINE(performance-unnecessary-value-param)
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

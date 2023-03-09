#include "tui/components/query.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fmt/core.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>
#include <range/v3/algorithm/for_each.hpp>

#include "tui/fuzzy_search_state.hpp"

namespace {
class QueryCommitState {

public:
  using CallbackType = std::function<void()>;

  explicit QueryCommitState(std::string *QueryStr)
      : QueryStr_(QueryStr) {}

  void commit() {
    OldQuery_ = *QueryStr_;
    ranges::for_each(OnCommitCallbacks_, &CallbackType::operator());
  }

  [[nodiscard]] bool isCommited() const { return *QueryStr_ == OldQuery_; }

  [[nodiscard]] std::string *getQueryString() { return QueryStr_; }

  void addCallback(CallbackType Callback) {
    OnCommitCallbacks_.push_back(std::move(Callback));
  }

private:
  std::string *QueryStr_;
  std::string OldQuery_{};
  std::vector<CallbackType> OnCommitCallbacks_{};
};

class QueryComponent : public ftxui::ComponentBase {
public:
  QueryComponent(std::string *const QueryStr,
                 const std::vector<std::string> *const Entries,
                 std::function<void()> Callback)
      : SearchState_{Entries},
        QueryState_(QueryStr) {
    Add(buildQueryComponent());
    QueryState_.addCallback(std::move(Callback));
  }

private:
  [[nodiscard]] ftxui::Component buildSearchFieldComponent() {
    QueryState_.addCallback([this] { ShowFuzzySearchBox_ = false; });

    const auto QueryTextInputOption = ftxui::InputOption{
        .on_change =
            [this] {
              SearchState_.scoreAndSort(*QueryState_.getQueryString());
              ShowFuzzySearchBox_ = true;
            },
        .on_enter =
            [this] {
              const auto &SearchResults = SearchState_.getValues();
              if (SearchResults.empty()) {
                return;
              }

              *QueryState_.getQueryString() = SearchResults.front();
              QueryState_.commit();
            },
    };

    const auto InputText = ftxui::Input(QueryState_.getQueryString(),
                                        "placeholder", QueryTextInputOption);

    return Renderer(InputText, [InputText] {
      return window(ftxui::text("Query"), InputText->Render());
    });
  }

  [[nodiscard]] ftxui::Element renderSelectedResult() {
    const auto &SearchResults = SearchState_.getValues();
    if (0 > SelectedSearchResult_ ||
        std::cmp_greater_equal(SelectedSearchResult_, SearchResults.size())) {
      return vbox(ftxui::text("Type: "), ftxui::text("Length: "));
    }
    const auto &Current =
        SearchResults[static_cast<size_t>(SelectedSearchResult_)];
    return vbox(ftxui::text(fmt::format("Type: {}", Current)),
                ftxui::text(fmt::format("Length: {}", Current.size())));
  };

  [[nodiscard]] ftxui::Component buildFuzzySearchResultsComponent() {
    auto MenuOption = ftxui::MenuOption::Vertical();
    MenuOption.on_enter = [this] {
      *QueryState_.getQueryString() =
          SearchState_.getValues()[static_cast<size_t>(SelectedSearchResult_)];
      QueryState_.commit();
    };

    const auto Results =
        Menu(&SearchState_.getValues(), &SelectedSearchResult_, MenuOption);

    return ftxui::Renderer(Results, [this, Results] {
      constexpr static auto MinWidth = 50;

      const auto WindowTitle = ftxui::text(
          fmt::format("# types: {}", SearchState_.getValues().size()));

      const auto WindowBody =
          hbox(Results->Render() | ftxui::frame | ftxui::flex_shrink,
               ftxui::separator(),
               renderSelectedResult() |
                   size(ftxui::Direction::WIDTH,
                        ftxui::Constraint::GREATER_THAN, MinWidth) |
                   ftxui::flex_shrink);
      return window(WindowTitle, WindowBody);
    });
  }

  [[nodiscard]] ftxui::Component buildQueryComponent() {
    const auto QueryComponent = buildSearchFieldComponent();
    const auto SearchResultsComponent =
        buildFuzzySearchResultsComponent() | ftxui::Maybe(&ShowFuzzySearchBox_);
    const auto Result =
        ftxui::Container::Vertical({QueryComponent, SearchResultsComponent});
    return Renderer(Result, [QueryComponent, SearchResultsComponent] {
      return vbox(QueryComponent->Render() | ftxui::flex_grow,
                  SearchResultsComponent->Render() | ftxui::flex_shrink) |
             ftxui::flex_shrink;
    });
  }

  FuzzySearchState SearchState_;
  int SelectedSearchResult_{0};
  bool ShowFuzzySearchBox_ = true;

  QueryCommitState QueryState_;

  ftxui::ButtonOption CommitButtonOptions_{ftxui::ButtonOption::Ascii()};
};
} // namespace

ftxui::Component
buildQueryComponent(std::string *QueriedName,
                    const std::vector<std::string> *const Entries,
                    std::function<void()> Callback) {
  return ftxui::Make<QueryComponent>(QueriedName, Entries, std::move(Callback));
}

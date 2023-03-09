#include "tui/components/paths_menu.hpp"

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>

namespace {
class PathsComponent : public ftxui::ComponentBase {
public:
  explicit PathsComponent(std::vector<std::string> *const Paths)
      : Paths_(Paths) {
    Add(ftxui::Menu(Paths_, &SelectedPath_));
  }

private:
  int SelectedPath_{0};
  std::vector<std::string> *Paths_{};
};
} // namespace

ftxui::Component buildPathsComponent(std::vector<std::string> *const Paths) {
  const auto PathsMenu = ftxui::Make<PathsComponent>(Paths);
  return Renderer(PathsMenu, [PathsMenu] {
    return vbox(PathsMenu->Render() | ftxui::frame) | ftxui::flex_shrink;
  });
}

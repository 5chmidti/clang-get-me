#include <memory>
#include <string>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnostic.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/format.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

static cl::OptionCategory ToolCategory("get_me");
static cl::opt<std::string> Option("o", cl::desc("Some option"),
                                   cl::value_desc("option"), cl::Required,
                                   cl::ValueRequired, cl::cat(ToolCategory));

class MyCallback : public MatchFinder::MatchCallback {
public:
  void run(const MatchFinder::MatchResult &Result) final {
    const auto *const MatchedExpr = Result.Nodes.getNodeAs<Expr>("expr");
    TextDiagnostic Diag(
        llvm::outs(), Result.Context->getLangOpts(),
        &Result.Context->getDiagnostics().getDiagnosticOptions());

    Diag.emitDiagnostic(
        Result.Context->getFullLoc(MatchedExpr->getBeginLoc()),
        DiagnosticsEngine::Level::Note, "my message",
        CharSourceRange::getTokenRange(MatchedExpr->getSourceRange()), None);
  }
};

int main(int argc, const char **argv) {
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  auto OptionsParser = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!OptionsParser) {
    llvm::errs() << OptionsParser.takeError();
    return 1;
  }
  const auto &Sources = OptionsParser->getSourcePathList();
  ClangTool Tool(OptionsParser->getCompilations(),
                 Sources.empty()
                     ? OptionsParser->getCompilations().getAllFiles()
                     : Sources);

  auto Callback = std::make_unique<MyCallback>();

  const auto Match = expr().bind("expr");

  MatchFinder Finder{};
  Finder.addMatcher(Match, Callback.get());

  auto Action = newFrontendActionFactory(&Finder);
  return Tool.run(Action.get());
}

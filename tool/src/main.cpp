#include <memory>
#include <string>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
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
static cl::opt<std::string> TypeName("t", cl::desc("Name of the type to get"),
                                     cl::Required, cl::ValueRequired,
                                     cl::cat(ToolCategory));

class GetMeCallback : public MatchFinder::MatchCallback {
public:
  void run(const MatchFinder::MatchResult &Result) final {
    const auto *const NDecl = Result.Nodes.getNodeAs<NamedDecl>("named-decl");
    const auto FullLoc = Result.Context->getFullLoc(NDecl->getLocation());
    TextDiagnostic Diag(
        llvm::outs(), Result.Context->getLangOpts(),
        &Result.Context->getDiagnostics().getDiagnosticOptions());

    const auto Message = [&Result]() {
      auto Res = fmt::format("found '{}'", TypeName.getValue());
      if (const auto *const ParentDecl =
              Result.Nodes.getNodeAs<NamedDecl>("parent-decl");
          ParentDecl != nullptr) {
        return fmt::format("{} from {} '{}'", Res,
                           ParentDecl->getDeclKindName(),
                           ParentDecl->getDeclName().getAsString());
      }
      return Res;
    }();
    Diag.emitDiagnostic(FullLoc, DiagnosticsEngine::Level::Note, Message,
                        CharSourceRange::getTokenRange(NDecl->getSourceRange()),
                        None);
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

  auto Callback = std::make_unique<GetMeCallback>();

  const auto Match =
      namedDecl(
          anyOf(functionDecl(returns(hasDeclaration(
                                 namedDecl(hasName(TypeName.getValue())))))
                    .bind("parent-decl"),
                varDecl(hasAncestor(namedDecl().bind("parent-decl")),
                        hasType(namedDecl(hasName(TypeName.getValue()))),
                        unless(parmVarDecl())),
                fieldDecl(hasAncestor(namedDecl().bind("parent-decl")),
                          hasType(namedDecl(hasName(TypeName.getValue()))))))
          .bind("named-decl");

  MatchFinder Finder{};
  Finder.addMatcher(Match, Callback.get());

  auto Action = newFrontendActionFactory(&Finder);
  return Tool.run(Action.get());
}

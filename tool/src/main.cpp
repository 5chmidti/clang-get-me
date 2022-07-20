#include <compare>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/PrettyPrinter.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/TextDiagnostic.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>

#include "get_me/graph.hpp"

// NOLINTBEGIN
using namespace llvm;
using namespace clang;
using namespace clang::tooling;

static cl::OptionCategory ToolCategory("get_me");
static cl::opt<std::string> TypeName("t", cl::desc("Name of the type to get"),
                                     cl::Required, cl::ValueRequired,
                                     cl::cat(ToolCategory));
// NOLINTEND

using edge_weight_type = LinkType;
using GraphType = boost::adjacency_list<
    boost::listS, boost::vecS, boost::directedS, boost::no_property,
    boost::property<boost::edge_weight_t, edge_weight_type>>;

using TransitionDataType = std::variant<FunctionDecl *, FieldDecl *>;
using TransitionSourceType = Type;

template <class... Ts> struct Overloaded : public Ts... {
  using Ts::operator()...;
};

template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

[[nodiscard]] static std::string
getTransitionName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{[](const FunctionDecl *FDecl) {
                   return fmt::format("{}", FDecl->getNameAsString());
                 },
                 [](const FieldDecl *FDecl) {
                   return fmt::format("{}", FDecl->getNameAsString());
                 }},
      Data);
}

[[nodiscard]] static std::string getTypeAsString(const Type &T) {
  if (const auto *const TDecl = T.getAsTagDecl()) {
    return TDecl->getNameAsString();
  }
  if (const auto *const BuiltinType = T.getAsPlaceholderType()) {
    return BuiltinType->getName(PrintingPolicy{LangOptions{}}).str();
  }

  if (!T.isLinkageValid()) {
    return "invalid linkage";
  }

  return "unknown type";
}

[[nodiscard]] static std::string
getTransitionTargetTypeName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{[](const FunctionDecl *FDecl) {
                   if (const auto *const Constructor =
                           dyn_cast<CXXConstructorDecl>(FDecl)) {
                     return Constructor->getParent()->getNameAsString();
                   }
                   return FDecl->getReturnType().getAsString();
                 },
                 [](const FieldDecl *FDecl) {
                   return FDecl->getType().getAsString();
                 }},
      Data);
}

[[nodiscard]] static std::string
getTransitionSourceTypeName(const TransitionDataType &Data) {
  return std::visit(
      Overloaded{
          [](const FunctionDecl *FDecl) -> std::string {
            const auto Parameters = FDecl->parameters();
            return fmt::format(
                "{}", fmt::join(Parameters |
                                    ranges::views::transform(
                                        [](const ParmVarDecl *PDecl) {
                                          return PDecl->getType().getAsString();
                                        }),
                                ", "));
          },
          [](const FieldDecl *FDecl) -> std::string {
            return FDecl->getParent()->getNameAsString();
          }},
      Data);
}

template <> struct fmt::formatter<TransitionDataType> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TransitionDataType &Val, FormatContext &Ctx)
      -> decltype(Ctx.out()) {
    return fmt::format_to(
        Ctx.out(), "{} {}({})", getTransitionTargetTypeName(Val),
        getTransitionName(Val), getTransitionSourceTypeName(Val));
  }
};

struct TransitionCollector {
  std::vector<TransitionDataType> Data{};

  template <typename T>
  void emplace(T &&Value) requires std::convertible_to<T, TransitionDataType> {
    Data.emplace_back(std::forward<T>(Value));
  }
};

class GetMeVisitor : public RecursiveASTVisitor<GetMeVisitor> {
public:
  GetMeVisitor(ASTContext *Context, TransitionCollector &Collector)
      : Ctx(Context), CollectorRef{Collector} {}

  [[nodiscard]] bool VisitFunctionDecl(FunctionDecl *FDecl) {
    CollectorRef.emplace(FDecl);
    return true;
  }

  [[nodiscard]] bool VisitFieldDecl(FieldDecl *Field) {
    // const Type *const TypePtr = Field->getType().getTypePtr();
    CollectorRef.emplace(Field);
    return true;
  }

private:
  ASTContext *Ctx;
  TransitionCollector &CollectorRef;
};

class GetMe : public ASTConsumer {
public:
  GetMe(ASTContext *Context, TransitionCollector &Collector)
      : Visitor(Context, Collector) {}

  void HandleTranslationUnit(clang::ASTContext &Context) override {
    // Traversing the translation unit decl via a RecursiveASTVisitor
    // will visit all nodes in the AST.
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  GetMeVisitor Visitor;
};

class GetMeAction : public clang::ASTFrontendAction {
public:
  explicit GetMeAction(TransitionCollector &Collector)
      : CollectorRef{Collector} {}

  [[nodiscard]] std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &Compiler,
                    llvm::StringRef /*InFile*/) override {
    return std::make_unique<GetMe>(&Compiler.getASTContext(), CollectorRef);
  }

private:
  TransitionCollector &CollectorRef;
};

class GetMeActionFactory : public tooling::FrontendActionFactory {
public:
  explicit GetMeActionFactory(TransitionCollector &Collector)
      : CollectorRef(Collector) {}

  [[nodiscard]] std::unique_ptr<clang::FrontendAction> create() override {
    return std::make_unique<GetMeAction>(CollectorRef);
  }

private:
  TransitionCollector &CollectorRef;
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

  GraphData<GraphType, TypeSet, TransitionDataType> GraphData{};

  TransitionCollector Collector{};
  GetMeActionFactory Factory{Collector};
  auto Action = std::make_unique<GetMeAction>(Collector);
  const auto ToolRunResult = Tool.run(&Factory);
  assert(ToolRunResult == 0);

  fmt::print("{}\n", Collector.Data);

  // build graph from gathered graph data
}

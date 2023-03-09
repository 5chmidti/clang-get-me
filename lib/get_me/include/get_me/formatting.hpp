#ifndef get_me_lib_get_me_include_get_me_formatting_hpp
#define get_me_lib_get_me_include_get_me_formatting_hpp

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <boost/algorithm/string/replace.hpp>
#include <clang/AST/Decl.h>
#include <clang/AST/Type.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <range/v3/algorithm/fold_left.hpp>

#include "get_me/graph.hpp"
#include "get_me/path_traversal.hpp"
#include "get_me/propagate_type_aliasing.hpp"
#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"
#include "support/ranges/ranges.hpp"
#include "support/variant.hpp"

namespace clang {
class Type;
} // namespace clang

[[nodiscard]] std::string getTransitionName(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionAcquiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string
getTransitionRequiredTypeNames(const TransitionDataType &Data);

[[nodiscard]] std::string toString(const clang::Type *Type);
[[nodiscard]] std::string toString(const clang::NamedDecl *NDecl);
[[nodiscard]] std::vector<std::string>
toString(const std::vector<PathType> &Paths, const GraphData &Data);

template <> struct fmt::formatter<EdgeDescriptor> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const EdgeDescriptor &Edge,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "{}",
                          std::pair{Source(Edge), Target(Edge)});
  }
};

template <> struct fmt::formatter<TransitionDataType> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TransitionDataType &Val,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(
        Ctx.out(), "{} {}({})", getTransitionAcquiredTypeNames(Val),
        getTransitionName(Val), getTransitionRequiredTypeNames(Val));
  }
};

template <> struct fmt::formatter<TypeSetValueType> {
  template <typename FormatContext>
  [[nodiscard]] constexpr auto parse(FormatContext &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TypeSetValueType &Val,
                            FormatContext &Ctx) const -> decltype(Ctx.out()) {
    return fmt::format_to(
        Ctx.out(), "{}",
        std::visit(Overloaded{[](const clang::Type *const Type) {
                                return toString(Type);
                              },
                              [](const ArithmeticType & /*Arithmetic*/)
                                  -> std::string { return "arithmetic"; }},
                   Val));
  }
};

template <> class fmt::formatter<GraphData> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    const auto *Iter = Ctx.begin();
    const auto *const End = Ctx.end();
    if (Iter != End && *Iter == 'd') {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
      Presentation_ = *Iter++;
      Ctx.advance_to(Iter);
    }
    return Iter;
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const GraphData &Val, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    switch (Presentation_) {
    case 'd':
      return fmt::format_to(Ctx.out(), "{}", toDotFormat(Val));
    }
    return fmt::format_to(Ctx.out(), "{}", Val);
  }

private:
  static std::string toDotFormat(const GraphData &Data) {
    const auto IndexMap = boost::get(boost::edge_index, Data.Graph);

    const auto ToString = [&Data, &IndexMap](const auto &Edge) {
      const auto SourceNode = Source(Edge);
      const auto TargetNode = Target(Edge);

      const auto Transition =
          ToTransition(Data.EdgeWeights[boost::get(IndexMap, Edge)]);
      const auto TargetVertex = Data.VertexData[TargetNode];
      const auto SourceVertex = Data.VertexData[SourceNode];

      auto EdgeWeightAsString = fmt::format("{}", Transition);
      boost::replace_all(EdgeWeightAsString, "\"", "\\\"");
      return fmt::format(
          R"(  "{}" -> "{}"[label="{}"]
)",
          SourceVertex, TargetVertex, EdgeWeightAsString);
    };

    auto Res = ranges::fold_left(
        toRange(boost::edges(Data.Graph)) | ranges::views::transform(ToString),
        std::string{"digraph D {{\n  layout = \"sfdp\";\n"},
        [](std::string Result, auto Line) {
          Result.append(std::move(Line));
          return Result;
        });
    Res.append("}}\n");

    return Res;
  }

  char Presentation_{};
};

template <> class fmt::formatter<TypeAlias> {
public:
  [[nodiscard]] constexpr auto parse(format_parse_context &Ctx)
      -> decltype(Ctx.begin()) {
    return Ctx.begin();
  }

  template <typename FormatContext>
  [[nodiscard]] auto format(const TypeAlias &Val, FormatContext &Ctx) const
      -> decltype(Ctx.out()) {
    return fmt::format_to(Ctx.out(), "({}, {})", toString(Val.Base),
                          toString(Val.Alias));
  }
};

#endif

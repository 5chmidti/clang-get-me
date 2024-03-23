#ifndef get_me_lib_get_me_include_get_me_config_hpp
#define get_me_lib_get_me_include_get_me_config_hpp

#include <array>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <fmt/core.h>
#include <llvm/Support/YAMLTraits.h>
#include <llvm/Support/raw_ostream.h>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/utility/tuple_algorithm.hpp>

class Config {
public:
  template <typename ValueType>
  using MappingType = std::pair<std::string_view, ValueType Config::*>;

  using BooleanMappingType = MappingType<bool>;
  using SizeTMappingType = MappingType<std::size_t>;

  [[nodiscard]] static consteval auto getConfigMapping() {
    return std::tuple{
        std::array{
            BooleanMappingType{"EnableFilterOverloads",
                               &Config::EnableFilterOverloads},
            BooleanMappingType{"EnablePropagateInheritance",
                               &Config::EnablePropagateInheritance},
            BooleanMappingType{"EnablePropagateTypeAlias",
                               &Config::EnablePropagateTypeAlias},
            BooleanMappingType{"EnableTruncateArithmetic",
                               &Config::EnableTruncateArithmetic},
            BooleanMappingType{"EnableFilterArithmeticTransitions",
                               &Config::EnableFilterArithmeticTransitions},
            BooleanMappingType{"EnableFilterStd", &Config::EnableFilterStd},
            BooleanMappingType{"EnableGraphBackwardsEdge",
                               &Config::EnableGraphBackwardsEdge},
            BooleanMappingType{"EnableVerboseTransitionCollection",
                               &Config::EnableVerboseTransitionCollection},
        },
        std::array{
            SizeTMappingType{"MaxGraphDepth", &Config::MaxGraphDepth},
            SizeTMappingType{"MaxRemainingTypes", &Config::MaxRemainingTypes},
            SizeTMappingType{"MaxPathLength", &Config::MaxPathLength},
            SizeTMappingType{"MaxPathOutputCount", &Config::MaxPathOutputCount},
        }};
  }

  [[nodiscard]] static Config parse(const std::filesystem::path &File);

  void save(const std::filesystem::path &File);

  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  bool EnableFilterOverloads = false;
  bool EnablePropagateInheritance = true;
  bool EnablePropagateTypeAlias = true;
  bool EnableTruncateArithmetic = false;
  bool EnableFilterArithmeticTransitions = false;
  bool EnableFilterStd = false;
  bool EnableGraphBackwardsEdge = true;
  bool EnableVerboseTransitionCollection = false;

  std::size_t MaxGraphDepth = std::numeric_limits<std::size_t>::max();
  std::size_t MaxRemainingTypes = std::numeric_limits<std::size_t>::max();
  std::size_t MaxPathLength = std::numeric_limits<std::size_t>::max();
  std::size_t MaxPathOutputCount = 10U;

  // NOLINTEND(misc-non-private-member-variables-in-classes)
};

template <> struct llvm::yaml::MappingTraits<Config> {
  static void mapping(llvm::yaml::IO &YamlIO, Config &Conf) {
    const auto MapOptionals = [&Conf,
                               &YamlIO](const ranges::range auto &Mappings) {
      const auto MapOptional =
          [&Conf, &YamlIO]<typename ValueType>(
              const Config::MappingType<ValueType> &MappingValue) {
            const auto &[ValueName, ValueAddress] = MappingValue;
            YamlIO.mapOptional(ValueName.data(),
                               std::invoke(ValueAddress, Conf));
          };

      ranges::for_each(Mappings, MapOptional);
    };

    ranges::tuple_for_each(Config::getConfigMapping(), MapOptionals);
  }
};

template <> class fmt::formatter<Config> {
public:
  [[nodiscard]] constexpr format_parse_context::iterator
  parse(format_parse_context &ctx) {
    return ctx.begin();
  }

  [[nodiscard]] format_context::iterator format(Config Conf,
                                                format_context &ctx) const {
    std::string Str;
    llvm::raw_string_ostream Stream{Str};
    auto OutStream = llvm::yaml::Output{Stream};
    OutStream << Conf;
    return fmt::format_to(ctx.out(), "{}", Str);
  }
};

#endif

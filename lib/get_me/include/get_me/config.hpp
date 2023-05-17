#ifndef get_me_lib_get_me_include_get_me_config_hpp
#define get_me_lib_get_me_include_get_me_config_hpp

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string_view>
#include <tuple>
#include <utility>

#include <fmt/core.h>
#include <llvm/Support/YAMLTraits.h>
#include <range/v3/algorithm/for_each.hpp>

class Config {
public:
  template <typename ValueType>
  using MappingType = std::pair<std::string_view, ValueType Config::*>;

  using BooleanMappingType = MappingType<bool>;
  using SizeTMappingType = MappingType<std::size_t>;
  using Int64MappingType = MappingType<std::int64_t>;

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
            SizeTMappingType{"MinPathCount", &Config::MinPathCount},
            SizeTMappingType{"MaxPathCount", &Config::MaxPathCount},
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
  bool EnableGraphBackwardsEdge = false;
  bool EnableVerboseTransitionCollection = false;

  std::size_t MaxGraphDepth = std::numeric_limits<std::size_t>::max();
  std::size_t MaxRemainingTypes = std::numeric_limits<std::size_t>::max();
  std::size_t MaxPathLength = std::numeric_limits<std::size_t>::max();
  std::size_t MinPathCount = 0U;
  std::size_t MaxPathOutputCount = 10U;
  std::size_t MaxPathCount = std::numeric_limits<std::size_t>::max();

  // NOLINTEND(misc-non-private-member-variables-in-classes)
};

template <> struct llvm::yaml::MappingTraits<Config> {
  static void mapping(llvm::yaml::IO &YamlIO, Config &Conf);
};

#endif

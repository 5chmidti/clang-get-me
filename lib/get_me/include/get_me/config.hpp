#ifndef get_me_lib_get_me_include_get_me_config_hpp
#define get_me_lib_get_me_include_get_me_config_hpp

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string_view>
#include <utility>

#include <llvm/Support/YAMLTraits.h>

class Config {
public:
  template <typename ValueType>
  using MappingType = std::pair<std::string_view, ValueType>;

  using BooleanMappingType = MappingType<bool Config::*>;
  using SizeTMappingType = MappingType<std::size_t Config::*>;
  using Int64MappingType = MappingType<std::int64_t Config::*>;

  static constexpr std::size_t NumBooleanFlags = 5;
  static constexpr std::size_t NumSizeTValues = 4;
  static constexpr std::size_t NumInt64Values = 1;

  [[nodiscard]] static consteval std::tuple<
      std::array<BooleanMappingType, NumBooleanFlags>,
      std::array<SizeTMappingType, NumSizeTValues>,
      std::array<Int64MappingType, NumInt64Values>>
  getConfigMapping() {
    constexpr auto BooleanMappings = std::array{
        BooleanMappingType{"EnableFilterOverloads",
                           &Config::EnableFilterOverloads},
        BooleanMappingType{"EnablePropagateInheritance",
                           &Config::EnablePropagateInheritance},
        BooleanMappingType{"EnablePropagateTypeAlias",
                           &Config::EnablePropagateTypeAlias},
        BooleanMappingType{"EnableTruncateArithmetic",
                           &Config::EnableTruncateArithmetic},
        BooleanMappingType{"EnableFilterStd", &Config::EnableFilterStd},
    };
    constexpr auto SizeTMappings = std::array{
        SizeTMappingType{"MaxGraphDepth", &Config::MaxGraphDepth},
        SizeTMappingType{"MaxPathLength", &Config::MaxPathLength},
        SizeTMappingType{"MinPathCount", &Config::MinPathCount},
        SizeTMappingType{"MaxPathCount", &Config::MaxPathCount},
    };
    constexpr auto Int64Mappings = std::array{
        Int64MappingType{"GraphVertexDepthDifferenceThreshold",
                         &Config::GraphVertexDepthDifferenceThreshold}};
    return {BooleanMappings, SizeTMappings, Int64Mappings};
  }

  [[nodiscard]] static Config parse(const std::filesystem::path &File);

  void save(const std::filesystem::path &File);

  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  bool EnableFilterOverloads = false;
  bool EnablePropagateInheritance = true;
  bool EnablePropagateTypeAlias = true;
  bool EnableTruncateArithmetic = false;
  bool EnableFilterStd = false;

  std::size_t MaxGraphDepth = std::numeric_limits<std::size_t>::max();
  std::size_t MaxPathLength = std::numeric_limits<std::size_t>::max();
  std::size_t MinPathCount = 0U;
  std::size_t MaxPathCount = std::numeric_limits<std::size_t>::max();

  std::int64_t GraphVertexDepthDifferenceThreshold = 1;
  // NOLINTEND(misc-non-private-member-variables-in-classes)
};

template <> struct llvm::yaml::MappingTraits<Config> {
  static void mapping(llvm::yaml::IO &YamlIO, Config &Conf);
};

#endif

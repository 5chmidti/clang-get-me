#ifndef get_me_include_get_me_config_hpp
#define get_me_include_get_me_config_hpp

#include <cstddef>
#include <filesystem>
#include <limits>

#include <llvm/Support/YAMLTraits.h>

class Config {
public:
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
  // NOLINTEND(misc-non-private-member-variables-in-classes)
};

template <> struct llvm::yaml::MappingTraits<Config> {
  static void mapping(llvm::yaml::IO &YamlIO, Config &Conf);
};

#endif

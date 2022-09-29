#ifndef get_me_config_hpp
#define get_me_config_hpp

#include <cstddef>
#include <optional>

struct Config {
  bool EnableFilterOverloads = false;
  bool EnablePropagateInheritance = true;
  bool EnablePropagateTypeAlias = true;
  bool EnableTruncateArithmetic = false;
  bool EnableFilterStd = false;

  std::optional<std::size_t> MaxGraphDepth = 10;
  std::optional<std::size_t> MaxPathLength = 10;
  std::optional<std::size_t> MinPathCount = 10;
  std::optional<std::size_t> MaxPathCount = 1000;
};

[[nodiscard]] constexpr Config getDefaultConfig() { return Config{}; }

#endif

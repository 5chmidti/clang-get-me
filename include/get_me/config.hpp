#ifndef get_me_include_get_me_config_hpp
#define get_me_include_get_me_config_hpp

#include <cstddef>
#include <limits>
#include <optional>

struct Config {
  bool EnableFilterOverloads = false;
  bool EnablePropagateInheritance = true;
  bool EnablePropagateTypeAlias = true;
  bool EnableTruncateArithmetic = false;
  bool EnableFilterStd = false;

  std::size_t MaxGraphDepth = std::numeric_limits<std::size_t>::max();
  std::size_t MaxPathLength = std::numeric_limits<std::size_t>::max();
  std::size_t MinPathCount = 0U;
  std::size_t MaxPathCount = std::numeric_limits<std::size_t>::max();
};

[[nodiscard]] constexpr Config getDefaultConfig() { return Config{}; }

#endif

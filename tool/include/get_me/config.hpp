#ifndef get_me_config_hpp
#define get_me_config_hpp

#include <cstddef>

struct Config {
  bool EnableFilterOverloads = false;
  bool EnablePropagateInheritance = true;
  bool EnablePropagateTypeAlias = true;
  bool EnableTruncateArithmetic = false;
  bool EnableFilterStd = false;

  std::size_t MaxGraphDepth = 10;
  std::size_t MaxPathLength = 10;
  std::size_t MinPathCount = 10;
  std::size_t MaxPathCount = 1000;
};

[[nodiscard]] consteval Config getDefaultConfig() { return Config{}; }

#endif

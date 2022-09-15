#ifndef get_me_config_hpp
#define get_me_config_hpp

struct Config {
  bool EnableArithmeticTruncation = false;
  bool EnableFilterOverloads = false;
  bool EnableTruncateArithmetic = false;
};

[[nodiscard]] consteval Config getDefaultConfig() { return Config{}; }

#endif

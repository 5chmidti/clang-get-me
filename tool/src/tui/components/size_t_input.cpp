#include "tui/components/size_t_input.hpp"

#include <ctre.hpp>

void parser(std::size_t &Value, std::string_view Str) {
  if (const auto Mat = ctre::match<"\\d+">(Str); Mat) {
    Value = Mat.get<0>().to_number<std::size_t>();
  }
}

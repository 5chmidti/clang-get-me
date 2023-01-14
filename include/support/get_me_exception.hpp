#ifndef get_me_include_support_get_me_exception_hpp
#define get_me_include_support_get_me_exception_hpp

#include <concepts>
#include <exception>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>
#include <spdlog/spdlog.h>

class GetMeException : public std::exception {
public:
  explicit GetMeException(std::string Message)
      : Message_(std::move(Message)) {
    spdlog::error(Message_);
  }

  template <typename... Ts>
  explicit GetMeException(const std::string_view FormatString, Ts &&...Args)
      : Message_{fmt::format(fmt::runtime(FormatString),
                             std::forward<Ts>(Args)...)} {
    spdlog::error(Message_);
  }

  [[nodiscard]] const char *what() const noexcept final { return ""; }

  template <typename... Ts>
  static void verify(const bool Condition, const std::string_view FormatString,
                     Ts &&...Args) {
    if (!Condition) {
      throw GetMeException{FormatString, std::forward<Ts>(Args)...};
    }
  }

private:
  std::string Message_;
};

#endif

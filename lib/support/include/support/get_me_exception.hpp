#ifndef get_me_lib_support_include_support_get_me_exception_hpp
#define get_me_lib_support_include_support_get_me_exception_hpp

#include <exception>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

class GetMeException : public std::exception {
public:
  explicit GetMeException(std::string Message)
      : Message_(std::move(Message)) {
    spdlog::error(Message_);
    spdlog::dump_backtrace();
  }

  template <typename... Ts>
  explicit GetMeException(const std::string_view FormatString, Ts &&...Args)
      : GetMeException{fmt::format(fmt::runtime(FormatString),
                                   std::forward<Ts>(Args)...)} {}

  [[nodiscard]] const char *what() const noexcept final { return ""; }

  template <typename... Ts>
  static void verify(const bool Condition, const std::string_view FormatString,
                     Ts &&...Args) {
    if (!Condition) {
      fail(FormatString, std::forward<Ts>(Args)...);
    }
  }

  template <typename... Ts>
  static void fail(const std::string_view FormatString, Ts &&...Args) {
    throw GetMeException{FormatString, std::forward<Ts>(Args)...};
  }

private:
  std::string Message_;
};

#endif

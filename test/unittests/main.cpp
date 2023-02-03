#include <catch2/catch_session.hpp>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
  static constexpr auto BacktraceCount = 1024U;
  spdlog::enable_backtrace(BacktraceCount);

  const auto Result = Catch::Session().run(argc, argv);

  if (Result != 0) {
    spdlog::dump_backtrace();
  }

  return Result;
}

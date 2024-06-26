#include "get_me/config.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>

#include <fmt/std.h> // IWYU pragma: keep
#include <llvm/Support/YAMLTraits.h>
#include <llvm/Support/raw_ostream.h>

#include "support/get_me_exception.hpp"

Config Config::parse(const std::filesystem::path &File) {
  GetMeException::verify(std::filesystem::exists(File),
                         "Config file does not exist ({})", File);

  auto FileStream = std::ifstream{File};
  const auto FileData = std::string{std::istreambuf_iterator<char>{FileStream},
                                    std::istreambuf_iterator<char>{}};
  auto Input = llvm::yaml::Input{FileData};
  auto Conf = Config{};
  Input >> Conf;

  const auto Error = Input.error();
  GetMeException::verify(!Error, "Failed to parse config file: {}",
                         Error.message());
  return Conf;
}

void Config::save(const std::filesystem::path &File) {
  auto Error = std::error_code{};
  const auto FileName = File.string();
  auto FileStream = llvm::raw_fd_ostream{FileName, Error};
  auto OutStream = llvm::yaml::Output{FileStream};
  OutStream << *this;
  GetMeException::verify(!Error, "Error while writing config file: {}",
                         Error.message());
}

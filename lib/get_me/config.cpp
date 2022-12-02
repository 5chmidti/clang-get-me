#include "get_me/config.hpp"

#include <fstream>
#include <string>
#include <system_error>

#include <llvm/Support/raw_ostream.h>
#include <spdlog/spdlog.h>

std::optional<Config> Config::parse(const std::filesystem::path &File) {
  auto FileStream = std::ifstream{File};
  const auto FileData = std::string{std::istreambuf_iterator<char>{FileStream},
                                    std::istreambuf_iterator<char>{}};
  auto Input = llvm::yaml::Input{FileData};
  auto Conf = Config{};
  Input >> Conf;
  if (const auto Error = Input.error(); Error) {
    spdlog::error("Failed to parse config file: {}", Error.message());
    return std::nullopt;
  }
  return Conf;
}

void Config::save(const std::filesystem::path &File) {
  auto Error = std::error_code{};
  const auto FileName = File.string();
  auto FileStream = llvm::raw_fd_ostream{FileName, Error};
  auto OutStream = llvm::yaml::Output{FileStream};
  OutStream << *this;
}

void llvm::yaml::MappingTraits<Config>::mapping(llvm::yaml::IO &IO,
                                                Config &Conf) {
  IO.mapOptional("EnableFilterOverloads", Conf.EnableFilterOverloads);
  IO.mapOptional("EnablePropagateInheritance", Conf.EnablePropagateInheritance);
  IO.mapOptional("EnablePropagateTypeAlias", Conf.EnablePropagateTypeAlias);
  IO.mapOptional("EnableTruncateArithmetic", Conf.EnableTruncateArithmetic);
  IO.mapOptional("EnableFilterStd", Conf.EnableFilterStd);

  IO.mapOptional("MaxGraphDepth", Conf.MaxGraphDepth);
  IO.mapOptional("MaxPathLength", Conf.MaxPathLength);
  IO.mapOptional("MinPathCount", Conf.MinPathCount);
  IO.mapOptional("MaxPathCount", Conf.MaxPathCount);
}

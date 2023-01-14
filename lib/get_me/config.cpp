#include "get_me/config.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <llvm/Support/raw_ostream.h>
#include <range/v3/algorithm/for_each.hpp>
#include <spdlog/spdlog.h>

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

void llvm::yaml::MappingTraits<Config>::mapping(llvm::yaml::IO &YamlIO,
                                                Config &Conf) {
  const auto MapOptional =
      [&YamlIO, &Conf]<typename ValueType>(
          const Config::MappingType<ValueType> &MappingValue) {
        const auto &[ValueName, ValueAddress] = MappingValue;
        YamlIO.mapOptional(ValueName.data(), std::invoke(ValueAddress, Conf));
      };

  const auto [BooleanMapping, SizeTMapping] = Config::getConfigMapping();
  ranges::for_each(BooleanMapping, MapOptional);
  ranges::for_each(SizeTMapping, MapOptional);
}

std::pair<std::vector<Config::BooleanMappingType>,
          std::vector<Config::SizeTMappingType>>
Config::getConfigMapping() {
  return {
      {
          {"EnableFilterOverloads", &Config::EnableFilterOverloads},
          {"EnablePropagateInheritance", &Config::EnablePropagateInheritance},
          {"EnablePropagateTypeAlias", &Config::EnablePropagateTypeAlias},
          {"EnableTruncateArithmetic", &Config::EnableTruncateArithmetic},
          {"EnableFilterStd", &Config::EnableFilterStd},
      },
      {
          {"MaxGraphDepth", &Config::MaxGraphDepth},
          {"MaxPathLength", &Config::MaxPathLength},
          {"MinPathCount", &Config::MinPathCount},
          {"MaxPathCount", &Config::MaxPathCount},
      }};
}

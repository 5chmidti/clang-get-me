#ifndef get_me_lib_support_include_support_enum_mappings_hpp
#define get_me_lib_support_include_support_enum_mappings_hpp

#include <string_view>
#include <utility>

template <typename T>
using EnumerationMappingType = std::pair<std::string_view, T>;

template <typename T> [[nodiscard]] constexpr auto enumeration() = delete;

#endif

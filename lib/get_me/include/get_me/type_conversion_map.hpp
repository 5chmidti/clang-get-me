#ifndef get_me_lib_get_me_include_get_me_type_conversion_map_hpp
#define get_me_lib_get_me_include_get_me_type_conversion_map_hpp

#include <map>

#include "get_me/type_set.hpp"

using TypeConversionMap = std::map<Type, TypeSet>;

void combine(TypeConversionMap &Lhs, TypeConversionMap &&Rhs);

#endif

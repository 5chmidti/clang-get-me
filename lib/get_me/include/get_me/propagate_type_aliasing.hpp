#ifndef get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp
#define get_me_lib_get_me_include_get_me_propagate_type_aliasing_hpp

#include <vector>


#include "get_me/type_conversion_map.hpp"
#include "get_me/type_set.hpp"

void propagateTypeAliasing(
    TypeConversionMap &ConversionMap,
    const std::vector<TypeSetValueType> &TypedefNameDecls);

#endif

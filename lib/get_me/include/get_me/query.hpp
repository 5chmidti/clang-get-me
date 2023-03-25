#ifndef get_me_lib_get_me_include_get_me_query_hpp
#define get_me_lib_get_me_include_get_me_query_hpp

#include <string_view>

#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

[[nodiscard]] TransitionCollector::associative_container_type
getTransitionsForQuery(
    const TransitionCollector::associative_container_type &Transitions,
    const TypeSetValueType &QueriedType);

[[nodiscard]] TypeSetValueType getQueriedTypeForInput(
    const TransitionCollector::associative_container_type &Transitions,
    std::string_view QueriedTypeAsString);

#endif

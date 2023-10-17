#ifndef get_me_lib_get_me_include_get_me_query_hpp
#define get_me_lib_get_me_include_get_me_query_hpp

#include <string_view>

#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

[[nodiscard]] TransitionData::associative_container_type getTransitionsForQuery(
    const TransitionData::associative_container_type &Transitions,
    const TypeSet &Query);

[[nodiscard]] TypeSet
getQueriedTypesForInput(const TransitionData &Transitions,
                        std::string_view QueriedTypeAsString);

#endif

#ifndef get_me_lib_get_me_include_get_me_query_hpp
#define get_me_lib_get_me_include_get_me_query_hpp

#include <string_view>

#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

[[nodiscard]] TransitionCollector
getTransitionsForQuery(const TransitionCollector &Transitions,
                       const TypeSetValueType &QueriedType);

[[nodiscard]] TypeSetValueType
getQueriedTypeForInput(const TransitionCollector &Transitions,
                       std::string_view QueriedTypeAsString);

#endif

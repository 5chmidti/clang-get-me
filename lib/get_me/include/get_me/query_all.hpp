#ifndef get_me_lib_query_all_include_query_all_hpp
#define get_me_lib_query_all_include_query_all_hpp

#include <memory>

#include "get_me/config.hpp"
#include "get_me/transitions.hpp"

void queryAll(const std::shared_ptr<TransitionCollector> &Transitions,
              const Config &Conf);

#endif

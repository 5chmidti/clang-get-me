#ifndef get_me_lib_get_me_include_get_me_query_hpp
#define get_me_lib_get_me_include_get_me_query_hpp

#include "get_me/transitions.hpp"
#include "get_me/type_set.hpp"

class QueryType {
public:
  QueryType(std::shared_ptr<TransitionCollector> Transitions, std::string Query,
            Config Conf);

  [[nodiscard]] TransitionCollector getTransitionsForQuery() const;

  // getSourceVertexMatchingQueriedType

  [[nodiscard]] const TransitionCollector &getTransitions() const {
    return *Transitions_;
  }
  [[nodiscard]] const std::string &getQueriedTypeAsString() const {
    return QueriedTypeAsString_;
  }
  [[nodiscard]] const TypeSet &getQueriedType() const { return QueriedType_; }
  [[nodiscard]] const Config &getConfig() const { return Conf_; }

private:
  [[nodiscard]] bool matchesQueriedTypeName(const TypeSetValueType &Val) const;

  [[nodiscard]] TypeSetValueType getQueriedTypeForInput();

  std::shared_ptr<TransitionCollector> Transitions_{};
  std::string QueriedTypeAsString_{};
  TypeSet QueriedType_{};
  Config Conf_{};
};

#endif

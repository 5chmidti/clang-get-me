#ifndef get_me_lib_get_me_include_get_me_path_traversal_hpp
#define get_me_lib_get_me_include_get_me_path_traversal_hpp

#include <vector>

#include "get_me/graph.hpp"

class Config;

using PathType = std::vector<EdgeDescriptor>;

[[nodiscard]] std::vector<std::string>
toString(const std::vector<PathType> &Paths, const GraphData &Data);

[[nodiscard]] std::vector<PathType>
pathTraversal(const GraphData &Data, const Config &Conf,
              VertexDescriptor SourceVertex);

#endif

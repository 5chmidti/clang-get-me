#ifndef get_me_include_get_me_path_traversal_hpp
#define get_me_include_get_me_path_traversal_hpp

#include <vector>

#include "get_me/graph.hpp"

struct Config;

using PathType = std::vector<EdgeDescriptor>;

[[nodiscard]] std::vector<PathType>
pathTraversal(const GraphType &Graph, const GraphData &Data, const Config &Conf,
              VertexDescriptor SourceVertex);

#endif

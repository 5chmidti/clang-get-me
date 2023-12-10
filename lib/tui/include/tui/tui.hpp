#ifndef get_me_tool_include_tool_tui_hpp
#define get_me_tool_include_tool_tui_hpp

#include <memory>

#include <clang/Tooling/Tooling.h>

#include "get_me/config.hpp"

void runTui(std::shared_ptr<Config> Conf, clang::tooling::ClangTool &Tool);

#endif

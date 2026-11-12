#pragma once
#include "project.hpp"
#include <sstream>

namespace studio {
fs::path nativeDataPath(std::string_view name);
fs::path nativeOutputPath(const Project& project, const std::string& name);
std::string nativeCommand(std::string_view line, std::istringstream& tokens);
std::string nativeReference(std::string path);
}

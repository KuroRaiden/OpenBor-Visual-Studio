#pragma once
#include "project.hpp"

namespace studio {
std::string mediaExtension(const fs::path& file);
void validateAudio(const fs::path& file);
void validateScene(const fs::path& file);
void copyMedia(const fs::path& source, const fs::path& output);
void exportScene(const ScreenScene& scene, const fs::path& root, const std::string& name,
                 const std::string& folder = "data/scenes/");
}

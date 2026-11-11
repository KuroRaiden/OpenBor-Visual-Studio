#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <array>

namespace studio {
namespace fs = std::filesystem;

struct Box {
    bool enabled = false;
    int x = 0, y = 0, width = 16, height = 24, depth = 10;
    int damage = 12;
};

struct Frame {
    fs::path image;
    int ticks = 8;
    int originX = 0, originY = 0;
    Box body, attack;
};

struct SpawnCue {
    bool enabled = false;
    int frame = 0;
    std::string model;
    int x = 18, z = 0, altitude = 20;
    float velocityX = 2.0f, velocityZ = 0.0f, velocityY = 0.0f;
};

struct Animation {
    std::string name = "idle";
    bool loop = true;
    std::vector<Frame> frames;
    SpawnCue projectile, item;
};

struct Character {
    std::string id = "fighter";
    std::string title = "Fighter";
    std::string type = "player";
    int health = 100;
    float speed = 2.0f;
    std::vector<Animation> animations;
};

struct StageSpawn {
    std::string model;
    int at = 0, x = 260, z = 192, altitude = 0;
    bool flip = false, boss = false;
    std::string item;
};

struct Wave {
    int at = 0;
    bool wait = true;
};

struct Stage {
    std::string title = "Untitled stage";
    fs::path background;
    std::vector<fs::path> panels;
    int laneTop = 160, laneBottom = 230;
    int playerX = 72, playerZ = 195;
    std::vector<StageSpawn> spawns;
    std::vector<Wave> waves;
};

struct Project {
    std::string title = "Untitled project";
    std::vector<Character> characters;
    std::vector<Stage> stages;
};

struct Pixels;
class Images;

std::string utf8(const fs::path& path);
fs::path pathFromUtf8(const std::string& path);
std::string modelToken(const std::string& name);
std::string lower(std::string value);
const Character* findCharacter(const Project& project, const std::string& id);
const Frame* previewFrame(const Character* character);
bool hasFrames(const Character& character);
Frame makeFrame(const fs::path& path, Images& images);

Project openProject(const fs::path& file);
void saveProject(const Project& project, const fs::path& file);
void renameCharacter(Project& project, size_t index, const std::string& id);
Character importSpriteFolder(const fs::path& folder, const std::string& direction, Images& images);
Character importCharacterFile(const fs::path& file, Images& images, int& skipped);
std::string modelText(const Character& character);
std::string stageText(const Stage& stage, size_t index);
fs::path exportProject(const Project& project, const fs::path& parent, Images& images);
void verifyProject(const Project& project);
}

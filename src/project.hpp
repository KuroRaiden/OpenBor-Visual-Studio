#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstdint>
#include <span>
#include <map>

namespace studio {
namespace fs = std::filesystem;

struct Box {
    bool enabled = false;
    int x = 0, y = 0, width = 16, height = 24, depth = 10;
    int damage = 12;
};

struct DrawEffects {
    bool enabled = false, flipX = false, flipY = false, embeddedAlpha = false;
    int scaleX = 256, scaleY = 256, rotation = 0, blend = 0;
    fs::path mask;
};

struct Frame {
    fs::path image;
    fs::path sound;
    int ticks = 8;
    int originX = 0, originY = 0;
    Box body, attack;
    DrawEffects effects;
    std::string extraCommands;
};

struct SpawnCue {
    bool enabled = false;
    int frame = 0;
    std::string model;
    int x = 18, z = 0, altitude = 20;
};

struct Animation {
    std::string name = "idle";
    bool loop = true;
    std::vector<Frame> frames;
    SpawnCue subEntity, item;
    int meshClip = -1;
    std::string extraCommands;
};

struct EntitySettings {
    int mp = -1, aggression = -1, score = -1;
    float jumpHeight = -1, lifespan = -1;
    int wall = -1, gravity = -1;
    float jumpSpeed = -1, runningJumpHeight = -1, runningJumpDistance = -1;
};

struct ColorSet {
    std::string name;
    std::vector<uint32_t> colors;
};

enum class MeshAxis { x, y, z, negativeX, negativeY, negativeZ };

struct MeshSettings {
    fs::path file;
    MeshAxis up = MeshAxis::y, forward = MeshAxis::z;
    float scale = 1, pitch = 0, yaw = 0, roll = 0;
    bool planar = true;
    bool operator==(const MeshSettings&) const = default;
};

inline constexpr int meshFrameWidth = 64, meshFrameHeight = 64;

struct Character {
    std::string id = "fighter";
    std::string title = "Fighter";
    std::string type = "player";
    int health = 100;
    float speed = 2.0f;
    std::vector<Animation> animations;
    EntitySettings entity;
    fs::path icon;
    fs::path deathSound;
    std::vector<uint32_t> sourcePalette;
    std::vector<ColorSet> colorSets;
    std::string extraHeader;
    MeshSettings mesh;
    std::string nativeFile;
};

struct StageSpawn {
    std::string model;
    int at = 0, x = 260, z = 192, altitude = 0;
    bool flip = false, boss = false;
    std::string item;
    int health = 0, mp = 0, palette = -1;
};

struct Wave {
    int at = 0;
    bool wait = true;
};

struct ScreenPoint { int x = 0, y = 0; };
struct ImageSize { int width = 0, height = 0; };

struct ScreenScene {
    fs::path file;
    ScreenPoint position;
    bool skippable = true;
};

struct StagePanel {
    fs::path image;
    ScreenPoint offset;
};

struct StageTerrain {
    bool pit = false;
    int x = 100, z = 230, width = 64, depth = 16, altitude = 32, type = 0;
    bool operator==(const StageTerrain&) const = default;
};

inline constexpr int stageTerrainLimit = 99999;

struct StageCamera {
    int zoom = 100;
    int offsetX = 0, offsetY = 0;
    bool followAltitude = false;
    bool nativeZoom = false;
    bool operator==(const StageCamera&) const = default;
};

inline constexpr int cameraZoomMin = 50, cameraZoomMax = 200, cameraOffsetLimit = 4096;

struct Stage {
    std::string title = "Untitled stage";
    fs::path background;
    ScreenPoint backgroundOffset;
    bool repeatBackgroundX = true, repeatBackgroundY = true;
    std::vector<StagePanel> panels;
    std::vector<StageTerrain> terrain;
    StageCamera camera;
    int laneTop = 160, laneBottom = 230;
    int backgroundDepth = -1;
    int playerX = 72, playerZ = 195;
    std::vector<StageSpawn> spawns;
    std::vector<Wave> waves;
    std::string extraHeader;
    fs::path music;
    int musicOffset = 0;
    ScreenScene before, after;
    std::string nativeFile;
    std::string nativeBeforeFile, nativeAfterFile;
};

inline constexpr int stageArtworkOffsetLimit = 99999;

struct StageArtLayout {
    std::vector<ScreenPoint> positions;
    int panelWidth = 0, panelHeight = 0, panelCount = 0;
    bool composite = false;
    int width() const { return panelWidth*panelCount; }
};

StageArtLayout stageArtLayout(const Stage& stage, std::span<const ImageSize> sizes);

struct Gauge {
    int width = 100, height = 5;
    bool ratio = false, vertical = false, noBorder = false;
    int border = 0, shadow = 0, fill = 0, backfill = 0;
};

struct HudPlayer {
    ScreenPoint life, mp, icon, score, enemy;
};

struct HudLayout {
    bool enabled = false, timerBorder = true;
    Gauge life, mp, enemy;
    ScreenPoint timer{149,4};
    int timerWidth = 21, timerHeight = 20, timerFont = 3;
    fs::path overlay;
    ScreenPoint overlayPosition;
    std::array<HudPlayer,4> players;
    HudLayout();
};

struct SelectSlot {
    ScreenPoint actor, ready;
    bool right = true;
};

struct SelectScreen {
    bool enabled = false, colorSelect = false;
    fs::path background;
    std::array<SelectSlot,4> slots{{
        {{48,194},{33,203},true},{{124,194},{109,203},false},
        {{200,194},{185,203},true},{{276,194},{261,203},false}
    }};
};

struct ScreenFont {
    fs::path bitmap;
    uint32_t color = 0xfff9f4e7u;
    bool tintBitmap = false;
    bool operator==(const ScreenFont&) const = default;
};

inline constexpr std::array screenBackgroundNames{"logo","title","titleb"};
inline constexpr std::array screenFontNames{"font","font2","font3","font4"};
inline constexpr std::array screenSceneNames{"logo","intro","howto"};

struct Screens {
    std::array<fs::path,screenBackgroundNames.size()> backgrounds;
    std::array<ScreenFont,screenFontNames.size()> fonts{{
        {},{{},0xffffd269u},{{},0xffabdff5u},{}
    }};
    std::array<ScreenScene,screenSceneNames.size()> scenes;
    fs::path music;
};

struct ModuleSettings {
    HudLayout hud;
    SelectScreen select;
    Screens screens;
    int maxPlayers = 2;
    std::string modelsHeader, levelsHeader, videoHeader, levelSetCommands;
};

struct Project {
    std::string title = "Untitled project";
    std::vector<Character> characters;
    std::vector<Stage> stages;
    ModuleSettings module;
    std::map<std::string,fs::path> nativeFiles;
    std::map<std::string,std::string> nativeNames;
    std::map<std::string,std::string> nativeEdits;
    std::map<std::string,std::string> nativeBaseline;
};

struct Pixels;
class Images;
class GlbModel;

std::string utf8(const fs::path& path);
fs::path pathFromUtf8(const std::string& path);
std::string modelToken(const std::string& name);
std::string animationId(std::string_view name);
std::string lower(std::string value);
const Character* findCharacter(const Project& project, const std::string& id);
const Frame* previewFrame(const Character* character);
ImageSize stageVideoSize(const Project& project);
bool hasFrames(const Character& character);
Frame makeFrame(const fs::path& path, Images& images);

Project openProject(const fs::path& file, bool verifyAssets = false);
Project importModule(const fs::path& folder, Images& images, int& skipped);
fs::path unpackModule(const fs::path& pak, const fs::path& parent);
std::string nativeFileText(const Project& project, const std::string& name);
void captureNativeBaseline(Project& project);
void saveProject(const Project& project, const fs::path& file);
void renameCharacter(Project& project, size_t index, const std::string& id);
Character importSpriteFolder(const fs::path& folder, const std::string& direction, Images& images);
Character importCharacterFile(const fs::path& file, Images& images, int& skipped);
std::string modelText(const Character& character, const GlbModel* mesh = nullptr, std::string assetRoot = {});
std::string stageText(const Stage& stage, size_t index, const Images& images);
std::string moduleModelsText(const ModuleSettings& module);
std::string moduleLevelsText(const ModuleSettings& module);
std::string modelsConfigurationText(const Project& project);
std::string levelsConfigurationText(const Project& project);
Screens loadScreenTemplate(const fs::path& folder);
enum class ExportDestination { newFolder, existingPak, existingData };
struct StagePlaytest { size_t stage; std::string player; };
fs::path exportProject(const Project& project, const fs::path& target, Images& images, bool playable = false,
                       ExportDestination destination = ExportDestination::newFolder,
                       const StagePlaytest* playtest = nullptr);
fs::path existingExportData(const fs::path& target);
void verifyProject(const Project& project);
}

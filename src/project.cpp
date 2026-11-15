#include "project.hpp"
#include "images.hpp"
#include "mesh.hpp"
#include "media.hpp"
#include "native.hpp"

#include <windows.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace studio {
using nlohmann::json;
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Box, enabled, x, y, width, height, depth, damage)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DrawEffects, enabled, flipX, flipY, embeddedAlpha, scaleX, scaleY, rotation, blend, mask)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Frame, image, sound, ticks, originX, originY, body, attack, effects, extraCommands)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SpawnCue, enabled, frame, model, x, z, altitude)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Animation, name, loop, frames, subEntity, item, meshClip, extraCommands)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EntitySettings, mp, aggression, score, jumpHeight, lifespan, wall, gravity, jumpSpeed, runningJumpHeight, runningJumpDistance)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ColorSet, name, colors)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MeshSettings, file, up, forward, scale, pitch, yaw, roll, planar)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Character, id, title, type, health, speed, animations, entity, icon, deathSound, sourcePalette, colorSets, extraHeader, mesh, nativeFile)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StageSpawn, model, at, x, z, altitude, flip, boss, item, health, mp, palette)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Wave, at, wait)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StageTerrain, pit, x, z, width, depth, altitude, type)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StageCamera, zoom, offsetX, offsetY, followAltitude, nativeZoom)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScreenPoint, x, y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Gauge, width, height, ratio, vertical, noBorder, border, shadow, fill, backfill)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HudPlayer, life, mp, icon, score, enemy)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(HudLayout, enabled, timerBorder, life, mp, enemy, timer, timerWidth, timerHeight, timerFont, overlay, overlayPosition, players)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SelectSlot, actor, ready, right)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SelectScreen, enabled, colorSelect, background, slots)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ScreenFont, bitmap, color, tintBitmap)
void to_json(json& value, const ScreenScene& scene) {
    value = {{"file",scene.file},{"position",scene.position},{"skippable",scene.skippable}};
}
void from_json(const json& value, ScreenScene& scene) {
    scene.file = value.contains("file") ? value.at("file").get<fs::path>() : value.value("gif",fs::path{});
    scene.position = value.value("position",ScreenPoint{});
    scene.skippable = value.value("skippable",true);
}
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Screens, backgrounds, fonts, scenes, music)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModuleSettings, hud, select, screens, maxPlayers, modelsHeader, levelsHeader, videoHeader, levelSetCommands)

void exportModuleArtwork(const Project& project, const fs::path& root, Images& images, bool playable);
fs::path writePlayablePak(const fs::path& root, const std::string& title);

void to_json(json& value, const StagePanel& panel) {
    if (panel.offset.x || panel.offset.y) value = {{"image",panel.image},{"offset",panel.offset}};
    else value = panel.image;
}

void from_json(const json& value, StagePanel& panel) {
    if (value.is_string()) {
        panel = {value.get<fs::path>(),{}};
    } else {
        panel = {value.at("image").get<fs::path>(),value.value("offset",ScreenPoint{})};
    }
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Stage, title, background, backgroundOffset, repeatBackgroundX, repeatBackgroundY, panels, terrain, camera, laneTop, laneBottom, backgroundDepth, playerX, playerZ, spawns, waves, extraHeader, music, musicOffset, before, after, nativeFile, nativeBeforeFile, nativeAfterFile)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Project, title, characters, stages, module, nativeFiles, nativeNames, nativeEdits, nativeBaseline)

namespace {

void writeText(const fs::path& path, const std::string& data) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(data.data(), std::streamsize(data.size()));
    file.close();
    if (!file) throw std::runtime_error("Could not write " + utf8(path));
}

std::string readText(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open " + utf8(path));
    if (fs::file_size(path) > 80'000'000) throw std::runtime_error("File is too large for this editor.");
    return {std::istreambuf_iterator<char>(file), {}};
}

template<class Callback>
void visitAssets(Project& project, Callback&& callback) {
    const auto optional = [&](fs::path& path) { if (!path.empty()) callback(path); };
    optional(project.module.hud.overlay);
    optional(project.module.select.background);
    for (auto& background : project.module.screens.backgrounds) optional(background);
    for (auto& font : project.module.screens.fonts) optional(font.bitmap);
    for (auto& scene : project.module.screens.scenes) optional(scene.file);
    optional(project.module.screens.music);
    for (auto& model : project.characters) {
        optional(model.icon);
        optional(model.deathSound);
        optional(model.mesh.file);
        for (auto& animation : model.animations) {
            for (auto& frame : animation.frames) {
                optional(frame.image);
                optional(frame.sound);
                optional(frame.effects.mask);
            }
        }
    }
    for (auto& stage : project.stages) {
        if (!stage.background.empty()) callback(stage.background);
        for (auto& panel : stage.panels) callback(panel.image);
        optional(stage.music);
        optional(stage.before.file);
        optional(stage.after.file);
    }
    for (auto& [name,file] : project.nativeFiles) callback(file);
}

std::string counterName(std::string_view prefix, size_t index, std::string_view suffix) {
    std::ostringstream name;
    name << prefix << std::setw(3) << std::setfill('0') << index << suffix;
    return name.str();
}

std::string imageName(size_t animation, size_t frame) {
    return counterName("a", animation, "_") + counterName("f", frame, ".png");
}

std::string maskName(size_t animation, size_t frame) { return "mask_" + imageName(animation,frame); }

std::string modelPath(const Character& character) {
    return "data/chars/" + character.id + "/" + character.id + ".txt";
}

std::string levelPath(size_t index) {
    return "data/levels/" + counterName("stage_", index + 1, ".txt");
}

std::string artPath(size_t index) {
    return "data/bgs/" + counterName("stage_", index + 1, "/");
}

std::string stageMusicPath(const Stage& stage, size_t index) {
    return "data/music/" + counterName("studio_stage_",index+1,mediaExtension(stage.music));
}

std::string stageSceneName(size_t index, bool after) {
    return counterName("studio_stage_",index+1,after ? "_after" : "_before");
}

std::map<fs::path,std::string> soundPaths(const Character& model, const std::string& root) {
    std::map<fs::path,std::string> paths;
    const auto add = [&](const fs::path& file) {
        if (!file.empty()) paths.try_emplace(file,root+counterName("studio_sound_",paths.size()+1,mediaExtension(file)));
    };
    add(model.deathSound);
    for (const auto& animation : model.animations)
        for (const auto& frame : animation.frames) add(frame.sound);
    return paths;
}

std::string csvField(std::string_view line, size_t& i) {
    std::string field;
    bool quoted = i < line.size() && line[i] == '"';
    if (quoted) ++i;
    while (i < line.size()) {
        const char c = line[i++];
        if (quoted && c == '"') {
            if (i < line.size() && line[i] == '"') {
                ++i;
                field += '"';
            } else {
                quoted = false;
            }
        } else if (!quoted && c == ',') {
            break;
        } else {
            field += c;
        }
    }
    return field;
}

std::vector<std::string> csvRow(std::string line) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::vector<std::string> row;
    size_t i = 0;
    while (i < line.size()) row.push_back(csvField(line, i));
    if (!line.empty() && line.back() == ',') row.emplace_back();
    return row;
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool validToken(const std::string& value) {
    return !value.empty() && value.size() <= 40 && value == modelToken(value);
}

bool knownType(const std::string& type) {
    static const std::set<std::string> types{
        "player", "enemy", "npc", "item", "projectile", "obstacle", "none", "shot"};
    return types.contains(type);
}

void validateCue(const SpawnCue& cue, const Animation& animation, const Project& project) {
    if (!cue.enabled) return;
    require(cue.frame >= 0 && size_t(cue.frame) < animation.frames.size(),
            "Set an existing trigger frame for " + animation.name + ".");
    const Character* model = findCharacter(project, cue.model);
    require(model && hasFrames(*model), "Select a model with frames for the cue in " + animation.name + ".");
}

void migrateSpawnSettings(json& animation) {
    if (auto old = animation.find("projectile"); old != animation.end()) {
        if (!animation.contains("subEntity")) animation["subEntity"] = old->get<SpawnCue>();
        animation.erase("projectile");
        if (auto item = animation.find("item"); item != animation.end()) *item = item->get<SpawnCue>();
    }
}

constexpr std::string_view itemCueMarker = "# OpenBOR Studio item cue ";

bool readItemCue(std::string_view line, SpawnCue& cue) {
    const auto start = line.find_first_not_of(" \t");
    if (start == line.npos || !line.substr(start).starts_with(itemCueMarker)) return false;
    std::istringstream tokens{std::string(line.substr(start+itemCueMarker.size()))};
    cue.enabled = bool(tokens >> cue.model >> cue.frame >> cue.x >> cue.z >> cue.altitude);
    return cue.enabled;
}

bool legacySpawnCommand(std::string_view command) {
    return command == "custknife" || command == "throwframe" ||
           command.starts_with("projectile_position_") || command.starts_with("projectile_velocity_");
}

void writeSpawnCues(std::ostream& out, const Animation& animation) {
    const auto& primary = animation.subEntity.enabled ? animation.subEntity : animation.item;
    if (!primary.enabled) return;
    require(validToken(primary.model),"Select a Sub Entity model for " + animation.name + ".");
    out << "    subentity " << primary.model << "\n    spawnframe "
        << primary.frame << ' ' << primary.x << ' ' << primary.z << ' ' << primary.altitude << " 0\n";
    if (!animation.subEntity.enabled || !animation.item.enabled) return;

    // The native animation can reference one model. Keep the separate item cue via the animation script.
    const auto& item = animation.item;
    require(validToken(item.model),"Select an item model for " + animation.name + ".");
    out << "    " << itemCueMarker << item.model << ' ' << item.frame << ' ' << item.x << ' '
        << item.z << ' ' << item.altitude << "\n"
        << "    @script\n"
        << "    if (frame == " << item.frame << ") {\n"
        << "        void studioParent = getlocalvar(\"self\");\n"
        << "        int studioFacing = getentityproperty(studioParent, \"direction\");\n"
        << "        float studioX = getentityproperty(studioParent, \"x\") + (2 * studioFacing - 1) * (" << item.x << ");\n"
        << "        float studioZ = getentityproperty(studioParent, \"z\") + (" << item.z << ");\n"
        << "        float studioY = getentityproperty(studioParent, \"a\") + (" << item.altitude << ");\n"
        << "        clearspawnentry();\n"
        << "        setspawnentry(\"name\", \"" << item.model << "\");\n"
        << "        setspawnentry(\"flip\", studioFacing);\n"
        << "        float studioScrollX = 0;\n"
        << "        if (openborvariant(\"in_level\")) studioScrollX = openborvariant(\"xpos\");\n"
        << "        setspawnentry(\"coords\", studioX - studioScrollX, studioZ, studioY);\n"
        << "        void studioItem = spawn();\n"
        << "        if (studioItem) {\n"
        << "            changeentityproperty(studioItem, \"position\", studioX, studioZ, studioY);\n"
        << "            changeentityproperty(studioItem, \"parent\", studioParent);\n"
        << "        }\n"
        << "    }\n"
        << "    @end_script\n";
}

int freeSpecial(const std::string& name) {
    constexpr std::string_view prefix = "freespecial";
    if (!name.starts_with(prefix) || name.size() == prefix.size()) return 0;
    const auto tail = name.substr(prefix.size());
    if (tail.size() > 3 || !std::all_of(tail.begin(),tail.end(),[](char c){return c >= '0' && c <= '9';})) return 0;
    return std::stoi(tail);
}

fs::path findGameRoot(const fs::path& modelPath) {
    for (auto dir = modelPath.parent_path(); !dir.empty() && dir != dir.root_path(); dir = dir.parent_path()) {
        if (lower(utf8(dir.filename())) == "data") return dir.parent_path();
    }
    return {};
}

fs::path resolveFrame(const fs::path& modelFile, const fs::path& game, const std::string& relative) {
    fs::path path = pathFromUtf8(relative);
    if (!path.is_absolute()) {
        path = lower(relative).starts_with("data/") || lower(relative).starts_with("data\\")
            ? (game.empty() ? modelFile.parent_path() : game) / path
            : modelFile.parent_path() / path;
    }
    if (!path.has_extension()) {
        for (const auto ext : {".png", ".gif", ".bmp"}) {
            auto candidate = path;
            candidate += ext;
            if (fs::exists(candidate)) return candidate.lexically_normal();
        }
    }
    return path.lexically_normal();
}

} // namespace

std::string utf8(const fs::path& path) {
    const auto bytes = path.generic_u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

fs::path pathFromUtf8(const std::string& value) {
    return fs::path(std::u8string(value.begin(), value.end()));
}

std::string lower(std::string value) {
    for (auto& c : value) c = char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::string modelToken(const std::string& name) {
    std::string result;
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (std::isalnum(u) && u < 128) result += char(std::tolower(u));
        else if ((c == '-' || c == '_' || c == ' ') && !result.empty() && result.back() != '_') result += '_';
        if (result.size() == 40) break;
    }
    if (result.ends_with('_')) result.pop_back();
    return result;
}

std::string animationId(std::string_view name) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto first = name.find_first_not_of(whitespace);
    if (first == std::string_view::npos) return {};
    auto id = lower(std::string(name.substr(first,name.find_last_not_of(whitespace)-first+1)));
    if (id == "die") id = "death";
    return id;
}

const Character* findCharacter(const Project& project, const std::string& id) {
    const auto key = lower(id);
    const auto it = std::find_if(project.characters.begin(), project.characters.end(),
                                 [&](const Character& c) { return lower(c.id) == key; });
    return it == project.characters.end() ? nullptr : &*it;
}

const Frame* previewFrame(const Character* character) {
    if (!character) return nullptr;
    for (const auto& animation : character->animations) {
        if (!animation.frames.empty() && animationId(animation.name) == "idle") return &animation.frames.front();
    }
    for (const auto& animation : character->animations) {
        if (!animation.frames.empty()) return &animation.frames.front();
    }
    return nullptr;
}

bool hasFrames(const Character& character) { return previewFrame(&character) != nullptr; }

Frame makeFrame(const fs::path& path, Images& images) {
    const auto pixels = images.read(path);
    Frame frame;
    frame.image = fs::absolute(path).lexically_normal();
    frame.originX = pixels.width / 2;
    frame.originY = pixels.height - 1;
    frame.body.width = std::max(1, pixels.width / 2);
    frame.body.height = std::max(1, pixels.height * 3 / 4);
    frame.body.x = pixels.width / 4;
    frame.body.y = std::max(0, frame.originY - frame.body.height);
    return frame;
}

Project openProject(const fs::path& file, bool verifyAssets) {
    const auto root = fs::absolute(file).parent_path();
    auto data = json::parse(readText(file));
    require(data.value("version", 0) == 1, "Unsupported OpenBOR Studio project version.");
    for (auto& model : data.at("project")["characters"])
        for (auto& animation : model["animations"]) migrateSpawnSettings(animation);
    auto project = data.at("project").get<Project>();
    for (const auto& model : project.characters) {
        const auto baseline = project.nativeBaseline.find(model.nativeFile);
        if (baseline == project.nativeBaseline.end()) continue;
        auto original = json::parse(baseline->second);
        for (auto& animation : original["animations"]) migrateSpawnSettings(animation);
        baseline->second = original.dump();
    }
    for (const auto& [name,asset] : project.nativeFiles) (void)nativeDataPath(name);
    for (const auto& [name,original] : project.nativeNames) {
        require(nativeReference(original) == name && project.nativeFiles.contains(name),"Invalid native module path.");
        (void)nativeDataPath(original);
    }
    visitAssets(project, [&](fs::path& asset) {
        require(!asset.empty() && !asset.has_root_path(), "Project asset path must be relative.");
        const auto relative = asset.lexically_normal();
        require(relative.begin() != relative.end() && *relative.begin() != L"..",
                "Project asset path points outside its directory.");
        asset = root / relative;
        if (verifyAssets) require(fs::is_regular_file(asset),"Missing recovery asset: "+utf8(relative));
    });
    for (size_t i = 0; i < project.stages.size(); ++i) {
        auto& stage = project.stages[i];
        if (!stage.nativeFile.empty() && project.nativeFiles.contains(stage.nativeFile) &&
            !data.at("project").at("stages")[i].contains("camera")) {
            if (fs::is_regular_file(project.nativeFiles.at(stage.nativeFile))) {
                stage.camera = readStageCamera(readText(project.nativeFiles.at(stage.nativeFile)));
                if (auto baseline = project.nativeBaseline.find(stage.nativeFile); baseline != project.nativeBaseline.end()) {
                    auto baselineData = json::parse(baseline->second);
                    baselineData["camera"] = stage.camera;
                    baseline->second = baselineData.dump();
                }
            }
        }
    }
    return project;
}

ImageSize stageVideoSize(const Project& project) {
    constexpr std::array<ImageSize,7> modes{{{320,240},{480,272},{640,480},{720,480},
                                             {800,480},{800,600},{960,540}}};
    auto size = modes.front();
    const auto apply = [&](const std::string& source) {
        std::istringstream input(source), tokens;
        for (std::string line; std::getline(input,line);) {
            const auto command = nativeCommand(line,tokens);
            if (command != "video" && command != "videomode") continue;
            int width, height;
            char separator;
            if (!(tokens >> width)) continue;
            if (tokens >> separator >> height && (separator == 'x' || separator == 'X')) {
                if (width >= 4 && width <= 8192 && height >= 4 && height <= 8192) size = {width & ~3,height};
            } else if (width >= 0 && width < int(modes.size())) size = modes[size_t(width)];
        }
    };
    if (project.nativeFiles.contains("data/video.txt")) {
        try { apply(nativeFileText(project,"data/video.txt")); }
        catch (const std::exception&) {}
    }
    if (!project.nativeEdits.contains("data/video.txt")) apply(project.module.videoHeader);
    return size;
}

void saveProject(const Project& project, const fs::path& file) {
    const auto full = fs::absolute(file);
    fs::path assetDir = full.stem();
    assetDir += L".assets";
    Project portable = project;
    std::map<fs::path, fs::path> copied;
    std::map<std::string,fs::path> nativeCopies;
    auto nativeDir = assetDir/"module";
    if (std::any_of(project.nativeFiles.begin(),project.nativeFiles.end(),[&](const auto& item) {
        const auto output = full.parent_path()/nativeDir/nativeOutputPath(project,item.first);
        return fs::exists(output) && !fs::equivalent(item.second,output);
    })) nativeDir += pathFromUtf8("_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    for (const auto& [name,source] : project.nativeFiles) {
        const auto saved = source.lexically_relative(full.parent_path());
        const auto relative = !saved.empty() && *saved.begin() == assetDir
            ? saved : nativeDir/nativeOutputPath(project,name);
        const auto output = full.parent_path()/relative;
        if (!fs::exists(output) || !fs::equivalent(source,output)) {
            fs::create_directories(output.parent_path());
            fs::copy_file(source,output);
        }
        nativeCopies.emplace(nativeReference(utf8(source.lexically_normal())),relative);
    }
    visitAssets(portable, [&](fs::path& asset) {
        if (const auto found = nativeCopies.find(nativeReference(utf8(asset.lexically_normal())));
            found != nativeCopies.end()) {
            asset = found->second;
            return;
        }
        if (const auto found = copied.find(asset); found != copied.end()) {
            asset = found->second;
            return;
        }
        std::ifstream input(asset,std::ios::binary);
        require(bool(input),"Could not open " + utf8(asset));
        uint64_t hash = 14695981039346656037ull;
        std::array<char,65536> chunk{};
        while (input.read(chunk.data(),chunk.size()) || input.gcount()) {
            for (std::streamsize i = 0; i < input.gcount(); ++i) {
                hash ^= static_cast<unsigned char>(chunk[size_t(i)]);
                hash *= 1099511628211ull;
            }
        }
        require(input.eof() && !input.bad(),"Could not read " + utf8(asset));
        std::ostringstream key;
        key << std::hex << hash;
        auto relative = assetDir / pathFromUtf8(key.str() + lower(utf8(asset.extension())));
        const auto output = full.parent_path() / relative;
        if (!fs::exists(output)) {
            fs::create_directories(output.parent_path());
            fs::copy_file(asset,output);
        } else if (!fs::equivalent(asset,output)) {
            std::ifstream original(asset,std::ios::binary), existing(output,std::ios::binary);
            require(original && existing && fs::file_size(asset) == fs::file_size(output) &&
                std::equal(std::istreambuf_iterator<char>(original),{},std::istreambuf_iterator<char>(existing),{}),
                "Asset fingerprint collision.");
        }
        copied.emplace(asset, relative);
        asset = relative;
    });
    fs::path temporary = full;
    temporary += L".tmp";
    writeText(temporary, json{{"version",1},{"project",portable}}.dump(2) + "\n");
    if (!MoveFileExW(temporary.c_str(), full.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::runtime_error("Could not replace project file " + utf8(full));
    }
}

void renameCharacter(Project& project, size_t index, const std::string& proposed) {
    auto& character = project.characters.at(index);
    const auto id = modelToken(proposed);
    require(validToken(id), "Enter a model ID using letters, digits, or underscores.");
    if (id == character.id) return;
    require(!findCharacter(project, id), "This model ID is already in use.");
    const std::string oldId = character.id;
    const auto rename = [&](std::string& reference) {
        if (reference == oldId) reference = id;
    };
    for (auto& model : project.characters) {
        for (auto& animation : model.animations) {
            rename(animation.subEntity.model);
            rename(animation.item.model);
        }
    }
    for (auto& stage : project.stages) {
        for (auto& spawn : stage.spawns) {
            rename(spawn.model);
            rename(spawn.item);
        }
    }
    character.id = id;
}

Character importSpriteFolder(const fs::path& folder, const std::string& direction, Images& images) {
    const auto root = fs::absolute(folder).lexically_normal();
    Character model;
    model.title = utf8(root.filename());
    if (const auto bracket = model.title.rfind(" ["); bracket != std::string::npos && model.title.ends_with(']')) {
        model.title.resize(bracket);
    }
    model.id = modelToken(model.title);
    if (model.id.empty()) model.id = "sprite";
    using IndexedFrames = std::map<int, Frame>;
    std::map<int, IndexedFrames> groups;
    const auto manifest = root.parent_path() / "frames.csv";
    if (fs::exists(manifest)) {
        std::istringstream input(readText(manifest));
        std::string line;
        std::getline(input, line);
        const auto columns = csvRow(line);
        const auto index = [&](std::string_view field) {
            return size_t(std::find(columns.begin(), columns.end(), field) - columns.begin());
        };
        const auto path = index("png"), facing = index("direction"), animation = index("animation");
        const auto frame = index("frame"), x = index("offset_x"), y = index("offset_y");
        const auto highest = std::max({path,facing,animation,frame,x,y});
        while (std::getline(input, line)) {
            const auto row = csvRow(line);
            if (row.size() <= highest || row[facing] != direction) continue;
            const auto asset = (manifest.parent_path() / pathFromUtf8(row[path])).lexically_normal();
            if (asset.parent_path().parent_path() != root) continue;
            Frame sprite = makeFrame(asset, images);
            sprite.originX = -std::stoi(row[x]);
            sprite.originY = -std::stoi(row[y]);
            groups[std::stoi(row[animation])].emplace(std::stoi(row[frame]), std::move(sprite));
        }
    }
    if (groups.empty()) {
        auto framesDir = root / "frames";
        if (!fs::is_directory(framesDir)) framesDir = root;
        std::vector<fs::path> paths;
        for (const auto& entry : fs::directory_iterator(framesDir)) {
            if (entry.is_regular_file() && lower(utf8(entry.path().extension())) == ".png") paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end());
        for (const auto& path : paths) {
            const auto stem = utf8(path.stem());
            const auto first = stem.find('_'), last = stem.rfind('_');
            if (first == last || first == std::string::npos) {
                if (framesDir == root && (stem == "preview" || stem == "sprite-sheet" || stem.starts_with("animation_"))) continue;
                auto& idle = groups[0];
                idle.emplace(int(idle.size()), makeFrame(path, images));
                continue;
            }
            const auto side = stem.substr(first+1, last-first-1);
            if (side != direction) continue;
            int group = 0, frame = 0;
            try {
                group = std::stoi(stem.substr(0,first));
                frame = std::stoi(stem.substr(last+1));
            } catch (const std::invalid_argument&) {
                continue;
            }
            groups[group].emplace(frame, makeFrame(path, images));
        }
    }
    require(!groups.empty(), "This folder has no individual PNG frames for the selected facing.");
    size_t ordinal = 0;
    for (auto& [group, frames] : groups) {
        Animation animation;
        animation.name = ordinal == 0 ? "idle" : group == 1 ? "walk" : "freespecial" + std::to_string(group);
        animation.loop = animation.name == "idle" || animation.name == "walk";
        for (auto& [frame, sprite] : frames) {
            (void)frame;
            animation.frames.push_back(std::move(sprite));
        }
        model.animations.push_back(std::move(animation));
        ++ordinal;
    }
    return model;
}

Character importCharacterFile(const fs::path& file, Images& images, int& skipped) {
    Character character;
    character.id = modelToken(utf8(file.stem()));
    character.title = utf8(file.stem());
    const auto root = findGameRoot(file);
    std::unique_ptr<GlbModel> mesh;
    Animation* animation = nullptr;
    Frame properties;
    const auto resolveSound = [&](const std::string& name) -> fs::path {
        if (lower(name) == "none") return {};
        auto path = resolveFrame(file,root,name);
        if (fs::is_regular_file(path)) return path;
        for (const auto extension : {".wav",".ogg",".oga"}) {
            const auto candidate = pathFromUtf8(utf8(path)+extension);
            if (!path.has_extension() && fs::is_regular_file(candidate)) return candidate;
        }
        ++skipped;
        return {};
    };
    bool explicitOrigin = false;
    std::istringstream input(readText(file));
    std::string line;
    skipped = 0;
    bool inlineScript = false;
    bool nextItemScript = false;
    while (std::getline(input, line)) {
        SpawnCue item;
        if (!inlineScript && animation && readItemCue(line,item)) {
            if (!animation->subEntity.enabled && animation->item.enabled)
                animation->subEntity = std::move(animation->item);
            animation->item = std::move(item);
            nextItemScript = true;
            continue;
        }
        line.resize(std::min(line.find('#'), line.size()));
        std::istringstream tokens(line);
        std::string command, value;
        if (!(tokens >> command)) continue;
        command = lower(command);
        if (command == "@script" || (command == "animationscript" && line.find("@script") != line.npos)) {
            inlineScript = true;
            if (!nextItemScript) ++skipped;
            nextItemScript = false;
            continue;
        }
        if (inlineScript) {
            if (command == "@end_script") inlineScript = false;
            continue;
        }
        if (command == "name") {
            tokens >> character.title;
            character.id = modelToken(character.title);
        } else if (command == "type") {
            tokens >> value;
            value = lower(value);
            if (knownType(value)) character.type = value;
            else ++skipped;
        } else if (command == "health") {
            tokens >> character.health;
        } else if (command == "speed") {
            tokens >> character.speed;
        } else if (command == "mp") {
            tokens >> character.entity.mp;
        } else if (command == "aggression") {
            tokens >> character.entity.aggression;
        } else if (command == "score") {
            tokens >> character.entity.score;
        } else if (command == "jumpheight" || command == "jump_height") {
            tokens >> character.entity.jumpHeight;
        } else if (command == "jumpspeed" || command == "jumpspeedf" || command == "jump_speed") {
            if (tokens >> character.entity.jumpSpeed && command == "jumpspeed") character.entity.jumpSpeed /= 10;
        } else if (command == "lifespan") {
            tokens >> character.entity.lifespan;
        } else if (command == "subject_to_wall") {
            tokens >> character.entity.wall;
        } else if (command == "subject_to_gravity") {
            tokens >> character.entity.gravity;
        } else if (command == "icon" && tokens >> value) {
            character.icon = resolveFrame(file,root,value);
        } else if (!animation && command == "diesound" && tokens >> std::quoted(value)) {
            character.deathSound = resolveSound(value);
        } else if (!animation && command == "atchain") {
            character.extraHeader += line+'\n';
        } else if (command == "meshmodel") {
            require(!mesh && bool(tokens >> std::quoted(value) >> character.mesh.scale),"Invalid meshmodel command.");
            character.mesh.file = resolveFrame(file,root,value);
            character.mesh.planar = false;
            if (tokens >> character.mesh.yaw >> value) character.mesh.planar = lower(value) == "planar";
            mesh = std::make_unique<GlbModel>(character.mesh.file,images);
        } else if (command == "anim") {
            if (!(tokens >> value)) throw std::runtime_error("Animation is missing its name.");
            animation = &character.animations.emplace_back();
            nextItemScript = false;
            animation->name = animationId(value);
            animation->loop = false;
            properties = Frame{};
            explicitOrigin = false;
        } else if (animation && command == "meshclip") {
            require(mesh && bool(tokens >> std::quoted(value)),"meshclip needs a GLB and a clip name.");
            const auto& clips = mesh->clips();
            const auto clip = std::find_if(clips.begin(),clips.end(),[&](const MeshClip& c) { return c.name == value; });
            require(clip != clips.end(),"GLB clip was not found: "+value);
            animation->meshClip = int(clip-clips.begin());
        } else if (animation && command == "frame") {
            if (!(tokens >> std::quoted(value))) throw std::runtime_error("Frame command is missing its image.");
            const auto path = resolveFrame(file,root,value);
            auto frame = makeFrame(path,images);
            frame.ticks = properties.ticks;
            if (explicitOrigin) {
                frame.originX = properties.originX;
                frame.originY = properties.originY;
            }
            frame.body = properties.body;
            frame.attack = properties.attack;
            frame.effects = properties.effects;
            frame.sound = std::move(properties.sound);
            properties.sound.clear();
            properties.effects.mask.clear();
            animation->frames.push_back(std::move(frame));
        } else if (animation && command == "sound" && tokens >> std::quoted(value)) {
            properties.sound = resolveSound(value);
        } else if (animation && command == "alphamask" && tokens >> value) {
            properties.effects.mask = resolveFrame(file,root,value);
        } else if (animation && command == "nodrawmethod") {
            properties.effects.enabled = false;
        } else if (animation && command == "drawmethod") {
            auto next = properties.effects;
            int flipX = 0, flipY = 0, shift = 0, remap = 0, flipRotate = 0;
            std::string fill;
            if (tokens >> next.scaleX >> next.scaleY >> flipX >> flipY >> shift
                       >> next.blend >> remap >> fill >> next.rotation >> flipRotate &&
                       !shift && !remap && (fill == "0" || fill == "000000") && !flipRotate) {
                next.enabled = true;
                next.flipX = flipX != 0;
                next.flipY = flipY != 0;
                properties.effects = next;
            } else ++skipped;
        } else if (animation && command == "delay") {
            if (tokens >> properties.ticks) properties.ticks = std::clamp(properties.ticks,1,1000);
            else ++skipped;
        } else if (animation && command == "offset") {
            explicitOrigin = bool(tokens >> properties.originX >> properties.originY);
        } else if (animation && command == "loop") {
            int loop = 0;
            tokens >> loop;
            animation->loop = loop != 0;
        } else if (animation && (command == "bbox" || command == "attack" || command == "attack1")) {
            auto& box = command == "bbox" ? properties.body : properties.attack;
            int x, y, width, height;
            box.enabled = false;
            if (tokens >> x >> y >> width >> height) {
                box.x = x; box.y = y; box.width = width; box.height = height;
                box.enabled = width > 0 && height > 0;
                if (command == "bbox") tokens >> box.depth;
                else tokens >> box.damage;
            }
        } else if (animation && command == "custknife") {
            tokens >> value;
            animation->subEntity.model = modelToken(value);
        } else if (animation && command == "throwframe") {
            auto& cue = animation->subEntity;
            cue.enabled = bool(tokens >> cue.frame);
            int altitude = 0;
            if (tokens >> altitude) cue.altitude = altitude == -1 ? 0 : altitude;
        } else if (animation && command == "projectile_position_x") tokens >> animation->subEntity.x;
        else if (animation && command == "projectile_position_y") tokens >> animation->subEntity.altitude;
        else if (animation && command == "projectile_position_z") tokens >> animation->subEntity.z;
        else if (animation && (command == "subentity" || command == "custentity")) {
            tokens >> value;
            animation->item.model = modelToken(value);
        } else if (animation && command == "spawnframe") {
            auto& cue = animation->item;
            int placement = 0;
            cue.enabled = bool(tokens >> cue.frame >> cue.x >> cue.z >> cue.altitude);
            if (tokens >> placement && placement != 0) {
                cue.enabled = false;
                ++skipped;
            }
        } else {
            ++skipped;
        }
    }
    for (auto& clip : character.animations) {
        if (!clip.subEntity.enabled && clip.item.enabled) {
            clip.subEntity = std::move(clip.item);
            clip.item = {};
        }
    }
    require(hasFrames(character), "No readable animation frames were found in that model.");
    return character;
}

std::string modelText(const Character& model, const GlbModel* mesh, std::string assetRoot) {
    std::ostringstream out;
    out << "name " << model.id << "\ntype " << model.type << "\n";
    if (model.type == "player" || model.type == "enemy" || model.type == "npc" || model.type == "obstacle") {
        out << "health " << model.health << "\nspeed " << model.speed << '\n';
    }
    if (model.type == "item") out << "health " << model.health << '\n';
    const auto& entity = model.entity;
    if (entity.mp >= 0) out << "mp " << entity.mp << '\n';
    if (entity.aggression >= 0) out << "aggression " << entity.aggression << '\n';
    if (entity.score >= 0) out << "score " << entity.score << '\n';
    if (entity.jumpHeight >= 0) out << "jumpheight " << entity.jumpHeight << '\n';
    if (entity.jumpSpeed >= 0) out << "jumpspeedf " << entity.jumpSpeed << '\n';
    if (entity.lifespan >= 0) out << "lifespan " << entity.lifespan << '\n';
    if (entity.wall >= 0) out << "subject_to_wall " << entity.wall << '\n';
    if (entity.gravity >= 0) out << "subject_to_gravity " << entity.gravity << '\n';
    if (assetRoot.empty()) assetRoot = "data/chars/" + model.id + '/';
    const auto sounds = soundPaths(model,assetRoot);
    if (!model.deathSound.empty()) out << "diesound " << sounds.at(model.deathSound) << '\n';
    if (!model.mesh.file.empty()) {
        require(mesh != nullptr,"Could not load the GLB for " + model.id + ".");
        std::ostringstream transform;
        transform.imbue(std::locale::classic());
        transform << std::fixed << std::setprecision(8) << model.mesh.scale << ' ' << model.mesh.yaw;
        out << "meshmodel " << assetRoot << "model.glb " << transform.str()
            << (model.mesh.planar ? " planar" : "") << '\n';
    }
    if (!model.icon.empty()) out << "icon " << assetRoot << "icon.png 0\n";
    for (size_t i=1; i<model.colorSets.size(); ++i)
        out << "alternatepal " << assetRoot << counterName("palette_",i,".act") << '\n';
    if (!model.extraHeader.empty()) out << model.extraHeader << '\n';
    if (entity.runningJumpHeight >= 0) out << "run.jump.y " << entity.runningJumpHeight << '\n';
    if (entity.runningJumpDistance >= 0) out << "run.jump.x " << entity.runningJumpDistance << '\n';
    bool previousEffect = false;
    for (size_t a = 0; a < model.animations.size(); ++a) {
        const auto& animation = model.animations[a];
        if (animation.frames.empty()) continue;
        out << "\nanim " << animationId(animation.name) << "\n    loop " << int(animation.loop) << '\n';
        if (animation.meshClip >= 0) {
            require(mesh && size_t(animation.meshClip) < mesh->clips().size(),"Choose an existing GLB clip for "+animation.name+".");
            out << "    meshclip " << mesh->clips()[size_t(animation.meshClip)].exportName << '\n';
        }
        writeSpawnCues(out,animation);
        if (!animation.extraCommands.empty()) out << animation.extraCommands << '\n';
        for (size_t f = 0; f < animation.frames.size(); ++f) {
            const auto& frame = animation.frames[f];
            const auto& body = frame.body;
            const auto& attack = frame.attack;
            out << "    delay " << frame.ticks << "\n    offset " << frame.originX << ' ' << frame.originY;
            if (body.enabled) {
                out << "\n    bbox " << body.x << ' ' << body.y << ' ' << body.width << ' ' << body.height << ' ' << body.depth;
            } else out << "\n    bbox 0 0 0 0";
            if (attack.enabled) {
                out << "\n    attack " << attack.x << ' ' << attack.y << ' ' << attack.width << ' ' << attack.height
                    << ' ' << attack.damage << " 0 0 0 0 " << attack.depth;
            } else out << "\n    attack 0";
            const auto& effect = frame.effects;
            const bool mask = effect.embeddedAlpha || !effect.mask.empty();
            if (effect.enabled) {
                out << "\n    drawmethod " << effect.scaleX << ' ' << effect.scaleY << ' '
                    << int(effect.flipX) << ' ' << int(effect.flipY) << " 0 "
                    << (mask ? 0 : effect.blend) << " 0 0 " << effect.rotation << " 0";
            } else if (previousEffect) out << "\n    nodrawmethod";
            previousEffect = effect.enabled;
            if (mask) out << "\n    alphamask " << assetRoot << maskName(a,f);
            if (!frame.sound.empty()) out << "\n    sound " << sounds.at(frame.sound);
            if (!frame.extraCommands.empty()) out << '\n' << frame.extraCommands;
            out << "\n    frame " << assetRoot << (frame.image.empty() ? "mesh_timing.png" : imageName(a,f)) << '\n';
        }
    }
    return out.str();
}

StageArtLayout stageArtLayout(const Stage& stage, std::span<const ImageSize> sizes) {
    StageArtLayout layout;
    if (sizes.empty()) return layout;
    int stitchedX = 0, right = 0, bottom = 0;
    for (size_t i = 0; i < sizes.size(); ++i) {
        const auto size = sizes[i];
        const auto offset = stage.panels[i].offset;
        const int x = stitchedX + std::clamp(offset.x,-stageArtworkOffsetLimit,stageArtworkOffsetLimit);
        const int y = std::clamp(offset.y,-stageArtworkOffsetLimit,stageArtworkOffsetLimit);
        layout.positions.push_back({x,y});
        stitchedX += size.width;
        right = std::max(right,x+size.width);
        bottom = std::max(bottom,y+size.height);
        layout.composite |= offset.x != 0 || offset.y != 0 ||
                            size.width != sizes.front().width || size.height != sizes.front().height;
    }
    if (!layout.composite) {
        layout.panelWidth = sizes.front().width;
        layout.panelHeight = sizes.front().height;
        layout.panelCount = int(sizes.size());
        return layout;
    }

    layout.panelHeight = std::max(240,bottom);
    const int maxWidth = std::clamp(40'000'000/layout.panelHeight,1,16384);
    const int width = std::max(1,right);
    layout.panelCount = std::clamp((width+maxWidth-1)/maxWidth,1,26);
    layout.panelWidth = (width+layout.panelCount-1)/layout.panelCount;
    return layout;
}

namespace {

std::vector<ImageSize> panelSizes(const Stage& stage, const Images& images) {
    std::vector<ImageSize> sizes;
    sizes.reserve(stage.panels.size());
    for (const auto& panel : stage.panels) sizes.push_back(images.dimensions(panel.image));
    return sizes;
}

std::string stageTerrainText(const std::vector<StageTerrain>& terrain) {
    std::ostringstream out;
    for (const auto& shape : terrain) {
        out << (shape.pit ? "hole " : "wall ") << shape.x << ' ' << shape.z << " 0 0 "
            << shape.width << ' ' << shape.width << ' ' << shape.depth << ' '
            << shape.altitude << ' ' << shape.type << '\n';
    }
    return out.str();
}

std::string stageDepthText(const Stage& stage) {
    const int backgroundDepth = stage.backgroundDepth > 0 ? stage.backgroundDepth : stage.laneBottom;
    return "z "+std::to_string(stage.laneTop)+' '+std::to_string(stage.laneBottom)+' '+std::to_string(backgroundDepth)+'\n';
}

std::string stagePlayerStartText(const Stage& stage) {
    return "spawn1 "+std::to_string(stage.playerX)+' '+std::to_string(stage.playerZ-stage.laneTop)+" 0\n";
}

// The generated section is identified so an exported data folder can be reopened and edited.
constexpr std::string_view cameraZoomMarker = "# studio_camera_zoom ";
constexpr std::string_view cameraHookMarker = "# studio_camera_previous ";

std::string applyStageCamera(const std::string& source, const StageCamera& camera,
                             bool updateZoom, bool updateOffset, bool updateFollow,
                             const std::string& scriptPath, std::string* scriptBody) {
    std::istringstream input(source), tokens;
    std::ostringstream remaining, header;
    bool inlineScript = false, generated = false;
    std::string previousSource;
    const auto keepHook = updateZoom && camera.zoom != 100 && !camera.nativeZoom;
    for (std::string line; std::getline(input,line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (updateZoom && !inlineScript && line.starts_with(cameraZoomMarker)) {
            generated = true;
            continue;
        }
        if (generated) {
            if (line.starts_with(cameraHookMarker)) {
                previousSource = line.substr(cameraHookMarker.size());
                if (!keepHook) remaining << previousSource << '\n';
            }
            if (line == "# studio_camera_end") generated = false;
            continue;
        }
        if (!inlineScript) {
            const auto command = nativeCommand(line,tokens);
            if ((updateOffset && command == "cameraoffset") || (updateFollow && command == "cameratype") ||
                (updateZoom && command == "camerazoom")) continue;
            const bool startsScript = startsInlineScript(tokens);
            if (keepHook && command == "updatedscript") {
                require(!startsScript,"Choose OpenBOR3D camera mode or move the inline updatedscript to a file before enabling 2D camera zoom.");
                std::string script;
                if (tokens >> std::quoted(script)) {
                    previousSource = line;
                    continue;
                }
            }
            inlineScript = startsScript;
        }
        if (inlineScript && line.find("@end_script") != std::string::npos) inlineScript = false;
        remaining << line << '\n';
    }
    if (updateOffset && (camera.offsetX || camera.offsetY))
        header << "cameraoffset " << camera.offsetX << ' ' << camera.offsetY << '\n';
    if (updateFollow && camera.followAltitude) header << "cameratype 1\n";
    if (!updateZoom || camera.zoom == 100) return header.str()+remaining.str();
    if (camera.nativeZoom)
        return header.str()+"camerazoom "+std::to_string(camera.zoom)+'\n'+remaining.str();

    std::string previous;
    nativeCommand(previousSource,tokens);
    tokens >> std::quoted(previous);
    header << cameraZoomMarker << camera.zoom << '\n';
    if (!previous.empty()) header << cameraHookMarker << previousSource << '\n';
    header << "updatedscript " << scriptPath << "\n# studio_camera_end\n";
    if (!scriptBody) return header.str()+remaining.str();
    std::ostringstream script;
    script << R"(
void main()
{
)";
    if (!previous.empty()) {
        script << R"(    void previous = getlocalvar("studio_camera_previous");
    if (!previous) {
        previous = allocscript("Studio camera previous updatedscript");
        loadscript(previous,)" << std::quoted(previous) << R"();
        compilescript(previous);
        setlocalvar("studio_camera_previous",previous);
    }
    executescript(previous);
)";
    }
    script << "    int zoom = " << camera.zoom << ";\n" << R"(
    if (!openborvariant("in_level") || openborvariant("game_paused")) return;
    int width = openborvariant("hResolution");
    int height = openborvariant("vResolution");
    int worldWidth = ((width * 100 / zoom + 3) / 4) * 4;
    int worldHeight = height * 100 / zoom;
    void world = getlocalvar("studio_camera_world");
    if (world) {
        if (getgfxproperty(world,"width") != worldWidth || getgfxproperty(world,"height") != worldHeight) {
            free(world);
            world = NULL();
        }
    }
    if (!world) {
        world = allocscreen(worldWidth,worldHeight);
        setlocalvar("studio_camera_world",world);
        if (!world) return;
    }
    clearscreen(world);
    drawspriteq(world,0,openborconstant("MIN_INT"),9997,
                (worldWidth-width)/2,worldHeight-height);
    changedrawmethod(NULL(),"reset",1);
    changedrawmethod(NULL(),"enabled",1);
    changedrawmethod(NULL(),"scalex",(width*256+worldWidth-1)/worldWidth);
    changedrawmethod(NULL(),"scaley",(height*256+worldHeight-1)/worldHeight);
    drawscreen(world,0,0,9999);
    changedrawmethod(NULL(),"reset",1);
}

void ondestroy()
{
    void world = getlocalvar("studio_camera_world");
    if (world) free(world);
    setlocalvar("studio_camera_world",NULL());
)";
    if (!previous.empty()) {
        script << R"(    void previous = getlocalvar("studio_camera_previous");
    if (previous) free(previous);
    setlocalvar("studio_camera_previous",NULL());
)";
    }
    script << "}\n";
    *scriptBody = script.str();
    return header.str()+remaining.str();
}

std::string stageTextWithLayout(const Stage& stage, size_t index, const StageArtLayout& layout,
                                std::string art = {}, std::string music = {},
                                std::string cameraScript = {}, std::string* scriptBody = nullptr) {
    if (art.empty()) art = artPath(index);
    std::ostringstream out;
    out << "# " << modelToken(stage.title) << "\ndirection right\n";
    if (!stage.music.empty()) {
        if (music.empty()) music = stageMusicPath(stage,index);
        out << "music " << music << ' ' << stage.musicOffset << '\n';
    }
    if (!stage.background.empty()) {
        out << "background " << art << "background.png";
        if (stage.backgroundOffset.x || stage.backgroundOffset.y || !stage.repeatBackgroundX || !stage.repeatBackgroundY)
            out << " 0.5 0.5 " << stage.backgroundOffset.x << ' ' << stage.backgroundOffset.y
                << " 0 0 " << (stage.repeatBackgroundX ? -1 : 1) << ' ' << (stage.repeatBackgroundY ? -1 : 1);
        out << '\n';
    }
    for (int panel = 0; panel < layout.panelCount; ++panel)
        out << "panel " << art << counterName("panel_",size_t(panel+1),".png") << " none none\n";
    if (layout.panelCount) {
        out << "order ";
        for (int panel = 0; panel < layout.panelCount; ++panel) out << char('a'+panel);
        out << '\n';
    }
    out << stagePlayerStartText(stage);
    out << stageTerrainText(stage.terrain);
    if (!stage.extraHeader.empty()) out << stage.extraHeader << '\n';
    struct Event { int at; const StageSpawn* spawn; };
    std::vector<Event> events;
    for (const auto& spawn : stage.spawns) events.push_back({spawn.at, &spawn});
    for (const auto& wave : stage.waves) if (wave.wait) events.push_back({wave.at,nullptr});
    std::stable_sort(events.begin(),events.end(),[](const Event& a,const Event& b){return a.at < b.at;});
    for (const auto& event : events) {
        out << '\n';
        if (event.spawn) {
            const auto& spawn = *event.spawn;
            out << "spawn " << spawn.model << "\n    coords " << spawn.x << ' ' << spawn.z << ' ' << spawn.altitude << '\n';
            if (spawn.flip) out << "    flip 1\n";
            if (spawn.boss) out << "    boss 1\n";
            if (!spawn.item.empty()) out << "    item " << spawn.item << '\n';
            if (spawn.health > 0) out << "    health " << spawn.health << '\n';
            if (spawn.mp > 0) out << "    mp " << spawn.mp << '\n';
            if (spawn.palette >= 0) out << "    map " << spawn.palette << '\n';
        } else out << "wait\n";
        out << "at " << event.at << '\n';
    }
    const auto& camera = stage.camera;
    if (camera != StageCamera{})
        return applyStageCamera(out.str(),camera,camera.zoom != 100,camera.offsetX || camera.offsetY,camera.followAltitude,
            cameraScript.empty() ? "data/scripts/"+counterName("studio_camera_",index+1,".c") : cameraScript,scriptBody);
    return out.str();
}

void exportStagePanels(const Stage& stage, const StageArtLayout& layout,
                       std::span<const ImageSize> sizes, const fs::path& folder, Images& images) {
    require(!layout.composite || uint64_t(layout.panelWidth)*layout.panelHeight <= 40'000'000,
            "The placed panel artwork is too large to export as OpenBOR panels.");
    for (int panel = 0; panel < layout.panelCount; ++panel) {
        Pixels result;
        if (!layout.composite) {
            result = images.read(stage.panels[size_t(panel)].image);
        } else {
            result = {layout.panelWidth,layout.panelHeight,
                      std::vector<uint8_t>(size_t(layout.panelWidth)*layout.panelHeight*4)};
            const int stripX = panel*layout.panelWidth;
            for (size_t i = 0; i < stage.panels.size(); ++i) {
                const auto [x,y] = layout.positions[i];
                const auto size = sizes[i];
                const int left = std::max(0,x-stripX), right = std::min(result.width,x+size.width-stripX);
                const int top = std::max(0,y), bottom = std::min(result.height,y+size.height);
                if (left >= right || top >= bottom) continue;
                const auto source = images.read(stage.panels[i].image);
                require(source.width == size.width && source.height == size.height,
                        "A panel image changed size during export. Try exporting again.");
                for (int row = top; row < bottom; ++row) {
                    auto* target = &result.rgba[(size_t(row)*result.width+left)*4];
                    const auto* pixel = &source.rgba[(size_t(row-y)*source.width+left+stripX-x)*4];
                    for (int column = left; column < right; ++column, target+=4, pixel+=4) {
                        if (pixel[3] >= 128) std::copy_n(pixel,4,target);
                    }
                }
            }
        }
        Histogram histogram;
        countColors(histogram,result);
        images.writeIndexed(folder/counterName("panel_",size_t(panel+1),".png"),result,makePalette(histogram));
    }
}

}

std::string stageText(const Stage& stage, size_t index, const Images& images) {
    return stageTextWithLayout(stage,index,stageArtLayout(stage,panelSizes(stage,images)));
}

void verifyProject(const Project& project) {
    const bool native = !project.nativeFiles.empty();
    require(native || !project.characters.empty() || !project.stages.empty(), "Add a model or a stage before exporting.");
    require(project.stages.size() <= 100, "Export supports up to 100 stages per level set.");
    require(project.module.maxPlayers >= 1 && project.module.maxPlayers <= 4,
            "The module must support between one and four players.");
    if (project.module.hud.enabled) {
        const auto& hud = project.module.hud;
        for (const auto* gauge : {&hud.life,&hud.mp,&hud.enemy})
            require(gauge->width > 0 && gauge->height > 0, "HUD gauges need positive dimensions.");
        require(hud.timerWidth > 0 && hud.timerHeight > 0 && hud.timerFont >= 0 && hud.timerFont <= 3,
                "The HUD timer needs positive dimensions and a font from 0 to 3.");
    }
    std::set<std::string> ids;
    for (const auto& model : project.characters) {
        require(validToken(model.id), "Invalid model ID: " + model.id);
        require(ids.insert(model.id).second, "Duplicate model ID: " + model.id);
        if (native && !model.nativeFile.empty()) continue;
        require(knownType(model.type), "Unsupported model type: " + model.type);
        require(hasFrames(model), "Add frames to " + model.id + " before exporting.");
        require(std::isfinite(model.speed) && model.speed >= 0, "Model speed must be nonnegative and finite.");
        if (!model.mesh.file.empty()) {
            const auto& mesh = model.mesh;
            require(validMeshAxes(mesh.up,mesh.forward) && std::isfinite(mesh.scale) && mesh.scale >= .000001f &&
                    mesh.scale <= 1'000'000 && std::isfinite(mesh.pitch) && std::isfinite(mesh.yaw) &&
                    std::isfinite(mesh.roll) && std::abs(mesh.pitch) <= 360 && std::abs(mesh.yaw) <= 360 &&
                    std::abs(mesh.roll) <= 360,"Invalid GLB size or orientation for " + model.id + ".");
        }
        if (!model.colorSets.empty()) {
            require(model.sourcePalette.size() >= 2 && model.sourcePalette.size() <= 256,
                    "Invalid source palette for " + model.id + ".");
            require(model.colorSets.size() <= 32, "Too many palettes for " + model.id + ".");
            for (const auto& set : model.colorSets)
                require(set.colors.size() == model.sourcePalette.size(),
                        "Palette size does not match the source palette for " + model.id + ".");
        }
        std::set<std::string> names;
        for (const auto& animation : model.animations) {
            if (animation.frames.empty()) continue;
            const auto id = animationId(animation.name);
            require(validToken(id), "Invalid animation ID \"" + animation.name + "\" in " + model.id +
                    ". Enter a single OpenBOR ID such as charge or attack1.");
            require(names.insert(id).second, "Animation \"" + animation.name + "\" in " + model.id +
                    " duplicates the OpenBOR ID \"" + id + "\".");
            require(animation.meshClip == -1 || (animation.meshClip >= 0 && !model.mesh.file.empty()),
                    "Choose a GLB for the clip mapped to "+animation.name+" in "+model.id+".");
            validateCue(animation.subEntity, animation, project);
            validateCue(animation.item, animation, project);
            for (const auto& frame : animation.frames) {
                require(frame.ticks > 0,"An animation frame in "+model.id+" needs a positive delay.");
                require(!frame.image.empty() || animation.meshClip >= 0,
                        "Map a GLB clip or add a sprite image for the timing frames in "+model.id+" / "+animation.name+".");
                require(!frame.image.empty() || (!frame.effects.embeddedAlpha && frame.effects.mask.empty()),
                        "A GLB timing frame has no sprite image for an alpha mask.");
                const auto& effect = frame.effects;
                require(!effect.enabled || (effect.scaleX > 0 && effect.scaleX <= 4096 &&
                        effect.scaleY > 0 && effect.scaleY <= 4096 && effect.blend >= 0 && effect.blend <= 6),
                        "Invalid draw effect in " + model.id + ".");
            }
        }
        require(names.contains("idle"), "Add an idle animation with frames to " + model.id + ".");
    }
    for (const auto& stage : project.stages) {
        require(stage.camera.zoom >= cameraZoomMin && stage.camera.zoom <= cameraZoomMax,
                "Choose a camera zoom between 50% and 200% for "+stage.title+".");
        for (const auto& shape : stage.terrain) {
            const auto inRange = [](int value) { return value >= -stageTerrainLimit && value <= stageTerrainLimit; };
            require(inRange(shape.x) && inRange(shape.z) && inRange(shape.type) &&
                    shape.width > 0 && shape.width <= stageTerrainLimit &&
                    shape.depth > 0 && shape.depth <= stageTerrainLimit &&
                    shape.altitude >= (shape.pit ? 0 : 1) && shape.altitude <= stageTerrainLimit,
                    "Invalid platform or pit geometry in "+stage.title+".");
        }
        if (native && !stage.nativeFile.empty()) continue;
        require(!stage.panels.empty(), "Add a panel to " + stage.title + " before exporting.");
        require(stage.panels.size() <= 26, "A stage can have at most 26 panel slots.");
        const auto validOffset = [](ScreenPoint offset) {
            return offset.x >= -stageArtworkOffsetLimit && offset.x <= stageArtworkOffsetLimit &&
                   offset.y >= -stageArtworkOffsetLimit && offset.y <= stageArtworkOffsetLimit;
        };
        require(validOffset(stage.backgroundOffset) &&
                std::all_of(stage.panels.begin(),stage.panels.end(),[&](const StagePanel& panel) {
                    return !panel.image.empty() && validOffset(panel.offset);
                }), "Invalid artwork placement in "+stage.title+".");
        require(stage.laneTop >= 0 && stage.laneBottom >= stage.laneTop && stage.laneBottom < 240,
                "Set a valid 0–239 walkable depth range for " + stage.title + ".");
        require(stage.playerZ >= stage.laneTop && stage.playerZ <= stage.laneBottom,
                "Place the player start inside the walkable lane in " + stage.title + ".");
        require(stage.spawns.size() + stage.waves.size() <= 600, "Stage spawn and wait events exceed the engine limit.");
        for (const auto& spawn : stage.spawns) {
            const auto* model = findCharacter(project,spawn.model);
            require(model != nullptr, "Choose a model for each spawn in " + stage.title + ".");
            require(!model->nativeFile.empty() || spawn.palette < 1 || size_t(spawn.palette) < model->colorSets.size(),
                    "The spawn palette for " + spawn.model + " does not exist.");
            require(spawn.at >= 0, "Stage spawn triggers cannot be negative.");
            require(spawn.item.empty() || findCharacter(project,spawn.item),
                    "Choose an existing carried item model in " + stage.title + ".");
        }
        for (const auto& wave : stage.waves) require(wave.at >= 0, "Stage waits cannot be negative.");
    }
}

std::string modelsConfigurationText(const Project& project) {
    std::ostringstream models;
    models << moduleModelsText(project.module);
    int specials = 8;
    std::set<std::string> preload;
    for (const auto& model : project.characters) {
        for (const auto& animation : model.animations) {
            specials = std::max(specials,freeSpecial(animationId(animation.name)));
            if (animation.frames.empty()) continue;
            if (animation.subEntity.enabled) preload.insert(animation.subEntity.model);
            if (animation.item.enabled) preload.insert(animation.item.model);
        }
    }
    models << "maxfreespecials " << specials << '\n';
    for (const auto& model : project.characters)
        models << (model.type == "player" || preload.contains(model.id) ? "load " : "know ")
               << model.id << ' ' << modelPath(model) << '\n';
    return models.str();
}

std::string levelsConfigurationText(const Project& project) {
    std::ostringstream levels;
    levels << moduleLevelsText(project.module);
    if (!project.stages.empty()) {
        levels << "set Adventure\nmaxplayers " << project.module.maxPlayers << '\n';
        if (!project.module.levelSetCommands.empty()) levels << project.module.levelSetCommands << '\n';
    }
    for (size_t i = 0; i < project.stages.size(); ++i) {
        const auto& stage = project.stages[i];
        levels << stageDepthText(stage);
        if (!stage.before.file.empty()) levels << "scene data/scenes/" << stageSceneName(i,false) << ".txt\n";
        levels << "file " << levelPath(i) << "\n";
        if (!stage.after.file.empty()) levels << "scene data/scenes/" << stageSceneName(i,true) << ".txt\n";
        if (i+1 < project.stages.size()) levels << "next\n";
    }
    return levels.str();
}

namespace {

std::map<std::string,json> nativeValues(const Project& project) {
    auto comparable = project;
    std::map<std::string,fs::path> relative;
    for (const auto& [name,source] : project.nativeFiles)
        relative.emplace(nativeReference(utf8(source.lexically_normal())),pathFromUtf8(name));
    comparable.nativeFiles.clear();
    visitAssets(comparable,[&](fs::path& file) {
        if (const auto it = relative.find(nativeReference(utf8(file.lexically_normal()))); it != relative.end())
            file = it->second;
    });
    std::map<std::string,json> values;
    for (const auto& model : comparable.characters)
        if (!model.nativeFile.empty()) values.emplace(model.nativeFile,model);
    for (const auto& stage : comparable.stages)
        if (!stage.nativeFile.empty()) values.emplace(stage.nativeFile,stage);
    values.emplace("data/studio-module",comparable.module);
    return values;
}

void eraseFields(json& value, std::initializer_list<const char*> keys) {
    for (const auto key : keys) value.erase(key);
}

json modelWithoutSoundJumpOrSpawns(json value) {
    eraseFields(value,{"deathSound","title"});
    eraseFields(value["entity"],{"jumpHeight","jumpSpeed","runningJumpHeight","runningJumpDistance"});
    for (auto& clip : value["animations"]) {
        eraseFields(clip,{"subEntity","item"});
        for (auto& frame : clip["frames"]) frame.erase("sound");
    }
    return value;
}

bool spawnCuesChanged(const json& current, const json& original) {
    return current.at("subEntity") != original.at("subEntity") || current.at("item") != original.at("item");
}

std::string patchModelSpawns(const Project& project, const Character& model,
                             const json& current, const json& original, const std::string& source) {
    const auto& clips = current.at("animations");
    const auto& baseline = original.at("animations");
    std::istringstream input(source), tokens;
    std::ostringstream out;
    std::string line;
    size_t clip = size_t(-1);
    bool changed = false, script = false, generated = false;
    while (std::getline(input,line)) {
        const auto command = nativeCommand(line,tokens);
        if (script || command == "@script" ||
            (command == "animationscript" && line.find("@script") != line.npos)) {
            script = line.find("@end_script") == line.npos;
            if (!generated) out << line << '\n';
            if (!script) generated = false;
            continue;
        }
        if (command == "anim") {
            ++clip;
            changed = clip < clips.size() && spawnCuesChanged(clips[clip],baseline[clip]);
            out << line << '\n';
            if (changed) {
                const auto& animation = model.animations[clip];
                validateCue(animation.subEntity,animation,project);
                validateCue(animation.item,animation,project);
                writeSpawnCues(out,animation);
            }
            continue;
        }
        if (changed) {
            SpawnCue item;
            if (readItemCue(line,item)) { generated = true; continue; }
            if (command == "subentity" || command == "custentity" || command == "spawnframe" ||
                legacySpawnCommand(command)) continue;
        }
        out << line << '\n';
    }
    return out.str();
}

json stageWithoutMediaOrTerrain(json value) {
    eraseFields(value,{"title","music","musicOffset","before","after","terrain","camera","backgroundDepth",
                       "laneTop","laneBottom","playerZ"});
    return value;
}

std::string patchModelSound(const Character& model, const json& current, const json& baseline,
                            const std::string& source, const fs::path& root, const std::string& assetRoot) {
    std::map<fs::path,std::string> paths;
    const auto add = [&](const fs::path& file) {
        const auto found = paths.find(file);
        if (found != paths.end()) return found->second;
        validateAudio(file);
        auto path = assetRoot+counterName("studio_sound_",paths.size()+1,mediaExtension(file));
        copyMedia(file,root/pathFromUtf8(path));
        paths.emplace(file,path);
        return path;
    };
    const bool deathChanged = current.at("deathSound") != baseline.at("deathSound");
    std::istringstream input(source), tokens;
    std::string line;
    std::ostringstream out, pending;
    std::vector<std::pair<std::string,bool>> pendingLines;
    size_t clip = size_t(-1), frame = 0;
    bool script = false;
    if (deathChanged && !model.deathSound.empty()) {
        // Keep the model's name and initial header first.
        pending << "diesound " << add(model.deathSound) << '\n';
    }
    while (std::getline(input,line)) {
        auto command = nativeCommand(line,tokens);
        if (command == "@script") script = true;
        if (script) {
            if (line.find("@end_script") != std::string::npos) script = false;
            command.clear();
        }
        if (command == "anim") {
            for (const auto& [previous,isSound] : pendingLines) out << previous << '\n';
            pendingLines.clear();
            if (++clip == 0) out << pending.str();
            frame = 0;
        }
        if (clip >= model.animations.size()) {
            if (command != "diesound" || !deathChanged) out << line << '\n';
            continue;
        }
        if (command != "frame") {
            pendingLines.emplace_back(line,command == "sound");
            continue;
        }
        const auto& frames = model.animations[clip].frames;
        const bool changed = frame < frames.size() &&
            current["animations"][clip]["frames"][frame]["sound"] != baseline["animations"][clip]["frames"][frame]["sound"];
        for (const auto& [previous,isSound] : pendingLines) {
            if (!changed || !isSound) out << previous << '\n';
        }
        pendingLines.clear();
        if (changed && !frames[frame].sound.empty()) out << "    sound " << add(frames[frame].sound) << '\n';
        out << line << '\n';
        ++frame;
    }
    for (const auto& [previous,isSound] : pendingLines) out << previous << '\n';
    return out.str();
}

bool changedJumpField(const json& now, const json& original, const char* field) {
    return now.at("entity").value(field,-1.f) != original.at("entity").value(field,-1.f);
}

std::string patchModelJump(const Character& model, const json& current, const json& baseline,
                           const std::string& source) {
    const bool heightChanged = changedJumpField(current,baseline,"jumpHeight");
    const bool speedChanged = changedJumpField(current,baseline,"jumpSpeed");
    const bool runHeightChanged = changedJumpField(current,baseline,"runningJumpHeight");
    const bool runDistanceChanged = changedJumpField(current,baseline,"runningJumpDistance");
    if (!heightChanged && !speedChanged && !runHeightChanged && !runDistanceChanged) return source;
    std::istringstream input(source), tokens;
    std::ostringstream out;
    std::string line;
    bool header = true;
    while (std::getline(input,line)) {
        if (header) {
            const auto command = nativeCommand(line,tokens);
            if (heightChanged && (command == "jumpheight" || command == "jump_height")) continue;
            if (speedChanged && (command == "jumpspeed" || command == "jumpspeedf" || command == "jump_speed")) continue;
            if (runHeightChanged && command == "run.jump.y") continue;
            if (runDistanceChanged && command == "run.jump.x") continue;
            if (command == "anim") {
                if (heightChanged && model.entity.jumpHeight >= 0) out << "jumpheight " << model.entity.jumpHeight << '\n';
                if (speedChanged && model.entity.jumpSpeed >= 0) out << "jumpspeedf " << model.entity.jumpSpeed << '\n';
                if (runHeightChanged && model.entity.runningJumpHeight >= 0) out << "run.jump.y " << model.entity.runningJumpHeight << '\n';
                if (runDistanceChanged && model.entity.runningJumpDistance >= 0) out << "run.jump.x " << model.entity.runningJumpDistance << '\n';
                header = false;
            }
        }
        out << line << '\n';
    }
    return out.str();
}

std::string patchStageText(const std::string& source, const Stage& stage, const std::string& music,
                           bool musicChanged, bool playerDepthChanged, const std::vector<StageTerrain>* originalTerrain) {
    const auto newline = source.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::istringstream input(source), tokens;
    std::ostringstream out;
    if (musicChanged && !music.empty()) out << "music " << music << ' ' << stage.musicOffset << newline;
    std::string line;
    size_t terrainIndex = 0;
    const auto writeTerrain = [&] {
        std::istringstream terrain(stageTerrainText(stage.terrain));
        for (std::string entry; std::getline(terrain,entry);) out << entry << newline;
    };
    bool placed = false, script = false;
    while (std::getline(input,line)) {
        auto command = nativeCommand(line,tokens);
        if (startsInlineScript(tokens)) script = true;
        if (script) {
            if (line.find("@end_script") != std::string::npos) script = false;
            command.clear();
        }
        if (musicChanged && command == "music") continue;
        if (playerDepthChanged && command == "spawn1") {
            std::string x, altitude;
            int oldDepth = 0;
            if (tokens >> x) {
                tokens >> oldDepth >> altitude;
                out << "spawn1 " << x << ' ' << stage.playerZ-stage.laneTop << ' '
                    << (altitude.empty() ? "0" : altitude) << newline;
                playerDepthChanged = false;
                continue;
            }
        }
        if (originalTerrain && terrainIndex < originalTerrain->size()) {
            if (const auto shape = readStageTerrain(command,tokens); shape && *shape == (*originalTerrain)[terrainIndex]) {
                ++terrainIndex;
                if (!placed) { writeTerrain(); placed = true; }
                continue;
            }
        }
        out << line << '\n';
    }
    if (playerDepthChanged) out << stagePlayerStartText(stage);
    if (originalTerrain && !placed) writeTerrain();
    return out.str();
}

std::string nativeExportRoot(const Project& project) {
    for (int i=1;; ++i) {
        const auto candidate = "data/studio_edits_"+std::to_string(i);
        const auto match = project.nativeFiles.lower_bound(candidate+'/');
        if (!project.nativeFiles.contains(candidate) &&
            (match == project.nativeFiles.end() || !match->first.starts_with(candidate+'/'))) return candidate+'/';
    }
}

void writeNativeRegistry(const Project& project, const std::map<std::string,json>& current,
                         const fs::path& root, const std::string& prefix) {
    const std::string file = "data/models.txt";
    if (project.nativeEdits.contains(file)) return;
    std::map<std::string,const Character*> models;
    std::set<std::string> preload;
    for (const auto& model : project.characters) {
        if (!model.nativeFile.empty()) models.emplace(model.nativeFile,&model);
        if (project.nativeEdits.contains(model.nativeFile)) continue;
        const auto prev = project.nativeBaseline.find(model.nativeFile);
        const auto original = prev == project.nativeBaseline.end() ? json{} : json::parse(prev->second);
        for (size_t i = 0; i < model.animations.size(); ++i) {
            const auto& clip = model.animations[i];
            if (clip.frames.empty()) continue;
            if (!original.is_null() && i < original.at("animations").size() &&
                !spawnCuesChanged(current.at(model.nativeFile).at("animations")[i],original.at("animations")[i])) continue;
            if (clip.subEntity.enabled) preload.insert(clip.subEntity.model);
            if (clip.item.enabled) preload.insert(clip.item.model);
        }
    }
    std::istringstream input(nativeFileText(project,file)), tokens;
    std::ostringstream out;
    std::string line;
    bool changed = false;
    while (std::getline(input,line)) {
        const auto command = nativeCommand(line,tokens);
        std::string id, path;
        if ((command == "load" || command == "know") && tokens >> id >> std::quoted(path)) {
            const auto name = nativeReference(path);
            if (const auto it = project.nativeBaseline.find(name); it != project.nativeBaseline.end()) {
                const auto model = models.find(name);
                if (model == models.end()) { changed = true; continue; }
                const auto original = json::parse(it->second);
                const bool typeChanged = model->second->type != original.at("type").get<std::string>();
                const bool promote = preload.contains(model->second->id) && command == "know";
                if (model->second->id != original.at("id").get<std::string>() || typeChanged || promote) {
                    out << (promote || model->second->type == "player" ? "load" : typeChanged ? "know" : command)
                        << ' ' << model->second->id << ' ' << path << '\n';
                    changed = true;
                    continue;
                }
            }
        }
        out << line << '\n';
    }
    for (const auto& model : project.characters) {
        if (!model.nativeFile.empty()) continue;
        changed = true;
        out << (model.type == "player" || preload.contains(model.id) ? "load " : "know ") << model.id << ' '
            << prefix << "chars/" << model.id << '/' << model.id << ".txt\n";
    }
    const auto commands = moduleModelsText(project.module);
    if (changed || !commands.empty()) writeText(root/file,commands+out.str());
}

void writeNativeLevels(const Project& project, const std::map<std::string,json>& current,
                       const fs::path& root, const std::string& prefix) {
    const std::string file = "data/levels.txt";
    if (project.nativeEdits.contains(file)) return;
    const auto addScene = [&](std::ostringstream& out, size_t i, bool after) {
        const auto& scene = after ? project.stages[i].after : project.stages[i].before;
        if (scene.file.empty()) return;
        const auto name = stageSceneName(i,after);
        exportScene(scene,root,name,prefix+"scenes/");
        out << "scene " << prefix << "scenes/" << name << ".txt\n";
    };
    std::map<std::string,size_t> stages;
    std::set<std::string> replacedScenes;
    for (size_t i=0; i<project.stages.size(); ++i) {
        const auto& stage = project.stages[i];
        if (!stage.nativeFile.empty()) stages.emplace(stage.nativeFile,i);
    }
    for (const auto& [name,baseline] : project.nativeBaseline) {
        const auto original = json::parse(baseline);
        if (!original.contains("laneTop")) continue;
        const auto now = current.find(name);
        for (const bool after : {false,true}) {
            const auto field = after ? "after" : "before";
            if (now == current.end() || now->second.at(field) != original.at(field))
                replacedScenes.insert(original.at(after ? "nativeAfterFile" : "nativeBeforeFile").get<std::string>());
        }
    }
    std::istringstream input(nativeFileText(project,file)), tokens;
    std::ostringstream out;
    std::string line, restoreZ = "z 160 230 230";
    bool changed = false, set = false, hasStage = false;
    while (std::getline(input,line)) {
        const auto command = nativeCommand(line,tokens);
        if (command == "set") { set = true; hasStage = false; }
        if (command == "z") restoreZ = line;
        std::string path;
        if (command == "scene" && tokens >> std::quoted(path) && replacedScenes.contains(nativeReference(path))) {
            changed = true;
            continue;
        }
        if (command == "file" && tokens >> std::quoted(path)) {
            const auto name = nativeReference(path);
            if (const auto base = project.nativeBaseline.find(name); base != project.nativeBaseline.end()) {
                const auto found = stages.find(name);
                if (found == stages.end()) { changed = true; continue; }
                const auto i = found->second;
                const auto& stage = project.stages[i];
                const auto original = json::parse(base->second);
                const auto& now = current.at(name);
                const bool depth = now.at("laneTop") != original.at("laneTop") ||
                                   now.at("laneBottom") != original.at("laneBottom") ||
                                   stage.backgroundDepth != original.value("backgroundDepth",-1);
                const bool before = now.at("before") != original.at("before");
                const bool after = now.at("after") != original.at("after");
                if (before) addScene(out,i,false);
                if (depth) out << stageDepthText(stage);
                out << line << '\n';
                if (depth) out << restoreZ << '\n';
                if (after) addScene(out,i,true);
                changed |= depth || before || after;
                hasStage = true;
                continue;
            }
            hasStage = true;
        }
        out << line << '\n';
    }
    for (size_t i=0; i<project.stages.size(); ++i) {
        const auto& stage = project.stages[i];
        if (!stage.nativeFile.empty()) continue;
        if (!set) {
            out << "set Adventure\nmaxplayers " << project.module.maxPlayers << '\n';
            set = true;
        }
        if (hasStage) out << "next\n";
        addScene(out,i,false);
        out << stageDepthText(stage) << "file " << prefix << "levels/" << counterName("stage_",i+1,".txt") << '\n';
        addScene(out,i,true);
        changed = hasStage = true;
    }
    const auto commands = moduleLevelsText(project.module);
    if (changed || !commands.empty()) writeText(root/file,commands+out.str());
}

} // namespace

void captureNativeBaseline(Project& project) {
    for (const auto& [name,settings] : nativeValues(project))
        project.nativeBaseline.emplace(name,settings.dump());
}

fs::path existingExportData(const fs::path& target) {
    auto path = fs::absolute(target).lexically_normal();
    if (path.filename().empty() && path != path.root_path()) path = path.parent_path();
    if (lower(utf8(path.filename())) != "data") path /= "data";
    require(fs::is_directory(path), "Choose an existing data folder or its parent game folder.");
    return path;
}

namespace {

void preparePlaytest(const Project& project, const StagePlaytest& test, const fs::path& root,
                     const std::string& prefix, bool native) {
    const auto& stage = project.stages.at(test.stage);
    const auto* player = findCharacter(project,test.player);
    require(player && player->type == "player" && hasFrames(*player),"Choose a completed player model.");
    const auto level = !stage.nativeFile.empty() ? stage.nativeFile : native
        ? prefix+"levels/"+counterName("stage_",test.stage+1,".txt") : levelPath(test.stage);
    const auto path = nativeOutputPath(project,level);
    const auto modelsFile = root/nativeOutputPath(project,"data/models.txt");
    const auto levelsFile = root/nativeOutputPath(project,"data/levels.txt");
    auto playerId = player->id;
    std::istringstream registry(readText(modelsFile)), tokens;
    std::ostringstream models;
    std::string line;
    bool registered = false;
    while (std::getline(registry,line)) {
        const auto command = nativeCommand(line,tokens);
        std::string name, model;
        if ((command == "know" || command == "load") && tokens >> name >> std::quoted(model) &&
            (player->nativeFile.empty() ? lower(name) == playerId : nativeReference(model) == player->nativeFile)) {
            if (!registered) playerId = name;
            registered = true;
            if (command == "know") {
                models << "load " << name << ' ' << model << '\n';
                continue;
            }
        }
        models << line << '\n';
    }
    require(registered,"The player is missing from models.txt.");
    writeText(modelsFile,models.str());

    std::istringstream order(readText(levelsFile));
    std::ostringstream global;
    std::string settings, branch;
    bool inSet = false, found = false;
    while (std::getline(order,line)) {
        const auto command = nativeCommand(line,tokens);
        if (command == "skiptoset") continue;
        if (command == "set") {
            inSet = true;
            settings.clear();
            branch.clear();
            continue;
        }
        if (!inSet) { global << line << '\n'; continue; }
        if (command == "branch") { branch = line+'\n'; continue; }
        if (command == "file") {
            std::string file;
            if (tokens >> std::quoted(file) && nativeReference(file) == level) { found = true; break; }
            branch.clear();
            continue;
        }
        constexpr std::array excluded{"z","scene","select","skipselect","next","end","maxplayers","cansave","ifcomplete"};
        if (std::find(excluded.begin(),excluded.end(),command) == excluded.end()) settings += line+'\n';
    }
    if (!found) { settings.clear(); branch.clear(); }
    writeText(levelsFile,global.str()+"\nskiptoset 0\nset Test_Play\n"+settings+
        "\nmaxplayers 1\ncansave 0\nskipselect "+playerId+'\n'+stageDepthText(stage)+branch+
        "file "+utf8(path)+"\nend\n");

    // A direct set launch has no Start button event to join player one.
    const auto update = root/nativeOutputPath(project,"data/scripts/update.c");
    std::string updateSource;
    if (fs::exists(update)) {
        updateSource = "#define main studio_playtest_update\n"+readText(update)+
                       "\n#undef main\n#define STUDIO_PLAYTEST_HAS_UPDATE\n";
    }
    writeText(update,updateSource+R"(
void main()
{
#ifdef STUDIO_PLAYTEST_HAS_UPDATE
    studio_playtest_update();
#endif
    if (openborvariant("in_level") && !getglobalvar("studio_playtest_joined")) {
        if (getplayerproperty(0,"entity"))
            setglobalvar("studio_playtest_joined",1);
        else
            changeplayerproperty(0,"playkeys",openborconstant("FLAG_START"));
    }
}
)");
}
}

fs::path exportProject(const Project& project, const fs::path& selectedPath, Images& images, bool playable,
                       ExportDestination mode, const StagePlaytest* playtest) {
    if (playtest) {
        require(playtest->stage < project.stages.size(),"Select a stage to test.");
        auto checked = project;
        checked.stages = {project.stages[playtest->stage]};
        verifyProject(checked);
        playable = true;
    } else verifyProject(project);
    if (mode == ExportDestination::existingPak) playable = true;
    if (mode == ExportDestination::existingData) playable = false;
    const bool native = !project.nativeFiles.empty();
    if (playable && !native) {
        require(!project.stages.empty(), "A playable export needs at least one stage.");
        require(std::any_of(project.characters.begin(),project.characters.end(),
                            [](const Character& model) { return model.type == "player"; }),
                "A playable export needs at least one player model.");
    }
    fs::path destination = fs::absolute(selectedPath).lexically_normal();
    fs::path root = destination;
    if (mode == ExportDestination::newFolder) {
        fs::create_directories(root);
        destination = root / "OpenBOR_Export";
        for (int suffix = 2; fs::exists(destination); ++suffix)
            destination = root / ("OpenBOR_Export_" + std::to_string(suffix));
    } else {
        if (mode == ExportDestination::existingData) destination = existingExportData(destination);
        else require(mediaExtension(destination) == ".pak" && fs::is_regular_file(destination),
                     "Choose an existing .pak file.");
        root = destination.parent_path();
    }
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto staging = root / (".borstudio-export-" + std::to_string(stamp));
    require(fs::create_directory(staging), "Could not create an export staging folder.");
    bool keepStaging = false;
    try {
        std::vector<std::string> rebuilt;
        const auto current = native ? nativeValues(project) : std::map<std::string,json>{};
        const auto prefix = native ? nativeExportRoot(project) : std::string{};
        for (const auto& [name,source] : project.nativeFiles) {
            const auto target = staging/nativeOutputPath(project,name);
            if (const auto edit = project.nativeEdits.find(name); edit != project.nativeEdits.end())
                writeText(target,edit->second);
            else copyMedia(source,target);
        }
        for (const auto& model : project.characters) {
            const auto assetRoot = (native ? prefix+"chars/" : "data/chars/") + model.id + '/';
            const auto modelFile = model.nativeFile.empty() ? assetRoot+model.id+".txt" : model.nativeFile;
            if (native && !model.nativeFile.empty()) {
                if (project.nativeEdits.contains(modelFile)) continue;
                const auto original = json::parse(project.nativeBaseline.at(modelFile));
                const auto& now = current.at(modelFile);
                if (modelWithoutSoundJumpOrSpawns(now) == modelWithoutSoundJumpOrSpawns(original)) {
                    bool audioChanged = now.at("deathSound") != original.at("deathSound"), spawnsChanged = false;
                    for (size_t a = 0; a < model.animations.size(); ++a) {
                        const auto& clip = now.at("animations")[a];
                        const auto& old = original.at("animations")[a];
                        audioChanged = audioChanged || clip.at("frames") != old.at("frames");
                        spawnsChanged = spawnsChanged || spawnCuesChanged(clip,old);
                    }
                    const bool jumpChanged = changedJumpField(now,original,"jumpHeight") ||
                                             changedJumpField(now,original,"jumpSpeed") ||
                                             changedJumpField(now,original,"runningJumpHeight") ||
                                             changedJumpField(now,original,"runningJumpDistance");
                    if (audioChanged || jumpChanged || spawnsChanged) {
                        auto source = nativeFileText(project,modelFile);
                        if (audioChanged) source = patchModelSound(model,now,original,source,staging,assetRoot);
                        if (spawnsChanged) source = patchModelSpawns(project,model,now,original,source);
                        writeText(staging/nativeOutputPath(project,modelFile),patchModelJump(model,now,original,source));
                    }
                    continue;
                }
                for (const auto& animation : model.animations) {
                    if (animation.frames.empty()) continue;
                    validateCue(animation.subEntity,animation,project);
                    validateCue(animation.item,animation,project);
                }
                rebuilt.push_back(modelFile);
            }
            for (const auto& [source,path] : soundPaths(model,assetRoot)) {
                validateAudio(source);
                copyMedia(source,staging/pathFromUtf8(path));
            }
            const auto palette = model.colorSets.empty() ? makeModelPalette(model,images) :
                Palette(model.sourcePalette.begin(),model.sourcePalette.end());
            auto base = model.colorSets.empty() ? palette :
                Palette(model.colorSets.front().colors.begin(),model.colorSets.front().colors.end());
            base.front() = 0;
            for (size_t i=1; i<base.size(); ++i) base[i] |= 0xff000000u;
            const auto modelDir = staging/pathFromUtf8(assetRoot);
            std::unique_ptr<GlbModel> mesh;
            if (!model.mesh.file.empty()) {
                mesh = std::make_unique<GlbModel>(model.mesh.file,images);
                mesh->write(modelDir/"model.glb",model.mesh);
            }
            bool timingImageWritten = false;
            for (size_t i=1; i<model.colorSets.size(); ++i) {
                std::string act(256*3,'\0');
                const auto& colors = model.colorSets[i].colors;
                for (size_t c=1; c<colors.size(); ++c) {
                    act[c*3] = char(colors[c] >> 16);
                    act[c*3+1] = char(colors[c] >> 8);
                    act[c*3+2] = char(colors[c]);
                }
                writeText(modelDir / counterName("palette_",i,".act"),act);
            }
            if (!model.icon.empty()) {
                const auto pixels = images.read(model.icon);
                Histogram iconHistogram;
                countColors(iconHistogram,pixels);
                images.writeIndexed(modelDir/"icon.png",pixels,makePalette(iconHistogram));
            }
            for (size_t a = 0; a < model.animations.size(); ++a) {
                const auto& animation = model.animations[a];
                for (size_t f = 0; f < animation.frames.size(); ++f) {
                    const auto& frame = animation.frames[f];
                    if (frame.image.empty()) {
                        if (!timingImageWritten) {
                            images.writeIndexed(modelDir/"mesh_timing.png",Pixels{2,2,std::vector<uint8_t>(16)},Palette{0,0xffffffffu});
                            timingImageWritten = true;
                        }
                        continue;
                    }
                    auto pixels = images.read(frame.image);
                    if (frame.effects.embeddedAlpha || !frame.effects.mask.empty()) {
                        const auto mask = images.alphaMask(frame,pixels);
                        images.writeMask(modelDir/maskName(a,f),mask);
                        for (size_t k=3; k<pixels.rgba.size(); k+=4) pixels.rgba[k] = pixels.rgba[k] ? 255 : 0;
                    }
                    images.writeIndexed(modelDir / imageName(a,f),pixels,palette,false,&base);
                }
            }
            writeText(staging/nativeDataPath(modelFile),modelText(model,mesh.get(),assetRoot));
        }
        for (size_t i = 0; i < project.stages.size(); ++i) {
            if (playtest && i != playtest->stage) continue;
            const auto& stage = project.stages[i];
            const auto level = stage.nativeFile.empty() ?
                (native ? prefix+"levels/"+counterName("stage_",i+1,".txt") : levelPath(i)) : stage.nativeFile;
            const auto cameraScript = (native ? prefix+"scripts/" : "data/scripts/")+
                                      counterName("studio_camera_",i+1,".c");
            std::string cameraBody;
            bool rebuild = true, musicChanged = true, playerDepthChanged = false;
            std::optional<std::vector<StageTerrain>> originalTerrain;
            StageCamera originalCamera;
            std::string music;
            if (native && !stage.nativeFile.empty()) {
                if (project.nativeEdits.contains(level)) continue;
                const auto original = json::parse(project.nativeBaseline.at(level));
                originalCamera = original.value("camera",StageCamera{});
                const auto& now = current.at(level);
                rebuild = stageWithoutMediaOrTerrain(now) != stageWithoutMediaOrTerrain(original);
                playerDepthChanged = stage.playerZ-stage.laneTop !=
                                     original.at("playerZ").get<int>()-original.at("laneTop").get<int>();
                const auto terrain = original.value("terrain",std::vector<StageTerrain>{});
                if (stage.terrain != terrain) originalTerrain = terrain;
                musicChanged = now.at("music") != original.at("music") || now.at("musicOffset") != original.at("musicOffset");
                if (now.at("music") == original.at("music")) {
                    const auto name = utf8(now.at("music").get<fs::path>());
                    if (project.nativeFiles.contains(name)) music = utf8(nativeOutputPath(project,name));
                }
                if (rebuild) rebuilt.push_back(level);
            }
            if (music.empty() && !stage.music.empty() && (rebuild || musicChanged)) {
                validateAudio(stage.music);
                music = native ? prefix+"music/"+counterName("stage_",i+1,mediaExtension(stage.music))
                               : stageMusicPath(stage,i);
                copyMedia(stage.music,staging/pathFromUtf8(music));
            }
            if (!native) {
                exportScene(stage.before,staging,stageSceneName(i,false));
                exportScene(stage.after,staging,stageSceneName(i,true));
            }
            if (!rebuild) {
                if (musicChanged || playerDepthChanged || originalTerrain || stage.camera != originalCamera) {
                    auto source = nativeFileText(project,level);
                    if (musicChanged || playerDepthChanged || originalTerrain)
                        source = patchStageText(source,stage,music,musicChanged,playerDepthChanged,
                                                originalTerrain ? &*originalTerrain : nullptr);
                    if (stage.camera != originalCamera)
                        source = applyStageCamera(source,stage.camera,
                            stage.camera.zoom != originalCamera.zoom || stage.camera.nativeZoom != originalCamera.nativeZoom,
                            stage.camera.offsetX != originalCamera.offsetX || stage.camera.offsetY != originalCamera.offsetY,
                            stage.camera.followAltitude != originalCamera.followAltitude,cameraScript,&cameraBody);
                    writeText(staging/nativeOutputPath(project,level),source);
                    if (!cameraBody.empty()) writeText(staging/nativeDataPath(cameraScript),cameraBody);
                }
                continue;
            }
            const auto artRoot = native ? prefix+"bgs/"+counterName("stage_",i+1,"/") : artPath(i);
            const auto art = staging/pathFromUtf8(artRoot);
            const auto exportImage = [&](const fs::path& path, const fs::path& output, bool pad) {
                const auto image = images.read(path);
                Histogram histogram;
                countColors(histogram, image);
                images.writeIndexed(output, image, makePalette(histogram), pad);
            };
            if (!stage.background.empty()) exportImage(stage.background, art/"background.png", true);
            const auto sizes = panelSizes(stage,images);
            const auto layout = stageArtLayout(stage,sizes);
            exportStagePanels(stage,layout,sizes,art,images);
            writeText(staging/nativeDataPath(level),stageTextWithLayout(stage,i,layout,artRoot,music,cameraScript,&cameraBody));
            if (!cameraBody.empty()) writeText(staging/nativeDataPath(cameraScript),cameraBody);
        }
        if (native) {
            writeNativeRegistry(project,current,staging,prefix);
            writeNativeLevels(project,current,staging,prefix);
            const auto original = json::parse(project.nativeBaseline.at("data/studio-module"));
            const auto& now = current.at("data/studio-module");
            if (now.at("videoHeader") != original.at("videoHeader") && !project.nativeEdits.contains("data/video.txt")) {
                const auto source = project.nativeFiles.contains("data/video.txt") ? nativeFileText(project,"data/video.txt") : "";
                writeText(staging/"data/video.txt",source+"\n"+project.module.videoHeader+"\n");
            }
            auto artwork = Project{};
            artwork.module.hud = project.module.hud;
            artwork.module.select = project.module.select;
            auto& screens = artwork.module.screens;
            const auto& changed = now.at("screens");
            const auto& base = original.at("screens");
            for (size_t i=0; i<screens.backgrounds.size(); ++i)
                if (changed["backgrounds"][i] != base["backgrounds"][i]) screens.backgrounds[i] = project.module.screens.backgrounds[i];
            for (size_t i=0; i<screens.fonts.size(); ++i)
                if (changed["fonts"][i] != base["fonts"][i]) screens.fonts[i] = project.module.screens.fonts[i];
            if (changed["music"] != base["music"]) screens.music = project.module.screens.music;
            for (size_t i=0; i<screens.scenes.size(); ++i) {
                const auto name = "data/scenes/"+std::string(screenSceneNames[i])+".txt";
                if (changed["scenes"][i] == base["scenes"][i] || project.nativeEdits.contains(name)) continue;
                const auto& scene = project.module.screens.scenes[i];
                if (scene.file.empty()) fs::remove(staging/pathFromUtf8(name));
                else screens.scenes[i] = scene;
            }
            exportModuleArtwork(artwork,staging,images,false);
        } else {
            writeText(staging/"data/models.txt",modelsConfigurationText(project));
            const auto levels = levelsConfigurationText(project);
            if (!levels.empty()) writeText(staging/"data/levels.txt",levels);
            writeText(staging/"data/video.txt","video 0\n"+project.module.videoHeader+"\n");
            exportModuleArtwork(project,staging,images,playable);
        }
        fs::path pak;
        if (playtest) preparePlaytest(project,*playtest,staging,prefix,native);
        if (playable) pak = writePlayablePak(staging,project.title);
        writeText(staging/"README.txt",
            "OpenBOR Studio content export\n\n"
            + std::string(playable ?
                "Copy the .pak from Paks into your OpenBOR 4.x installation's Paks folder. "
                "The editable files are included under data.\n" :
                "Merge the registrations and settings in data/models.txt and data/levels.txt into your existing module. "
                "Screen assets appear under data when you set them in Module > Screens.\n")
            + std::string(native ? "The export includes the imported data tree and any Native TXT edits.\n" :
                                   "Video mode 0 uses 320x240. Sub Entity spawn positions are relative to the parent model.\n")
            + (std::any_of(project.characters.begin(),project.characters.end(),[](const Character& model) {
                return !model.mesh.file.empty();
            }) ? "This export requires OpenBOR3D with meshmodel and meshclip support.\n" : "")
            + (std::any_of(project.stages.begin(),project.stages.end(),[](const Stage& stage) {
                return stage.camera.nativeZoom && stage.camera.zoom != 100;
            }) ? "Native camera zoom requires OpenBOR3D with camerazoom support.\n" : "")
            + [&] {
                std::string note;
                if (!rebuilt.empty()) note = "\nStudio regenerated these visually edited files. Review any custom commands against the originals in Native TXT.\n";
                for (const auto& file : rebuilt) note += file+'\n';
                return note;
            }());
        if (mode == ExportDestination::existingPak) {
            require(MoveFileExW(pak.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
                    "Could not replace "+utf8(destination)+". Close OpenBOR if it is using this PAK.");
        } else if (mode == ExportDestination::existingData) {
            const auto merged = staging/"merged";
            const auto original = staging/"original";
            const auto ordinary = [](const fs::path& path) {
                const auto flags = GetFileAttributesW(path.c_str());
                require(flags != INVALID_FILE_ATTRIBUTES && !(flags & FILE_ATTRIBUTE_REPARSE_POINT),
                        "Cannot update a data folder containing a linked path: "+utf8(path));
            };
            ordinary(destination);
            for (const auto& entry : fs::recursive_directory_iterator(destination)) ordinary(entry.path());
            fs::copy(destination,merged,fs::copy_options::recursive);
            fs::copy(staging/"data",merged,fs::copy_options::recursive | fs::copy_options::overwrite_existing);
            fs::rename(destination,original);
            try {
                fs::rename(merged,destination);
            } catch (...) {
                std::error_code error;
                fs::rename(original,destination,error);
                if (error) {
                    keepStaging = true;
                    throw std::runtime_error("Could not restore the data folder. Your original files are in "+utf8(original)+".");
                }
                throw;
            }
        } else {
            fs::rename(staging,destination);
        }
    } catch (...) {
        std::error_code ec;
        if (!keepStaging) fs::remove_all(staging,ec);
        throw;
    }
    if (mode != ExportDestination::newFolder) {
        std::error_code error;
        fs::remove_all(staging,error);
    }
    return destination;
}
}

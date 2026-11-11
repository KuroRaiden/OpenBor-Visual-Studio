#include "project.hpp"
#include "images.hpp"

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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Frame, image, ticks, originX, originY, body, attack)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SpawnCue, enabled, frame, model, x, z, altitude, velocityX, velocityZ, velocityY)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Animation, name, loop, frames, projectile, item)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Character, id, title, type, health, speed, animations)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(StageSpawn, model, at, x, z, altitude, flip, boss, item)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Wave, at, wait)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Stage, title, background, panels, laneTop, laneBottom, playerX, playerZ, spawns, waves)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Project, title, characters, stages)

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
void visitImages(Project& project, Callback&& callback) {
    for (auto& model : project.characters) {
        for (auto& animation : model.animations) {
            for (auto& frame : animation.frames) callback(frame.image);
        }
    }
    for (auto& stage : project.stages) {
        if (!stage.background.empty()) callback(stage.background);
        for (auto& panel : stage.panels) callback(panel);
    }
}

std::string counterName(std::string_view prefix, size_t index, std::string_view suffix) {
    std::ostringstream name;
    name << prefix << std::setw(3) << std::setfill('0') << index << suffix;
    return name.str();
}

std::string imageName(size_t animation, size_t frame) {
    return counterName("a", animation, "_") + counterName("f", frame, ".png");
}

std::string modelPath(const Character& character) {
    return "data/chars/" + character.id + "/" + character.id + ".txt";
}

std::string levelPath(size_t index) {
    return "data/levels/" + counterName("stage_", index + 1, ".txt");
}

std::string artPath(size_t index) {
    return "data/bgs/" + counterName("stage_", index + 1, "/");
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
    require(std::isfinite(cue.velocityX) && std::isfinite(cue.velocityY) && std::isfinite(cue.velocityZ),
            "Spawn velocity must be finite.");
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

const Character* findCharacter(const Project& project, const std::string& id) {
    const auto key = lower(id);
    const auto it = std::find_if(project.characters.begin(), project.characters.end(),
                                 [&](const Character& c) { return lower(c.id) == key; });
    return it == project.characters.end() ? nullptr : &*it;
}

const Frame* previewFrame(const Character* character) {
    if (!character) return nullptr;
    for (const auto& animation : character->animations) {
        if (animation.name == "idle" && !animation.frames.empty()) return &animation.frames.front();
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

Project openProject(const fs::path& file) {
    const auto root = fs::absolute(file).parent_path();
    const json data = json::parse(readText(file));
    require(data.value("version", 0) == 1, "Unsupported OpenBOR Studio project version.");
    auto project = data.at("project").get<Project>();
    visitImages(project, [&](fs::path& asset) {
        require(!asset.empty() && !asset.has_root_path(), "Project asset path must be relative.");
        const auto relative = asset.lexically_normal();
        require(relative.begin() != relative.end() && *relative.begin() != L"..",
                "Project asset path points outside its directory.");
        asset = root / relative;
    });
    return project;
}

void saveProject(const Project& project, const fs::path& file) {
    const auto full = fs::absolute(file);
    fs::path assetDir = full.stem();
    assetDir += L".assets";
    Project portable = project;
    std::map<fs::path, fs::path> copied;
    visitImages(portable, [&](fs::path& asset) {
        if (const auto found = copied.find(asset); found != copied.end()) {
            asset = found->second;
            return;
        }
        const auto contents = readText(asset);
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char byte : contents) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        std::ostringstream key;
        key << std::hex << hash;
        auto relative = assetDir / pathFromUtf8(key.str() + lower(utf8(asset.extension())));
        const auto output = full.parent_path() / relative;
        if (fs::exists(output)) require(readText(output) == contents, "Asset fingerprint collision.");
        else writeText(output, contents);
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
            rename(animation.projectile.model);
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
    Animation* animation = nullptr;
    Frame properties;
    bool explicitOrigin = false;
    std::istringstream input(readText(file));
    std::string line;
    skipped = 0;
    while (std::getline(input, line)) {
        line.resize(std::min(line.find('#'), line.size()));
        std::istringstream tokens(line);
        std::string command, value;
        if (!(tokens >> command)) continue;
        command = lower(command);
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
        } else if (command == "anim") {
            if (!(tokens >> value)) throw std::runtime_error("Animation is missing its name.");
            animation = &character.animations.emplace_back();
            animation->name = lower(value);
            animation->loop = false;
            properties = Frame{};
            explicitOrigin = false;
        } else if (animation && command == "frame") {
            if (!(tokens >> value)) throw std::runtime_error("Frame command is missing its image.");
            const auto path = resolveFrame(file,root,value);
            auto frame = makeFrame(path,images);
            frame.ticks = properties.ticks;
            if (explicitOrigin) {
                frame.originX = properties.originX;
                frame.originY = properties.originY;
            }
            frame.body = properties.body;
            frame.attack = properties.attack;
            animation->frames.push_back(std::move(frame));
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
            tokens >> animation->projectile.model;
            animation->projectile.model = modelToken(animation->projectile.model);
        } else if (animation && command == "throwframe") {
            auto& cue = animation->projectile;
            cue.enabled = bool(tokens >> cue.frame);
            tokens >> cue.altitude;
        } else if (animation && command == "projectile_position_x") tokens >> animation->projectile.x;
        else if (animation && command == "projectile_position_y") tokens >> animation->projectile.altitude;
        else if (animation && command == "projectile_position_z") tokens >> animation->projectile.z;
        else if (animation && command == "projectile_velocity_x") tokens >> animation->projectile.velocityX;
        else if (animation && command == "projectile_velocity_y") tokens >> animation->projectile.velocityY;
        else if (animation && command == "projectile_velocity_z") tokens >> animation->projectile.velocityZ;
        else if (animation && (command == "subentity" || command == "custentity")) {
            tokens >> animation->item.model;
            animation->item.model = modelToken(animation->item.model);
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
    require(hasFrames(character), "No readable animation frames were found in that model.");
    return character;
}

std::string modelText(const Character& model) {
    std::ostringstream out;
    out << "name " << model.id << "\ntype " << model.type << "\n";
    if (model.type == "player" || model.type == "enemy" || model.type == "npc" || model.type == "obstacle") {
        out << "health " << model.health << "\nspeed " << model.speed << '\n';
    }
    if (model.type == "item") out << "health " << model.health << '\n';
    for (size_t a = 0; a < model.animations.size(); ++a) {
        const auto& animation = model.animations[a];
        if (animation.frames.empty()) continue;
        out << "\nanim " << animation.name << "\n    loop " << int(animation.loop) << '\n';
        const auto& p = animation.projectile;
        if (p.enabled) {
            out << "    custknife " << p.model << "\n    throwframe " << p.frame
                << ' ' << (p.altitude == 0 ? -1 : p.altitude)
                << "\n    projectile_position_x " << p.x
                << "\n    projectile_position_z " << p.z
                << "\n    projectile_position_y " << p.altitude
                << "\n    projectile_velocity_x " << p.velocityX
                << "\n    projectile_velocity_z " << p.velocityZ
                << "\n    projectile_velocity_y " << p.velocityY << '\n';
        }
        const auto& item = animation.item;
        if (item.enabled) {
            out << "    subentity " << item.model << "\n    spawnframe "
                << item.frame << ' ' << item.x << ' ' << item.z << ' ' << item.altitude << " 0\n";
        }
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
            out << "\n    frame data/chars/" << model.id << '/' << imageName(a,f) << '\n';
        }
    }
    return out.str();
}

std::string stageText(const Stage& stage, size_t index) {
    const auto art = artPath(index);
    std::ostringstream out;
    out << "# " << modelToken(stage.title) << "\ndirection right\n";
    if (!stage.background.empty()) out << "background " << art << "background.png\n";
    for (size_t panel = 0; panel < stage.panels.size(); ++panel) {
        out << "panel " << art << counterName("panel_",panel+1,".png") << " none none\n";
    }
    if (!stage.panels.empty()) {
        out << "order ";
        for (size_t panel = 0; panel < stage.panels.size(); ++panel) out << char('a'+panel);
        out << '\n';
    }
    out << "spawn1 " << stage.playerX << ' ' << (stage.playerZ - stage.laneTop) << " 0\n";
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
        } else out << "wait\n";
        out << "at " << event.at << '\n';
    }
    return out.str();
}

void verifyProject(const Project& project) {
    require(!project.characters.empty() || !project.stages.empty(), "Add a model or a stage before exporting.");
    require(project.stages.size() <= 100, "Export supports up to 100 stages per level set.");
    std::set<std::string> ids;
    for (const auto& model : project.characters) {
        require(validToken(model.id), "Invalid model ID: " + model.id);
        require(ids.insert(model.id).second, "Duplicate model ID: " + model.id);
        require(knownType(model.type), "Unsupported model type: " + model.type);
        require(hasFrames(model), "Add frames to " + model.id + " before exporting.");
        require(std::isfinite(model.speed) && model.speed >= 0, "Model speed must be nonnegative and finite.");
        std::set<std::string> names;
        for (const auto& animation : model.animations) {
            if (animation.frames.empty()) continue;
            require(validToken(animation.name), "Use an OpenBOR animation ID in " + model.id + ".");
            require(names.insert(animation.name).second, "Duplicate animation " + animation.name + " in " + model.id + ".");
            validateCue(animation.projectile, animation, project);
            validateCue(animation.item, animation, project);
            for (const auto& frame : animation.frames) {
                require(!frame.image.empty() && frame.ticks > 0, "An animation frame in " + model.id + " needs an image and a delay.");
            }
        }
        require(names.contains("idle"), "Add an idle animation with frames to " + model.id + ".");
    }
    for (const auto& stage : project.stages) {
        require(!stage.panels.empty(), "Add a panel to " + stage.title + " before exporting.");
        require(stage.panels.size() <= 26, "A stage can have at most 26 panel slots.");
        require(stage.laneTop >= 0 && stage.laneBottom >= stage.laneTop && stage.laneBottom < 240,
                "Set a valid 0–239 walkable depth range for " + stage.title + ".");
        require(stage.playerZ >= stage.laneTop && stage.playerZ <= stage.laneBottom,
                "Place the player start inside the walkable lane in " + stage.title + ".");
        require(stage.spawns.size() + stage.waves.size() <= 600, "Stage spawn and wait events exceed the engine limit.");
        for (const auto& spawn : stage.spawns) {
            require(findCharacter(project,spawn.model), "Choose a model for each spawn in " + stage.title + ".");
            require(spawn.at >= 0, "Stage spawn triggers cannot be negative.");
            require(spawn.item.empty() || findCharacter(project,spawn.item),
                    "Choose an existing carried item model in " + stage.title + ".");
        }
        for (const auto& wave : stage.waves) require(wave.at >= 0, "Stage waits cannot be negative.");
    }
}

fs::path exportProject(const Project& project, const fs::path& parent, Images& images) {
    verifyProject(project);
    const auto root = fs::absolute(parent);
    fs::path destination = root / "OpenBOR_Export";
    for (int suffix = 2; fs::exists(destination); ++suffix) destination = root / ("OpenBOR_Export_" + std::to_string(suffix));
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto staging = root / (".borstudio-export-" + std::to_string(stamp));
    require(fs::create_directory(staging), "Could not create an export staging folder.");
    try {
        std::ostringstream models, levels;
        int specials = 8;
        for (const auto& model : project.characters) {
            for (const auto& animation : model.animations) specials = std::max(specials,freeSpecial(animation.name));
        }
        models << "maxfreespecials " << specials << '\n';
        for (const auto& model : project.characters) {
            models << (model.type == "player" ? "load " : "know ") << model.id << ' ' << modelPath(model) << '\n';
            Histogram histogram;
            std::set<fs::path> uniqueImages;
            for (const auto& animation : model.animations) {
                for (const auto& frame : animation.frames) uniqueImages.insert(frame.image);
            }
            for (const auto& path : uniqueImages) countColors(histogram, images.read(path));
            const auto palette = makePalette(histogram);
            const auto modelDir = staging / "data" / "chars" / model.id;
            for (size_t a = 0; a < model.animations.size(); ++a) {
                const auto& animation = model.animations[a];
                for (size_t f = 0; f < animation.frames.size(); ++f) {
                    images.writeIndexed(modelDir / imageName(a,f), images.read(animation.frames[f].image), palette);
                }
            }
            writeText(staging / modelPath(model), modelText(model));
        }
        for (size_t i = 0; i < project.stages.size(); ++i) {
            const auto& stage = project.stages[i];
            const auto art = staging / artPath(i);
            const auto exportImage = [&](const fs::path& path, const fs::path& output, bool pad) {
                const auto image = images.read(path);
                Histogram histogram;
                countColors(histogram, image);
                images.writeIndexed(output, image, makePalette(histogram), pad);
            };
            if (!stage.background.empty()) exportImage(stage.background, art/"background.png", true);
            for (size_t panel = 0; panel < stage.panels.size(); ++panel) {
                exportImage(stage.panels[panel], art/counterName("panel_",panel+1,".png"), false);
            }
            writeText(staging/levelPath(i), stageText(stage,i));
            if (i == 0) levels << "set Adventure\n";
            levels << "z " << stage.laneTop << ' ' << stage.laneBottom << ' ' << stage.laneBottom << "\n"
                   << "file " << levelPath(i) << "\n";
            if (i+1 < project.stages.size()) levels << "next\n";
        }
        writeText(staging/"data/models.txt", models.str());
        if (!project.stages.empty()) writeText(staging/"data/levels.txt",levels.str());
        writeText(staging/"data/video.txt","video 0\n");
        writeText(staging/"README.txt",
            "OpenBOR Studio content export\n\n"
            "This data folder contains OpenBOR 4.x model and stage assets. Merge the registrations "
            "in models.txt and levels.txt into your module. Your module supplies its menu, fonts, "
            "HUD, audio and other shared resources. Video mode 0 uses 320x240. "
            "Projectile XYZ offsets target recent OpenBOR 4.x engine builds.\n");
        fs::rename(staging, destination);
    } catch (...) {
        std::error_code ec;
        fs::remove_all(staging,ec);
        throw;
    }
    return destination;
}
}

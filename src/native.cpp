#include "native.hpp"
#include "images.hpp"
#include "media.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <stdexcept>

namespace studio {

std::string nativeReference(std::string path) {
    std::replace(path.begin(),path.end(),'\\','/');
    return lower(std::move(path));
}

fs::path nativeDataPath(std::string_view name) {
    const auto fail = [&] { throw std::runtime_error("Invalid data path in the OpenBOR module: "+std::string(name)); };
    if (name.size() < 6 || lower(std::string(name.substr(0,5))) != "data/") fail();
    for (size_t start = 0; start < name.size();) {
        const auto end = name.find('/',start);
        const auto part = name.substr(start,end-start);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ' ||
            part.find_first_of("\\:*?\"<>|") != std::string_view::npos ||
            std::any_of(part.begin(),part.end(),[](unsigned char c) { return c < 32; })) fail();
        const auto base = lower(std::string(part.substr(0,part.find('.'))));
        if (base == "con" || base == "prn" || base == "aux" || base == "nul" || base == "conin$" || base == "conout$")
            fail();
        if (base.starts_with("com") || base.starts_with("lpt")) {
            const auto suffix = std::string_view(base).substr(3);
            if ((suffix.size() == 1 && suffix[0] >= '0' && suffix[0] <= '9') ||
                suffix == "¹" || suffix == "²" || suffix == "³") fail();
        }
        if (end == std::string_view::npos) break;
        start = end+1;
        if (start == name.size()) fail();
    }
    return pathFromUtf8(std::string(name));
}

fs::path nativeOutputPath(const Project& project, const std::string& name) {
    if (const auto it = project.nativeNames.find(name); it != project.nativeNames.end())
        return nativeDataPath(it->second);
    return nativeDataPath(name);
}

std::string nativeCommand(std::string_view line, std::istringstream& tokens) {
    if (line.starts_with("\xef\xbb\xbf")) line.remove_prefix(3);
    tokens.clear();
    tokens.str(std::string(line.substr(0,line.find('#'))));
    std::string command;
    tokens >> command;
    return lower(command);
}

bool startsInlineScript(std::istringstream& tokens) {
    const auto pos = tokens.tellg();
    std::string argument;
    tokens >> argument;
    tokens.clear();
    if (pos != std::streampos(-1)) tokens.seekg(pos);
    return lower(argument) == "@script";
}

StageCamera readStageCamera(const std::string& source) {
    StageCamera camera;
    std::istringstream input(source), tokens;
    bool script = false;
    for (std::string line; std::getline(input,line);) {
        if (script) {
            if (line.find("@end_script") != std::string::npos) script = false;
            continue;
        }
        constexpr std::string_view marker = "# studio_camera_zoom ";
        const bool generated = line.starts_with(marker);
        if (generated) line = "camerazoom "+line.substr(marker.size());
        if (const auto command = nativeCommand(line,tokens); command == "camerazoom") {
            int zoom;
            if (tokens >> zoom) {
                camera.zoom = zoom > 0 ? std::clamp(zoom,cameraZoomMin,cameraZoomMax) : 100;
                camera.nativeZoom = !generated;
            }
        } else if (command == "cameraoffset") {
            tokens >> camera.offsetX >> camera.offsetY;
        } else if (command == "cameratype") {
            int type = 0;
            tokens >> type;
            camera.followAltitude = type == 1;
        } else if (startsInlineScript(tokens)) {
            script = line.find("@end_script") == std::string::npos;
        }
    }
    return camera;
}

std::optional<StageTerrain> readStageTerrain(std::string_view command, std::istringstream& tokens) {
    if (command != "wall" && command != "hole") return {};
    std::array<int,9> values{};
    size_t count = 0;
    for (std::string token; tokens >> token;) {
        if (count == values.size()) return {};
        std::istringstream number(token);
        double value;
        if (!(number >> value) || number.peek() != EOF || !std::isfinite(value) ||
            std::abs(value) > stageTerrainLimit || value != std::trunc(value)) return {};
        values[count++] = int(value);
    }
    const bool pit = command == "hole";
    const auto [x,z,upperLeft,lowerLeft,upperRight,lowerRight,depth,altitude,type] = values;
    if (count < (pit ? 7u : 8u) || upperLeft != lowerLeft || upperRight != lowerRight ||
        upperRight <= upperLeft || depth <= 0 || altitude < (pit ? 0 : 1) ||
        std::abs(x+upperLeft) > stageTerrainLimit || upperRight-upperLeft > stageTerrainLimit) return {};
    return StageTerrain{pit,x+upperLeft,z,upperRight-upperLeft,depth,altitude,type};
}

namespace {

uint64_t little(std::istream& file, int bytes) {
    uint64_t value = 0;
    for (int i=0; i<bytes; ++i) {
        const int byte = file.get();
        if (byte == EOF) throw std::runtime_error("The OpenBOR PAK is truncated.");
        value |= uint64_t(byte) << (8*i);
    }
    return value;
}

fs::path asset(const Project& project, const std::string& name, std::initializer_list<const char*> extensions = {}) {
    const auto ref = nativeReference(name);
    if (const auto it = project.nativeFiles.find(ref); it != project.nativeFiles.end()) return it->second;
    if (!pathFromUtf8(ref).has_extension()) {
        for (const auto ext : extensions) {
            if (const auto it = project.nativeFiles.find(ref+ext); it != project.nativeFiles.end()) return it->second;
        }
    }
    return {};
}

ScreenScene readScene(const Project& project, const std::string& name) {
    ScreenScene scene;
    if (!project.nativeFiles.contains(name)) return scene;
    std::istringstream script(nativeFileText(project,name)), tokens;
    std::string line;
    while (std::getline(script,line)) {
        const auto command = nativeCommand(line,tokens);
        if (command.empty()) continue;
        std::string file;
        if (!scene.file.empty() || !(tokens >> std::quoted(file))) return {};
        if (command != "video" && command != "animation") return {};
        scene.file = asset(project,file);
        int skipOne = 0, noSkip = 0;
        if (command == "animation") tokens >> scene.position.x >> scene.position.y;
        tokens >> skipOne >> noSkip;
        if (skipOne || scene.file.empty()) return {};
        scene.skippable = !noSkip;
    }
    return scene;
}

Stage readStage(const Project& project, const std::string& name, int top, int bottom, int backgroundDepth) {
    Stage stage;
    stage.nativeFile = name;
    stage.title = utf8(pathFromUtf8(name).stem());
    stage.laneTop = top;
    stage.laneBottom = bottom;
    stage.backgroundDepth = backgroundDepth;
    stage.playerZ = std::clamp(stage.playerZ,top,bottom);
    StageSpawn spawn;
    bool pendingSpawn = false, pendingWait = false;
    std::string order;
    std::vector<StagePanel> panels;
    const auto source = nativeFileText(project,name);
    stage.camera = readStageCamera(source);
    std::istringstream input(source), tokens;
    std::string line;
    bool script = false;
    while (std::getline(input,line)) {
        if (script) {
            if (line.find("@end_script") != std::string::npos) script = false;
            continue;
        }
        const auto command = nativeCommand(line,tokens);
        if (startsInlineScript(tokens)) {
            script = line.find("@end_script") == std::string::npos;
            continue;
        }
        std::string value;
        if (command == "wall" || command == "hole") {
            if (const auto terrain = readStageTerrain(command,tokens)) stage.terrain.push_back(*terrain);
        } else if (command == "background" && stage.background.empty() && tokens >> std::quoted(value)) {
            stage.background = asset(project,value,{".png",".gif",".bmp"});
            float xRatio, zRatio;
            int xSpace, ySpace, xRepeat = -1, yRepeat = -1;
            tokens >> xRatio >> zRatio >> stage.backgroundOffset.x >> stage.backgroundOffset.y
                   >> xSpace >> ySpace >> xRepeat >> yRepeat;
            stage.repeatBackgroundX = xRepeat != 1;
            stage.repeatBackgroundY = yRepeat != 1;
        } else if (command == "panel" && tokens >> std::quoted(value)) {
            panels.push_back({asset(project,value,{".png",".gif",".bmp"}),{}});
        } else if (command == "order") {
            tokens >> order;
        } else if (command == "music" && tokens >> std::quoted(value)) {
            stage.music = asset(project,value,{".ogg",".oga",".wav",".bor"});
            tokens >> stage.musicOffset;
        } else if (command == "spawn1") {
            int z = stage.playerZ-top;
            tokens >> stage.playerX >> z;
            stage.playerZ = std::clamp(z,0,bottom-top)+top;
        } else if (command == "spawn" && tokens >> value) {
            spawn = {};
            spawn.model = modelToken(value);
            pendingSpawn = true;
            pendingWait = false;
        } else if (command == "wait") {
            pendingWait = true;
            pendingSpawn = false;
        } else if (command == "at") {
            int at;
            if (tokens >> at) {
                if (pendingWait) stage.waves.push_back({at,true});
                if (pendingSpawn) {
                    spawn.at = at;
                    stage.spawns.push_back(spawn);
                }
            }
            pendingWait = pendingSpawn = false;
        } else if (pendingSpawn) {
            if (command == "coords") tokens >> spawn.x >> spawn.z >> spawn.altitude;
            if (command == "flip") tokens >> spawn.flip;
            if (command == "boss") tokens >> spawn.boss;
            if (command == "health") tokens >> spawn.health;
            if (command == "mp") tokens >> spawn.mp;
            if (command == "map") tokens >> spawn.palette;
            if (command == "item" && tokens >> value) spawn.item = modelToken(value);
        }
    }
    if (order.empty()) {
        for (const auto& panel : panels)
            if (!panel.image.empty()) stage.panels.push_back(panel);
    } else {
        for (const char c : lower(order)) {
            const auto i = size_t(c-'a');
            if (i < panels.size() && !panels[i].image.empty()) stage.panels.push_back(panels[i]);
        }
    }
    return stage;
}
}

std::string nativeFileText(const Project& project, const std::string& name) {
    if (const auto it = project.nativeEdits.find(name); it != project.nativeEdits.end()) return it->second;
    const auto source = project.nativeFiles.at(name);
    std::ifstream file(source,std::ios::binary);
    if (!file || fs::file_size(source) > 8'000'000)
        throw std::runtime_error("Cannot edit " + name + " as text.");
    std::string result{std::istreambuf_iterator<char>(file),{}};
    if (result.find('\0') != std::string::npos) throw std::runtime_error("File is not plain text: " + name);
    return result;
}

fs::path unpackModule(const fs::path& pak, const fs::path& parent) {
    std::ifstream file(pak,std::ios::binary);
    char magic[4];
    if (!file.read(magic,4) || std::string_view(magic,4) != "PACK")
        throw std::runtime_error("Choose a valid OpenBOR PAK.");
    const auto format = little(file,4);
    if (format > 1) throw std::runtime_error("This OpenBOR PAK version is unsupported.");
    const int width = format == 1 ? 8 : 4;
    const auto size = fs::file_size(pak);
    if (size < uint64_t(8+width) || size > uint64_t(std::numeric_limits<std::streamoff>::max()))
        throw std::runtime_error("Invalid OpenBOR PAK size.");
    const auto end = size-width;
    file.seekg(std::streamoff(end));
    const auto table = little(file,width);
    if (table < 8 || table >= end) throw std::runtime_error("Invalid OpenBOR PAK table.");
    struct Entry { fs::path path; uint64_t offset, size; };
    std::vector<Entry> entries;
    std::set<std::string> paths;
    file.seekg(std::streamoff(table));
    for (auto at = table; at < end;) {
        const auto record = little(file,4);
        const int header = 4+2*width;
        if (record <= uint64_t(header) || record > end-at || record > uint64_t(header+32768))
            throw std::runtime_error("Invalid OpenBOR PAK entry.");
        const auto offset = little(file,width), length = little(file,width);
        if (offset < 8 || offset > table || length > table-offset)
            throw std::runtime_error("Invalid OpenBOR PAK file range.");
        std::string name(size_t(record-header),'\0');
        if (!file.read(name.data(),std::streamsize(name.size())) || name.back() != '\0' || name.find('\0') != name.size()-1)
            throw std::runtime_error("Invalid OpenBOR PAK file name.");
        name.pop_back();
        std::replace(name.begin(),name.end(),'\\','/');
        const auto key = nativeReference(name);
        if (key.starts_with("data/")) {
            auto path = nativeDataPath(name);
            if (!paths.insert(key).second) throw std::runtime_error("Duplicate PAK path: "+name);
            entries.push_back({std::move(path),offset,length});
        }
        at += record;
    }
    if (!paths.contains("data/models.txt") || !paths.contains("data/levels.txt"))
        throw std::runtime_error("The PAK needs data/models.txt and data/levels.txt.");
    const auto stem = modelToken(utf8(pak.stem()));
    const auto base = stem.empty() ? "openbor_module" : stem;
    auto output = parent/pathFromUtf8(base+"_unpacked");
    for (int suffix=2; fs::exists(output); ++suffix)
        output = parent/pathFromUtf8(base+"_unpacked_"+std::to_string(suffix));
    if (!fs::create_directory(output)) throw std::runtime_error("Could not create the unpacked module folder.");
    try {
        std::array<char,65536> buffer{};
        for (const auto& entry : entries) {
            const auto destination = output/entry.path;
            fs::create_directories(destination.parent_path());
            std::ofstream target(destination,std::ios::binary);
            if (!target) throw std::runtime_error("Could not extract "+utf8(entry.path)+".");
            file.seekg(std::streamoff(entry.offset));
            for (auto left = entry.size; left > 0;) {
                const auto count = std::min<uint64_t>(left,buffer.size());
                if (!file.read(buffer.data(),std::streamsize(count)) || !target.write(buffer.data(),std::streamsize(count)))
                    throw std::runtime_error("Could not extract "+utf8(entry.path)+".");
                left -= count;
            }
            target.close();
            if (!target) throw std::runtime_error("Could not finish extracting "+utf8(entry.path)+".");
        }
    } catch (...) {
        std::error_code error;
        fs::remove_all(output,error);
        throw;
    }
    return output;
}

Project importModule(const fs::path& folder, Images& images, int& skipped) {
    const auto selected = fs::absolute(folder).lexically_normal();
    const auto data = lower(utf8(selected.filename())) == "data" ? selected : selected/"data";
    if (!fs::is_directory(data)) throw std::runtime_error("Choose the data folder or its parent module folder.");
    Project project;
    project.title = utf8(data.parent_path().filename());
    for (const auto& item : fs::recursive_directory_iterator(data)) {
        if (!item.is_regular_file() || item.is_symlink()) continue;
        const auto original = utf8(item.path().lexically_relative(data.parent_path()));
        const auto relative = nativeReference(original);
        (void)nativeDataPath(relative);
        if (!project.nativeFiles.emplace(relative,item.path()).second)
            throw std::runtime_error("Duplicate module path: "+relative);
        if (original != relative) project.nativeNames.emplace(relative,original);
    }
    if (!project.nativeFiles.contains("data/models.txt") || !project.nativeFiles.contains("data/levels.txt"))
        throw std::runtime_error("The module needs data/models.txt and data/levels.txt.");
    skipped = 0;
    std::set<std::string> models, levels;
    std::string line;
    std::istringstream registry(nativeFileText(project,"data/models.txt")), tokens;
    while (std::getline(registry,line)) {
        const auto command = nativeCommand(line,tokens);
        std::string id, file;
        if ((command != "load" && command != "know") || !(tokens >> id >> std::quoted(file))) continue;
        const auto name = nativeReference(file);
        if (!models.insert(name).second) continue;
        try {
            int omitted = 0;
            auto character = importCharacterFile(project.nativeFiles.at(name),images,omitted);
            character.id = modelToken(id);
            character.nativeFile = name;
            if (findCharacter(project,character.id)) { ++skipped; continue; }
            project.characters.push_back(std::move(character));
        } catch (const std::exception&) { ++skipped; }
    }
    std::istringstream order(nativeFileText(project,"data/levels.txt"));
    int top = 160, bottom = 230, backgroundDepth = -1;
    std::string before;
    size_t lastStage = size_t(-1);
    while (std::getline(order,line)) {
        const auto command = nativeCommand(line,tokens);
        if (command == "set" || command == "next") {
            before.clear();
            lastStage = size_t(-1);
        }
        if (command == "scene") {
            std::string file;
            if (tokens >> std::quoted(file)) {
                const auto scene = nativeReference(file);
                if (scene.ends_with("_before.txt")) before = scene;
                if (scene.ends_with("_after.txt") && lastStage < project.stages.size()) {
                    auto& stage = project.stages[lastStage];
                    stage.after = readScene(project,scene);
                    if (!stage.after.file.empty()) stage.nativeAfterFile = scene;
                }
            }
        }
        if (command == "z") {
            int first = 0, last = 0, reference = 0;
            if (tokens >> first >> last) {
                top = std::clamp(first > 0 ? first : 160,0,239);
                bottom = std::clamp(last > 0 ? last : 230,top,239);
                tokens >> reference;
                backgroundDepth = reference > 0 ? reference : 160;
            }
        }
        if (command != "file") continue;
        std::string file;
        if (!(tokens >> std::quoted(file))) continue;
        const auto name = nativeReference(file);
        lastStage = size_t(-1);
        if (levels.insert(name).second) {
            if (!project.nativeFiles.contains(name)) ++skipped;
            else try {
                auto stage = readStage(project,name,top,bottom,backgroundDepth);
                if (!before.empty()) {
                    stage.before = readScene(project,before);
                    if (!stage.before.file.empty()) stage.nativeBeforeFile = before;
                }
                lastStage = project.stages.size();
                project.stages.push_back(std::move(stage));
            } catch (const std::exception&) { ++skipped; }
        }
        before.clear();
    }
    auto& screens = project.module.screens;
    for (size_t i=0; i<screens.backgrounds.size(); ++i)
        screens.backgrounds[i] = asset(project,"data/bgs/"+std::string(screenBackgroundNames[i]),{".png",".gif",".bmp"});
    for (size_t i=0; i<screens.fonts.size(); ++i)
        screens.fonts[i].bitmap = asset(project,"data/sprites/"+std::string(screenFontNames[i]),{".png",".gif",".bmp"});
    for (size_t i=0; i<screens.scenes.size(); ++i)
        screens.scenes[i] = readScene(project,"data/scenes/"+std::string(screenSceneNames[i])+".txt");
    screens.music = asset(project,"data/music/menu",{".ogg",".bor"});
    captureNativeBaseline(project);
    return project;
}
}

#include "ui.hpp"
#include "imgui_stdlib.h"
#include <shobjidl.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace studio {
namespace {

constexpr auto accent = IM_COL32(253,174,101,255);
constexpr auto ink = IM_COL32(229,237,246,255);
constexpr auto dim = IM_COL32(150,166,186,255);
constexpr auto bodyColor = IM_COL32(111,228,173,255);
constexpr auto attackColor = IM_COL32(253,110,118,255);
constexpr auto shotColor = IM_COL32(93,211,246,255);
constexpr auto itemColor = IM_COL32(248,199,110,255);
constexpr auto guideColor = IM_COL32(110,142,178,100);

ImVec2 add(ImVec2 a, ImVec2 b) { return {a.x+b.x,a.y+b.y}; }
ImVec2 sub(ImVec2 a, ImVec2 b) { return {a.x-b.x,a.y-b.y}; }
ImVec2 mul(ImVec2 a, float scale) { return {a.x*scale,a.y*scale}; }

void section(const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(.76f,.82f,.90f,1), "%s", title);
    ImGui::Spacing();
}

void textDim(const char* text) {
    ImGui::TextColored(ImVec4(.55f,.63f,.74f,1), "%s", text);
}

bool filled(const char* label, ImVec2 size = {}) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.98f,.62f,.31f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,.72f,.43f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.87f,.47f,.24f,1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.12f,.09f,.07f,1));
    const bool click = ImGui::Button(label,size);
    ImGui::PopStyleColor(4);
    return click;
}

bool toggleButton(const char* title, bool active, ImVec2 size = {}) {
    if (active) return filled(title,size);
    return ImGui::Button(title,size);
}

void labelTip(const char* tip) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) ImGui::SetTooltip("%s",tip);
}

void child(const char* id, ImVec2 size, ImGuiWindowFlags flags = 0) {
    ImGui::BeginChild(id,size,ImGuiChildFlags_Borders, flags);
}

void image(ImDrawList* draw, const Texture* tex, ImVec2 a, ImVec2 b, bool flipped = false,
           ImU32 tint = IM_COL32_WHITE) {
    if (!tex) return;
    draw->AddImage(ImTextureID(reinterpret_cast<uintptr_t>(tex->view.Get())), a,b,
                   {flipped ? 1.0f : 0.0f,0},{flipped ? 0.0f : 1.0f,1},tint);
}

void sprite(ImDrawList* draw, Images& images, const Frame& frame, ImVec2 origin,
            float scale, bool flipped, ImU32 tint = IM_COL32_WHITE) {
    const auto tex = images.texture(frame.image);
    if (!tex) return;
    const int pivotX = flipped ? tex->width-frame.originX : frame.originX;
    const ImVec2 a{origin.x - pivotX*scale, origin.y - frame.originY*scale};
    image(draw, tex, a, add(a,{tex->width*scale,tex->height*scale}), flipped, tint);
}

void thumbnail(ImDrawList* draw, const Texture* tex, ImVec2 a, ImVec2 size) {
    if (!tex) return;
    const float zoom = std::min({size.x/tex->width,size.y/tex->height,4.0f});
    const ImVec2 scaled{tex->width*zoom,tex->height*zoom};
    const auto top = add(a,mul(sub(size,scaled),.5f));
    image(draw,tex,top,add(top,scaled));
}

void label(ImDrawList* draw, ImVec2 p, const char* text, ImU32 color) {
    const ImVec2 size = ImGui::CalcTextSize(text);
    draw->AddRectFilled(sub(p,{5,3}),add(p,add(size,{5,3})),IM_COL32(12,19,30,235),4);
    draw->AddText(p,color,text);
}

void cross(ImDrawList* draw, ImVec2 point, ImU32 color, float radius = 7.0f) {
    draw->AddCircleFilled(point,radius+2,IM_COL32(12,19,30,235),16);
    draw->AddCircle(point,radius,color,16,1.5f);
    draw->AddLine({point.x-radius-3,point.y},{point.x+radius+3,point.y},color,1.5f);
    draw->AddLine({point.x,point.y-radius-3},{point.x,point.y+radius+3},color,1.5f);
}

void grid(ImDrawList* draw, ImVec2 begin, ImVec2 end, ImVec2 origin, float zoom, bool show) {
    draw->AddRectFilled(begin,end,IM_COL32(12,18,29,255),6);
    if (!show) return;
    const float size = zoom >= 1.0f ? zoom * 8 : zoom*32;
    const float x0 = begin.x + std::fmod(std::fmod(origin.x-begin.x,size)+size,size) - size;
    const float y0 = begin.y + std::fmod(std::fmod(origin.y-begin.y,size)+size,size) - size;
    for (float y = y0; y < end.y; y += size) {
        int index = 0;
        for (float x = x0; x < end.x; x += size, ++index) {
            const int row = int(std::round((y-y0)/size));
            if ((index + row) % 2 == 0) draw->AddRectFilled({x,y},{x+size,y+size},IM_COL32(17,25,38,255));
        }
    }
}

fs::path dialogPath(HWND window, bool folder, bool saving, bool imageFile = false, bool modelFile = false) {
    ComPtr<IFileDialog> dialog;
    const CLSID clsid = saving ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
    if (FAILED(CoCreateInstance(clsid,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
        throw std::runtime_error("Windows could not open the file dialog.");
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : 0));
    const COMDLG_FILTERSPEC project[] = {{L"OpenBOR Studio project",L"*.borstudio"}};
    const COMDLG_FILTERSPEC images[] = {{L"Image files",L"*.png;*.gif;*.bmp;*.jpg;*.jpeg;*.tif;*.tiff;*.webp"},{L"All files",L"*.*"}};
    const COMDLG_FILTERSPEC models[] = {{L"OpenBOR entity model",L"*.txt"}};
    if (!folder) {
        if (imageFile) dialog->SetFileTypes(2, images);
        else if (modelFile) dialog->SetFileTypes(1, models);
        else {
            dialog->SetFileTypes(1, project);
            dialog->SetDefaultExtension(L"borstudio");
        }
    }
    const HRESULT result = dialog->Show(window);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    if (FAILED(result)) throw std::runtime_error("The file dialog could not complete.");
    ComPtr<IShellItem> item;
    PWSTR path = nullptr;
    if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        throw std::runtime_error("Could not read the selected path.");
    }
    fs::path out(path);
    CoTaskMemFree(path);
    return out;
}

std::vector<fs::path> dialogImages(HWND window) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
        throw std::runtime_error("Windows could not open the image picker.");
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);
    const COMDLG_FILTERSPEC images[] = {{L"Image files",L"*.png;*.gif;*.bmp;*.jpg;*.jpeg;*.tif;*.tiff;*.webp"},{L"All files",L"*.*"}};
    dialog->SetFileTypes(2,images);
    const HRESULT result = dialog->Show(window);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    if (FAILED(result)) throw std::runtime_error("The image picker could not complete.");
    ComPtr<IShellItemArray> results;
    if (FAILED(dialog->GetResults(&results))) throw std::runtime_error("Could not read selected images.");
    DWORD count = 0;
    results->GetCount(&count);
    std::vector<fs::path> paths;
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        PWSTR path = nullptr;
        if (SUCCEEDED(results->GetItemAt(i,&item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) {
            paths.emplace_back(path);
            CoTaskMemFree(path);
        }
    }
    std::sort(paths.begin(),paths.end());
    return paths;
}

int clampIndex(int index, size_t count) {
    return std::clamp(index, 0, std::max(0,int(count)-1));
}

const Frame* previousFrame(const Animation& animation, size_t index) {
    if (index > 0) return &animation.frames[index - 1];
    if (animation.loop && animation.frames.size() > 1) return &animation.frames.back();
    return nullptr;
}

void zoomAt(float& zoom, ImVec2& pan, float wheel, ImVec2 origin, float low, float high) {
    const float old = zoom;
    zoom = std::clamp(zoom * std::pow(1.18f, wheel), low, high);
    const auto mouse = ImGui::GetIO().MousePos;
    const auto displacement = sub(mouse,origin);
    pan = add(pan,mul(displacement,1.0f-zoom/old));
}

float screenY(const SpawnCue& cue) { return float(cue.z - cue.altitude); }

} // namespace

void configureStyle() {
    ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle();
    s.WindowPadding = {13,11}; s.FramePadding = {8,5}; s.ItemSpacing = {8,7};
    s.WindowRounding = 0; s.ChildRounding = 7; s.PopupRounding = 8;
    s.FrameRounding = 5; s.GrabRounding = 4; s.ScrollbarRounding = 5;
    s.ChildBorderSize = 1; s.WindowBorderSize = 0; s.FrameBorderSize = 0;
    s.ScrollbarSize = 10;
    auto& c = s.Colors;
    c[ImGuiCol_Text] = {.89f,.93f,.97f,1};
    c[ImGuiCol_TextDisabled] = {.43f,.49f,.59f,1};
    c[ImGuiCol_WindowBg] = {.055f,.077f,.116f,1};
    c[ImGuiCol_ChildBg] = {.079f,.104f,.153f,1};
    c[ImGuiCol_PopupBg] = {.09f,.12f,.17f,1};
    c[ImGuiCol_Border] = {.18f,.23f,.31f,1};
    c[ImGuiCol_Button] = {.145f,.185f,.254f,1};
    c[ImGuiCol_ButtonHovered] = {.205f,.27f,.37f,1};
    c[ImGuiCol_ButtonActive] = {.27f,.35f,.46f,1};
    c[ImGuiCol_FrameBg] = {.105f,.14f,.20f,1};
    c[ImGuiCol_FrameBgHovered] = {.14f,.195f,.27f,1};
    c[ImGuiCol_FrameBgActive] = {.18f,.25f,.34f,1};
    c[ImGuiCol_Header] = {.17f,.24f,.34f,1};
    c[ImGuiCol_HeaderHovered] = {.19f,.29f,.42f,1};
    c[ImGuiCol_HeaderActive] = {.22f,.33f,.47f,1};
    c[ImGuiCol_Tab] = {.125f,.175f,.25f,1};
    c[ImGuiCol_TabHovered] = {.24f,.31f,.41f,1};
    c[ImGuiCol_TabSelected] = {.25f,.32f,.41f,1};
    c[ImGuiCol_TabSelectedOverline] = {.98f,.68f,.40f,1};
    c[ImGuiCol_CheckMark] = {.98f,.68f,.40f,1};
    c[ImGuiCol_SliderGrab] = {.98f,.68f,.40f,1};
    c[ImGuiCol_SliderGrabActive] = {1,.78f,.53f,1};
}

Editor::Editor(HWND window, ID3D11Device* device, const fs::path& sample)
    : window_(window), images_(device), sample_(sample) {
    executeReplace("New");
}

void Editor::setCaptureView(bool stage) {
    if (fs::is_regular_file(sample_)) replace("Demo");
    stageMode_ = stage;
}

void Editor::checkpoint() {
    if (undo_.size() >= 36) undo_.erase(undo_.begin());
    undo_.push_back(project_);
    redo_.clear();
    dirty_ = true;
}

void Editor::history(bool forward) {
    auto& source = forward ? redo_ : undo_;
    auto& target = forward ? undo_ : redo_;
    if (source.empty()) return;
    target.push_back(std::move(project_));
    project_ = std::move(source.back());
    source.pop_back();
    normalizeSelection();
    renameInput_ = project_.characters.empty() ? "" : project_.characters[size_t(model_)].id;
    resetPlayback();
    dirty_ = true;
}

void Editor::normalizeSelection() {
    model_ = clampIndex(model_,project_.characters.size());
    stage_ = clampIndex(stage_,project_.stages.size());
    if (!project_.characters.empty()) {
        const auto& model = project_.characters[size_t(model_)];
        animation_ = clampIndex(animation_,model.animations.size());
        if (!model.animations.empty()) frame_ = clampIndex(frame_,model.animations[size_t(animation_)].frames.size());
    }
    if (!project_.stages.empty()) {
        const auto& stage = project_.stages[size_t(stage_)];
        spawn_ = clampIndex(spawn_,stage.spawns.size());
        panel_ = clampIndex(panel_,stage.panels.size());
    }
}

void Editor::resetPlayback(bool play) {
    spriteDrag_.reset();
    playing_ = play;
    playbackSeconds_ = 0;
    cueAge_ = itemAge_ = -1.0f;
}

void Editor::setModel(int index) {
    model_ = index;
    animation_ = frame_ = 0;
    normalizeSelection();
    renameInput_ = project_.characters.empty() ? "" : project_.characters[size_t(model_)].id;
    resetPlayback();
}

void Editor::setAnimation(int index) {
    animation_ = index;
    frame_ = 0;
    resetPlayback();
}

void Editor::insertModel(Character model) {
    const auto base = model.id.empty() ? std::string("fighter") : model.id.substr(0,35);
    std::string id = base;
    for (int suffix = 2; findCharacter(project_,id); ++suffix) id = base + "_" + std::to_string(suffix);
    const auto old = model.id;
    model.id = id;
    for (auto& animation : model.animations) {
        if (animation.projectile.model == old && !old.empty()) animation.projectile.model = id;
        if (animation.item.model == old && !old.empty()) animation.item.model = id;
    }
    checkpoint();
    project_.characters.push_back(std::move(model));
    setModel(int(project_.characters.size())-1);
    stageMode_ = false;
}

void Editor::newModel() {
    Character model;
    model.animations.emplace_back();
    insertModel(std::move(model));
    status_ = "Add images to the Idle timeline.";
}

void Editor::addAnimation() {
    auto& animations = project_.characters[size_t(model_)].animations;
    const auto contains = [&](const std::string& name) {
        return std::any_of(animations.begin(), animations.end(), [&](const Animation& a) { return a.name == name; });
    };
    Animation animation;
    if (contains("idle")) {
        if (!contains("walk")) animation.name = "walk";
        else if (!contains("attack1")) animation.name = "attack1";
        else {
            int slot = 1;
            while (contains("freespecial"+std::to_string(slot))) ++slot;
            animation.name = "freespecial"+std::to_string(slot);
        }
    }
    animation.loop = animation.name == "idle" || animation.name == "walk";
    checkpoint();
    animations.push_back(std::move(animation));
    setAnimation(int(animations.size())-1);
}

void Editor::newStage() {
    checkpoint();
    project_.stages.emplace_back();
    stage_ = int(project_.stages.size())-1;
    spawn_ = panel_ = camera_ = 0;
    stageView_ = {1.0f,{}};
    status_ = "Add a panel image to define the stage.";
}

void Editor::load(const fs::path& path, bool asSample) {
    auto next = openProject(path);
    images_.clear();
    project_ = std::move(next);
    file_ = asSample ? fs::path{} : path;
    dirty_ = false;
    undo_.clear(); redo_.clear();
    model_ = animation_ = frame_ = stage_ = spawn_ = panel_ = camera_ = 0;
    characterView_ = {asSample ? 6.0f : 4.0f,{}};
    stageView_ = {asSample ? 1.45f : 1.0f,{}};
    stageMode_ = project_.characters.empty() && !project_.stages.empty();
    normalizeSelection();
    renameInput_ = project_.characters.empty() ? "" : project_.characters[size_t(model_)].id;
    resetPlayback();
    if (asSample && !project_.characters.empty() && !project_.characters[0].animations.empty()) {
        const auto& clip = project_.characters[0].animations[0];
        if (clip.projectile.enabled) {
            frame_ = clampIndex(clip.projectile.frame, clip.frames.size());
            selectSpawnTab_ = true;
        }
    }
    status_ = asSample ? "Demo project loaded. Save a copy to keep your changes." : "Opened " + utf8(path.filename());
}

void Editor::save(bool as) {
    auto output = file_;
    if (output.empty() || as) output = dialogPath(window_,false,true);
    if (output.empty()) return;
    saveProject(project_,output);
    project_ = openProject(output);
    images_.clear();
    file_ = output;
    dirty_ = false;
    status_ = "Saved " + utf8(output.filename());
}

void Editor::replace(std::string action) {
    if (dirty_) {
        pending_ = std::move(action);
        confirmReplace_ = true;
    } else {
        try { executeReplace(action); }
        catch (const std::exception& error) { status_ = error.what(); }
    }
}

void Editor::executeReplace(const std::string& action) {
    if (action == "New") {
        project_ = {};
        undo_.clear(); redo_.clear(); images_.clear();
        file_.clear();
        model_ = animation_ = frame_ = stage_ = spawn_ = panel_ = camera_ = 0;
        dirty_ = false;
        resetPlayback();
        status_ = "Empty project. Add a model or stage.";
    } else if (action == "Open") {
        const auto path = dialogPath(window_,false,false);
        if (!path.empty()) load(path,false);
    } else if (action == "Demo") load(sample_,true);
    else if (action == "Close") DestroyWindow(window_);
}

void Editor::addFrames() {
    if (project_.characters.empty()) return;
    const auto paths = dialogImages(window_);
    if (paths.empty()) return;
    std::vector<Frame> next;
    for (const auto& path : paths) next.push_back(makeFrame(path,images_));
    auto& model = project_.characters[size_t(model_)];
    if (model.animations.empty()) addAnimation();
    checkpoint();
    auto& animation = model.animations[size_t(animation_)];
    frame_ = int(animation.frames.size());
    animation.frames.insert(animation.frames.end(),std::make_move_iterator(next.begin()),std::make_move_iterator(next.end()));
    resetPlayback();
    status_ = "Added " + std::to_string(paths.size()) + " frames to " + animation.name + ".";
}

void Editor::addPanels() {
    if (project_.stages.empty()) return;
    const auto paths = dialogImages(window_);
    if (paths.empty()) return;
    auto& stage = project_.stages[size_t(stage_)];
    if (stage.panels.size() + paths.size() > 26) throw std::runtime_error("Stages support up to 26 panels.");
    for (const auto& path : paths) (void)images_.read(path);
    checkpoint();
    panel_ = int(stage.panels.size());
    stage.panels.insert(stage.panels.end(),paths.begin(),paths.end());
    status_ = "Panels added to " + stage.title + ".";
}

void Editor::importModel() {
    const auto path = dialogPath(window_,false,false,false,true);
    if (path.empty()) return;
    int skipped = 0;
    auto model = importCharacterFile(path,images_,skipped);
    insertModel(std::move(model));
    status_ = "Imported editable model settings. " + std::to_string(skipped) + " unsupported commands skipped.";
}

void Editor::tick(Animation& animation) {
    if (!playing_ || animation.frames.empty()) return;
    const float dt = std::min(ImGui::GetIO().DeltaTime, .1f) * playbackSpeed_;
    if (cueAge_ >= 0) cueAge_ += dt;
    if (itemAge_ >= 0) itemAge_ += dt;
    playbackSeconds_ += dt;
    const auto trigger = [&]() {
        if (animation.projectile.enabled && frame_ == animation.projectile.frame) cueAge_ = 0;
        if (animation.item.enabled && frame_ == animation.item.frame) itemAge_ = 0;
    };
    if (playbackSeconds_ == dt) trigger();
    for (int iterations = 0; iterations < 24; ++iterations) {
        const float duration = std::max(1,animation.frames[size_t(frame_)].ticks)/100.0f;
        if (playbackSeconds_ < duration) break;
        playbackSeconds_ -= duration;
        if (frame_+1 < int(animation.frames.size())) ++frame_;
        else if (animation.loop) {
            frame_ = 0;
            cueAge_ = itemAge_ = -1;
        } else {
            playing_ = false;
            break;
        }
        trigger();
    }
}

bool Editor::chooseModel(const char* labelText, std::string& id, bool emptyAllowed) {
    const auto model = findCharacter(project_,id);
    const char* display = model ? model->title.c_str() : id.empty() ? "Choose a model" : id.c_str();
    bool changed = false;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo(labelText,display)) {
        if (emptyAllowed && ImGui::Selectable("None",id.empty()) && !id.empty()) {
            checkpoint(); id.clear(); changed = true;
        }
        for (const auto& candidate : project_.characters) {
            ImGui::PushID(candidate.id.c_str());
            const auto label = candidate.title + "  [" + candidate.id + "]";
            if (ImGui::Selectable(label.c_str(),id == candidate.id) && id != candidate.id) {
                checkpoint(); id = candidate.id; changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

void Editor::previewText(const std::string& text) {
    textPreview_ = text;
    previewOpen_ = true;
}

void Editor::draw() {
    normalizeSelection();
    const auto& viewport = *ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport.WorkPos);
    ImGui::SetNextWindowSize(viewport.WorkSize);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("Studio",nullptr,flags);
    try {
        drawHeader();
        if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) {
            const auto& io = ImGui::GetIO();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false)) save(io.KeyShift);
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O,false)) replace("Open");
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N,false)) replace("New");
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z,false)) history(io.KeyShift);
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y,false)) history(true);
            if (ImGui::IsKeyPressed(ImGuiKey_F1,false)) stageMode_ = false;
            if (ImGui::IsKeyPressed(ImGuiKey_F2,false)) { stageMode_ = true; resetPlayback(); }
        }
        const auto area = ImGui::GetContentRegionAvail();
        const float left = std::clamp(area.x*.17f,181.0f,235.0f);
        const float right = std::clamp(area.x*.23f,260.0f,325.0f);
        const float height = std::max(340.0f,area.y-30);
        if (stageMode_) stageWorkspace(left,right,height);
        else characterWorkspace(left,right,height);
        drawStatus();
        drawModals();
    } catch (const std::exception& error) {
        status_ = error.what();
    }
    ImGui::End();
}

void Editor::drawHeader() {
    const auto pos = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos,add(pos,{36,36}),accent,8);
    draw->AddText(add(pos,{7,9}),IM_COL32(30,19,13,255),"OB");
    ImGui::Dummy({40,37});
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr,22.0f);
    ImGui::TextUnformatted("OpenBOR Studio");
    ImGui::PopFont();
    textDim(project_.title.c_str());
    ImGui::EndGroup();
    ImGui::SameLine(0,26);
    if (toggleButton("Characters",!stageMode_,{93,35})) { stageMode_ = false; }
    ImGui::SameLine();
    if (toggleButton("Stages",stageMode_,{72,35})) { stageMode_ = true; resetPlayback(); }
    ImGui::SameLine();
    const float toolbar = 350;
    if (ImGui::GetContentRegionAvail().x > toolbar) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - toolbar);
    }
    if (ImGui::Button("Project",{70,35})) ImGui::OpenPopup("Project menu");
    if (ImGui::BeginPopup("Project menu")) {
        if (ImGui::MenuItem("New project","Ctrl+N")) replace("New");
        if (ImGui::MenuItem("Open project","Ctrl+O")) replace("Open");
        if (ImGui::MenuItem("Save as","Ctrl+Shift+S")) {
            try { save(true); } catch (const std::exception& error) { status_ = error.what(); }
        }
        if (ImGui::MenuItem("Open demo",nullptr,false,fs::is_regular_file(sample_))) replace("Demo");
        ImGui::SetNextItemWidth(260);
        auto title = project_.title;
        if (ImGui::InputText("Project title",&title,ImGuiInputTextFlags_EnterReturnsTrue) && !title.empty()) {
            checkpoint();
            project_.title = title;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) replace("Close");
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(undo_.empty());
    if (ImGui::Button("Undo",{51,35})) history(false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redo_.empty());
    if (ImGui::Button("Redo",{51,35})) history(true);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(dirty_ ? "Save *" : "Save",{54,35})) save(false);
    ImGui::SameLine();
    if (filled("Export",{74,35})) {
        try {
            const auto parent = dialogPath(window_,true,false);
            if (!parent.empty()) status_ = "Exported to " + utf8(exportProject(project_,parent,images_));
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void Editor::drawStatus() {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(.64f,.73f,.82f,1),"%s",status_.c_str());
}

void Editor::drawModals() {
    if (confirmReplace_) {
        ImGui::OpenPopup("Unsaved project");
        confirmReplace_ = false;
    }
    if (ImGui::BeginPopupModal("Unsaved project",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save your edits before leaving this project?");
        if (filled("Save and continue")) {
            try {
                save(false);
                if (!dirty_) { ImGui::CloseCurrentPopup(); executeReplace(pending_); }
            } catch (const std::exception& error) { status_ = error.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard edits")) {
            ImGui::CloseCurrentPopup();
            try { executeReplace(pending_); }
            catch (const std::exception& error) { status_ = error.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (importDialog_) {
        ImGui::OpenPopup("Import sprite pack");
        importDialog_ = false;
    }
    if (ImGui::BeginPopupModal("Import sprite pack",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Choose a character folder from an extracted sprite pack.");
        ImGui::TextUnformatted("Pick the artwork facing that should appear as right in OpenBOR.");
        static const char* labels[] = {"Right","Left","Down","Up"};
        static const char* values[] = {"right","left","down","up"};
        ImGui::SetNextItemWidth(140);
        ImGui::Combo("Source facing",&importFacing_,labels,4);
        if (filled("Choose folder")) {
            try {
                const auto folder = dialogPath(window_,true,false);
                if (!folder.empty()) {
                    insertModel(importSpriteFolder(folder,values[importFacing_],images_));
                    status_ = "Imported animation groups and frame origins. Review clip names and timing.";
                    ImGui::CloseCurrentPopup();
                }
            } catch (const std::exception& error) { status_ = error.what(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        textDim(status_.c_str());
        ImGui::EndPopup();
    }
    if (previewOpen_) { ImGui::OpenPopup("OpenBOR text"); previewOpen_ = false; }
    ImGui::SetNextWindowSize({660,540},ImGuiCond_Appearing);
    bool show = true;
    if (ImGui::BeginPopupModal("OpenBOR text",&show)) {
        ImGui::InputTextMultiline("##Text",&textPreview_,{-1,ImGui::GetContentRegionAvail().y-39},ImGuiInputTextFlags_ReadOnly);
        if (filled("Copy text")) ImGui::SetClipboardText(textPreview_.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void Editor::characterWorkspace(float left, float right, float height) {
    child("Character library",{left,height});
    characterLibrary();
    ImGui::EndChild();
    ImGui::SameLine();
    const float center = ImGui::GetContentRegionAvail().x - right - ImGui::GetStyle().ItemSpacing.x;
    Character* model = project_.characters.empty() ? nullptr : &project_.characters[size_t(model_)];
    Animation* animation = model && !model->animations.empty() ? &model->animations[size_t(animation_)] : nullptr;
    ImGui::BeginGroup();
    child("Animation viewport",{center,height-154},ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    characterCanvas(animation,height-154);
    ImGui::EndChild();
    child("Animation timeline",{center,146},ImGuiWindowFlags_NoScrollbar);
    timeline(animation,146);
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::SameLine();
    child("Character inspector",{right,height});
    if (model) frameInspector(*model,animation);
    else {
        section("Inspector");
        textDim("Select or create a model.");
    }
    ImGui::EndChild();
}

void Editor::characterLibrary() {
    section("Models");
    if (filled("+ New model",{-1,31})) newModel();
    if (ImGui::Button("Import sprite pack",{-1,29})) importDialog_ = true;
    if (ImGui::Button("Import OpenBOR model",{-1,29})) {
        try { importModel(); } catch (const std::exception& error) { status_ = error.what(); }
    }
    ImGui::Spacing();
    for (size_t i = 0; i < project_.characters.size(); ++i) {
        const auto& model = project_.characters[i];
        ImGui::PushID(int(i));
        const auto start = ImGui::GetCursorScreenPos();
        const bool selected = model_ == int(i);
        if (ImGui::Selectable("##Model",selected,0,{0,42})) setModel(int(i));
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(add(start,{3,3}),add(start,{40,39}),IM_COL32(25,35,51,255),4);
        if (const auto frame = previewFrame(&model)) thumbnail(draw,images_.texture(frame->image),add(start,{5,4}),{33,33});
        if (selected) draw->AddRectFilled(start,add(start,{2,42}),accent,1);
        draw->AddText(add(start,{48,3}),ink,model.title.c_str());
        draw->AddText(add(start,{48,22}),dim,model.type.c_str());
        ImGui::PopID();
    }
    if (project_.characters.empty()) return;
    ImGui::Separator();
    section("Animations");
    if (ImGui::Button("+ Animation",{-1,27})) addAnimation();
    auto& model = project_.characters[size_t(model_)];
    for (size_t i = 0; i < model.animations.size(); ++i) {
        auto& animation = model.animations[i];
        ImGui::PushID(int(i));
        const std::string caption = animation.name + "  (" + std::to_string(animation.frames.size()) + ")";
        if (ImGui::Selectable(caption.c_str(),animation_ == int(i),0,{0,25})) setAnimation(int(i));
        ImGui::PopID();
    }
}

void Editor::drawCue(SpawnCue& cue, const char* title, bool projectile, Animation& animation) {
    ImGui::PushID(title);
    const bool old = cue.enabled;
    bool enabled = old;
    if (ImGui::Checkbox(title,&enabled)) {
        checkpoint();
        cue.enabled = enabled;
        if (enabled) {
            cue.frame = frame_;
            if (cue.model.empty()) {
                for (const auto& model : project_.characters) {
                    if (model.type == (projectile ? "projectile" : "item")) {
                        cue.model = model.id;
                        break;
                    }
                }
            }
        }
        resetPlayback();
    }
    if (cue.enabled) {
        ImGui::TextUnformatted("Spawn model");
        chooseModel("##Cue model",cue.model,false);
        ImGui::SetNextItemWidth(-89);
        if (ImGui::SliderInt("Frame",&cue.frame,0,std::max(0,int(animation.frames.size())-1))) dirty_ = true;
        if (ImGui::Button("Use current frame")) { checkpoint(); cue.frame = frame_; }
        ImGui::SameLine();
        if (ImGui::Button("Jump")) {
            frame_ = clampIndex(cue.frame,animation.frames.size());
            resetPlayback();
        }
        ImGui::TextUnformatted("Relative X / Z / altitude");
        int position[] = {cue.x,cue.z,cue.altitude};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragInt3("##Position",position,1.0f,-2048,2048)) {
            cue.x=position[0]; cue.z=position[1]; cue.altitude=position[2];
            dirty_ = true;
        }
        if (projectile) {
            ImGui::TextUnformatted("Velocity X / Z / Y");
            float velocity[] = {cue.velocityX,cue.velocityZ,cue.velocityY};
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat3("##Velocity",velocity,.05f,-50,50,"%.2f")) {
                cue.velocityX=velocity[0]; cue.velocityZ=velocity[1]; cue.velocityY=velocity[2];
                dirty_ = true;
            }
        }
        const auto mode = projectile ? EditTool::projectile : EditTool::item;
        if (toggleButton("Place in preview",tool_ == mode,{-1,25})) {
            tool_ = mode;
            frame_ = clampIndex(cue.frame,animation.frames.size());
            resetPlayback();
        }
    }
    ImGui::PopID();
}

void Editor::frameInspector(Character& model, Animation* animation) {
    section("Inspector");
    if (ImGui::BeginTabBar("Model settings")) {
        if (ImGui::BeginTabItem("Frame")) {
            if (animation && !animation->frames.empty()) {
                auto& frame = animation->frames[size_t(frame_)];
                ImGui::Text("%s  /  Frame %02d",animation->name.c_str(),frame_);
                textDim(utf8(frame.image.filename()).c_str());
                ImGui::SetNextItemWidth(-88);
                if (ImGui::DragInt("Delay (cs)",&frame.ticks,1,1,1000)) dirty_ = true;
                int offset[] = {frame.originX,frame.originY};
                ImGui::SetNextItemWidth(-88);
                if (ImGui::DragInt2("Origin",offset,1,-4096,4096)) {
                    frame.originX = offset[0]; frame.originY = offset[1]; dirty_ = true;
                }
                if (ImGui::Button("Apply delay to clip",{-1,26})) {
                    checkpoint();
                    for (auto& next : animation->frames) next.ticks = frame.ticks;
                }
                if (ImGui::Button("Apply origin to clip",{-1,26})) {
                    checkpoint();
                    for (auto& next : animation->frames) { next.originX = frame.originX; next.originY = frame.originY; }
                }
                ImGui::Separator();
                const auto* previous = previousFrame(*animation,size_t(frame_));
                ImGui::BeginDisabled(!previous);
                if (ImGui::Button("Copy previous boxes",{-1,26})) {
                    if (playing_) resetPlayback();
                    ImGui::OpenPopup("##Copy boxes");
                }
                ImGui::EndDisabled();
                if (!previous && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("This frame has no previous frame.");
                if (ImGui::BeginPopup("##Copy boxes")) {
                    if (previous) {
                        if (frame_ == 0) ImGui::TextDisabled("From last frame");
                        const bool body = ImGui::MenuItem("Body");
                        const bool attack = ImGui::MenuItem("Attack");
                        const bool both = ImGui::MenuItem("Both");
                        if (body || attack || both) {
                            checkpoint();
                            if (body || both) frame.body = previous->body;
                            if (attack || both) frame.attack = previous->attack;
                        }
                    } else ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();
                }
                const auto editBox = [&](const char* name, Box& box, bool attack) {
                    ImGui::PushID(name);
                    bool enabled = box.enabled;
                    if (ImGui::Checkbox(name,&enabled)) { checkpoint(); box.enabled = enabled; }
                    if (box.enabled) {
                        ImGui::TextUnformatted("X / Y / width / height");
                        int rectangle[] = {box.x,box.y,box.width,box.height};
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::DragInt4("##Box",rectangle,1,-4096,4096)) {
                            box.x=rectangle[0]; box.y=rectangle[1]; box.width=std::max(1,rectangle[2]);
                            box.height=std::max(1,rectangle[3]); dirty_ = true;
                        }
                        ImGui::SetNextItemWidth(-88);
                        if (ImGui::DragInt("Depth",&box.depth,1,0,250)) dirty_ = true;
                        if (attack) {
                            ImGui::SetNextItemWidth(-88);
                            if (ImGui::DragInt("Damage",&box.damage,1,0,99999)) dirty_ = true;
                        }
                    }
                    ImGui::PopID();
                };
                editBox("Body box",frame.body,false);
                editBox("Attack box",frame.attack,true);
            } else textDim("Add a frame to edit its timing and boxes.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Spawns",nullptr,selectSpawnTab_ ? ImGuiTabItemFlags_SetSelected : 0)) {
            if (animation && !animation->frames.empty()) {
                selectSpawnTab_ = false;
                drawCue(animation->projectile,"Projectile cue",true,*animation);
                ImGui::Separator();
                drawCue(animation->item,"Item spawn cue",false,*animation);
                ImGui::Spacing();
                ImGui::TextWrapped("X flips with facing. Altitude moves the marker up; Z moves it down.");
            } else textDim("Add a frame to place a spawn cue.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Model")) {
            ImGui::TextUnformatted("Display name");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##Title",&model.title)) dirty_ = true;
            ImGui::TextUnformatted("OpenBOR model ID");
            ImGui::SetNextItemWidth(-72);
            ImGui::InputText("##ID",&renameInput_);
            ImGui::SameLine();
            if (ImGui::Button("Apply")) {
                try {
                    checkpoint();
                    renameCharacter(project_,size_t(model_),renameInput_);
                    renameInput_ = model.id;
                } catch (const std::exception& error) { status_ = error.what(); }
            }
            ImGui::TextUnformatted("Type");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::BeginCombo("##Type",model.type.c_str())) {
                for (const char* type : {"player","enemy","npc","item","projectile","obstacle","none","shot"}) {
                    if (ImGui::Selectable(type,model.type == type) && model.type != type) {
                        checkpoint(); model.type = type;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SetNextItemWidth(-85);
            if (ImGui::DragInt("Health",&model.health,1,1,100000)) dirty_ = true;
            ImGui::SetNextItemWidth(-85);
            if (ImGui::DragFloat("Speed",&model.speed,.05f,0,50,"%.2f")) dirty_ = true;
            if (animation) {
                ImGui::Separator();
                section("Selected animation");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##Animation ID",&animation->name)) dirty_ = true;
                if (ImGui::Checkbox("Loop",&animation->loop)) dirty_ = true;
            }
            if (ImGui::Button("Preview model TXT",{-1,30})) previewText(modelText(model));
            if (animation && ImGui::Button("Remove animation",{-1,27})) {
                checkpoint();
                model.animations.erase(model.animations.begin()+animation_);
                setAnimation(clampIndex(animation_,model.animations.size()));
            }
            if (ImGui::Button("Remove model",{-1,27})) {
                checkpoint();
                const auto id = model.id;
                project_.characters.erase(project_.characters.begin()+model_);
                for (auto& other : project_.characters) {
                    for (auto& clip : other.animations) {
                        if (clip.projectile.model == id) clip.projectile = {};
                        if (clip.item.model == id) clip.item = {};
                    }
                }
                for (auto& stage : project_.stages) {
                    std::erase_if(stage.spawns,[&](const StageSpawn& spawn){return spawn.model == id;});
                    for (auto& spawn : stage.spawns) if (spawn.item == id) spawn.item.clear();
                }
                setModel(clampIndex(model_,project_.characters.size()));
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void Editor::characterCanvas(Animation* animation, float height) {
    (void)height;
    const bool available = animation && !animation->frames.empty();
    ImGui::TextUnformatted(available ? animation->name.c_str() : "Animation preview");
    ImGui::SameLine();
    textDim(available ? "Animation preview" : "");
    ImGui::SameLine();
    const float remain = ImGui::GetContentRegionAvail().x;
    if (remain > 191) ImGui::SetCursorPosX(ImGui::GetCursorPosX()+remain-191);
    if (ImGui::Button(facingLeft_ ? "Facing left" : "Facing right")) {
        spriteDrag_.reset();
        facingLeft_ = !facingLeft_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset view")) {
        spriteDrag_.reset();
        characterView_ = {4,{}};
    }
    ImGui::Separator();

    static const std::array<std::pair<const char*,EditTool>,6> tools{{
        {"Select",EditTool::select},{"Origin",EditTool::origin},{"Body",EditTool::body},
        {"Attack",EditTool::attack},{"Shot",EditTool::projectile},{"Item",EditTool::item}}};
    for (size_t i = 0; i < tools.size(); ++i) {
        if (i) ImGui::SameLine(0,5);
        const auto [name,mode] = tools[i];
        if (toggleButton(name,tool_==mode)) tool_ = mode;
    }
    if (ImGui::GetContentRegionAvail().x > 92) {
        ImGui::SameLine();
        ImGui::Checkbox("Grid",&showGrid_);
    }
    if (tool_ == EditTool::origin) {
        if (toggleButton("Place pivot",originMode_ == OriginMode::placePivot)) {
            originMode_ = OriginMode::placePivot;
        }
        labelTip("Click where the pivot belongs on this frame.");
        ImGui::SameLine(0,5);
        if (toggleButton("Drag sprite",originMode_ == OriginMode::dragSprite)) {
            originMode_ = OriginMode::dragSprite;
        }
        labelTip("Drag the character while the pivot stays fixed.");
    }
    const bool dragSprite = tool_ == EditTool::origin && originMode_ == OriginMode::dragSprite;
    if (!dragSprite || !available) spriteDrag_.reset();

    const auto start = ImGui::GetCursorScreenPos();
    const auto size = ImGui::GetContentRegionAvail();
    const ImVec2 end = add(start,size);
    const ImVec2 center = add(start,{size.x*.49f,size.y*.64f});
    auto& view = characterView_;
    auto origin = add(center,view.pan);
    ImGui::InvisibleButton("##Character canvas",size,ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const auto& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    if (!spriteDrag_) {
        if (hovered && io.MouseWheel != 0) zoomAt(view.zoom,view.pan,io.MouseWheel,origin,.5f,18.0f);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            view.pan = add(view.pan,io.MouseDelta);
        }
    }
    origin = add(center,view.pan);
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(start,end,true);
    grid(draw,start,end,origin,view.zoom,showGrid_);
    draw->AddLine({start.x,origin.y},{end.x,origin.y},guideColor);
    draw->AddLine({origin.x,start.y},{origin.x,end.y},guideColor);
    if (!available) {
        label(draw,add(start,{18,20}),"Add frames to preview an animation",ink);
        draw->PopClipRect();
        return;
    }

    tick(*animation);
    auto& frame = animation->frames[size_t(frame_)];
    const auto tex = images_.texture(frame.image);
    const auto topLeft = [&]() {
        const int pivot = facingLeft_ && tex ? tex->width-frame.originX : frame.originX;
        return sub(origin,{pivot*view.zoom,frame.originY*view.zoom});
    };
    const auto spriteCoord = [&](ImVec2 point) {
        auto p = mul(sub(point,topLeft()),1/view.zoom);
        if (facingLeft_ && tex) p.x = tex->width-p.x;
        return p;
    };
    const float sign = facingLeft_ ? -1.0f : 1.0f;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && tool_ != EditTool::select) {
        resetPlayback();
        if (tool_ == EditTool::projectile || tool_ == EditTool::item) {
            auto& cue = tool_ == EditTool::projectile ? animation->projectile : animation->item;
            checkpoint();
            const bool first = !cue.enabled;
            cue.enabled = true;
            if (first) {
                cue.frame = frame_;
                const char* type = tool_ == EditTool::projectile ? "projectile" : "item";
                const auto it = std::find_if(project_.characters.begin(),project_.characters.end(),
                    [&](const Character& c){return c.type == type;});
                if (it != project_.characters.end()) cue.model = it->id;
            }
            cue.x = int(std::lround((io.MousePos.x-origin.x)*sign/view.zoom));
            cue.altitude = cue.z-int(std::lround((io.MousePos.y-origin.y)/view.zoom));
        } else if (tex) {
            const ImVec2 p = spriteCoord(io.MousePos);
            if (dragSprite) {
                if (p.x >= 0 && p.x <= tex->width && p.y >= 0 && p.y <= tex->height) {
                    spriteDrag_ = SpriteDrag{io.MousePos,frame.originX,frame.originY};
                }
            } else if (tool_ == EditTool::origin) {
                checkpoint();
                frame.originX = int(std::lround(p.x));
                frame.originY = int(std::lround(p.y));
            } else {
                checkpoint();
                dragging_ = true;
                dragFrom_ = p;
                auto& box = tool_ == EditTool::body ? frame.body : frame.attack;
                box.enabled = true;
                box.x = int(std::lround(p.x)); box.y = int(std::lround(p.y));
                box.width = box.height = 1;
            }
        }
    }
    if (dragging_ && (tool_ == EditTool::body || tool_ == EditTool::attack) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const auto p = spriteCoord(io.MousePos);
        auto& box = tool_ == EditTool::body ? frame.body : frame.attack;
        box.x = int(std::lround(std::min(p.x,dragFrom_.x)));
        box.y = int(std::lround(std::min(p.y,dragFrom_.y)));
        box.width = std::max(1,int(std::lround(std::abs(p.x-dragFrom_.x))));
        box.height = std::max(1,int(std::lround(std::abs(p.y-dragFrom_.y))));
    }
    if (spriteDrag_) {
        auto& drag = *spriteDrag_;
        if (ImGui::IsMousePosValid()) {
            const auto delta = mul(sub(io.MousePos,drag.mouseStart),1.0f/view.zoom);
            const int x = drag.originX - int(std::lround(delta.x * sign));
            const int y = drag.originY - int(std::lround(delta.y));
            if (x != frame.originX || y != frame.originY) {
                if (!drag.changed) checkpoint();
                drag.changed = true;
                frame.originX = x;
                frame.originY = y;
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) spriteDrag_.reset();
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging_ = false;

    if (showOnionSkin_) {
        if (const auto* previous = previousFrame(*animation,size_t(frame_)))
            sprite(draw,images_,*previous,origin,view.zoom,facingLeft_,IM_COL32(183,171,255,115));
    }
    if (tex) sprite(draw,images_,frame,origin,view.zoom,facingLeft_);
    else label(draw,add(start,{18,20}),"Frame image unavailable",attackColor);
    const auto rect = [&](const Box& box, ImU32 color) {
        if (!box.enabled || !tex) return;
        const int x = facingLeft_ ? tex->width-box.x-box.width : box.x;
        const auto a = add(topLeft(),{x*view.zoom,box.y*view.zoom});
        const auto b = add(a,{box.width*view.zoom,box.height*view.zoom});
        draw->AddRectFilled(a,b,(color & 0xffffff) | 0x2c000000);
        draw->AddRect(a,b,color,0.0f,0,1.5f);
    };
    rect(frame.body,bodyColor);
    rect(frame.attack,attackColor);
    cross(draw,origin,accent,5);
    const auto drawSpawn = [&](const SpawnCue& cue, bool projectile, float age) {
        if (!cue.enabled) return;
        const auto color = projectile ? shotColor : itemColor;
        const bool current = cue.frame == frame_;
        const auto at = add(origin,{cue.x*sign*view.zoom,screenY(cue)*view.zoom});
        const auto ground = add(origin,{cue.x*sign*view.zoom,cue.z*view.zoom});
        draw->AddLine(origin,ground,(color&0xffffff)|0x58000000,1);
        draw->AddLine(ground,at,(color&0xffffff)|0x90000000,1);
        if (projectile) {
            const auto velocity = ImVec2{cue.velocityX*sign,cue.velocityZ-cue.velocityY};
            draw->AddLine(at,add(at,mul(velocity,view.zoom*13)),(color&0xffffff)|0x94000000,2);
        }
        if (showGhost_ && (current || age >= 0)) {
            const auto* child = previewFrame(findCharacter(project_,cue.model));
            if (child) {
                const float travel = projectile && age > 0 ? std::min(age,.75f)*45.0f : 0;
                const auto velocity = ImVec2{cue.velocityX*sign,cue.velocityZ-cue.velocityY};
                sprite(draw,images_,*child,add(at,mul(velocity,travel*view.zoom)),view.zoom,facingLeft_,
                       current ? IM_COL32(255,255,255,175) : IM_COL32(255,255,255,140));
            }
        }
        cross(draw,at,current ? color : (color&0xffffff)|0x85000000);
        const auto caption = std::string(projectile ? "Projectile" : "Item") + "  F" + std::to_string(cue.frame);
        label(draw,add(at,{11,projectile ? -24.0f : 10.0f}),caption.c_str(),color);
    };
    drawSpawn(animation->projectile,true,cueAge_);
    drawSpawn(animation->item,false,itemAge_);

    const auto scale = std::to_string(int(std::lround(view.zoom*100))) + "%";
    label(draw,{start.x+13,end.y-28},scale.c_str(),dim);
    const char* hint = dragSprite ? "Drag the sprite to the fixed pivot" :
        tool_==EditTool::origin ? "Click to place the pivot" :
        tool_==EditTool::body || tool_==EditTool::attack ? "Drag to draw a box" :
        tool_==EditTool::projectile || tool_==EditTool::item ? "Click to place the spawn point" :
        "Wheel zoom  |  Middle drag pan";
    label(draw,{end.x-ImGui::CalcTextSize(hint).x-13,end.y-28},hint,dim);
    draw->PopClipRect();
}

void Editor::timeline(Animation* animation, float height) {
    (void)height;
    const bool available = animation && !animation->frames.empty();
    const float toolbarRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    ImGui::BeginDisabled(project_.characters.empty());
    if (filled("+ Frames")) {
        try { addFrames(); } catch (const std::exception& error) { status_ = error.what(); }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!available);
    if (ImGui::Button("|<") && available) { frame_=0; resetPlayback(); }
    ImGui::SameLine(0,4);
    if (toggleButton(playing_ ? "Pause" : "Play",playing_,{63,0}) && available) {
        if (!playing_ && !animation->loop && frame_+1 == int(animation->frames.size())) frame_=0;
        resetPlayback(!playing_);
    }
    if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId) &&
        ImGui::IsKeyPressed(ImGuiKey_Space,false) && available) {
        if (!playing_ && !animation->loop && frame_+1 == int(animation->frames.size())) frame_=0;
        resetPlayback(!playing_);
    }
    ImGui::SameLine(0,4);
    if (ImGui::Button(">") && available) {
        frame_ = (frame_+1)%int(animation->frames.size());
        resetPlayback();
    }
    ImGui::SameLine(0,4);
    if (ImGui::Button("Delete") && available) {
        checkpoint();
        for (auto* cue : {&animation->projectile,&animation->item}) {
            if (cue->frame == frame_) cue->enabled = false;
            if (cue->frame > frame_) --cue->frame;
        }
        animation->frames.erase(animation->frames.begin()+frame_);
        frame_ = clampIndex(frame_,animation->frames.size());
        resetPlayback();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Onion skin",&showOnionSkin_);
    labelTip("Overlay the previous frame at its saved pivot. Looping clips wrap to the last frame.");
    ImGui::SameLine();
    ImGui::Checkbox("Spawn ghost",&showGhost_);
    if (toolbarRight - ImGui::GetItemRectMax().x > 100) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(91);
        ImGui::DragFloat("##Playback speed",&playbackSpeed_,.025f,.1f,3.0f,"%.2fx",ImGuiSliderFlags_AlwaysClamp);
        labelTip("Playback speed");
    }
    if (!animation || animation->frames.empty()) {
        ImGui::Spacing();
        textDim("Import a sequence of images to fill the timeline.");
        return;
    }
    ImGui::BeginChild("Filmstrip",{-1,-1},ImGuiChildFlags_None,ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < int(animation->frames.size()); ++i) {
        if (i) ImGui::SameLine(0,6);
        ImGui::PushID(i);
        const auto start = ImGui::GetCursorScreenPos();
        const auto& frame = animation->frames[size_t(i)];
        ImGui::InvisibleButton("##Frame",{66,75});
        if (ImGui::IsItemClicked()) { frame_ = i; resetPlayback(); }
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("FRAME_INDEX",&i,sizeof(i));
            ImGui::Text("Move frame %d",i);
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto* payload = ImGui::AcceptDragDropPayload("FRAME_INDEX")) {
                const int old = *static_cast<const int*>(payload->Data);
                if (old != i && old >= 0 && old < int(animation->frames.size())) {
                    checkpoint();
                    auto& frames = animation->frames;
                    if (old < i) std::rotate(frames.begin()+old,frames.begin()+old+1,frames.begin()+i+1);
                    else std::rotate(frames.begin()+i,frames.begin()+old,frames.begin()+old+1);
                    for (auto* cue : {&animation->projectile,&animation->item}) {
                        if (cue->frame == old) cue->frame = i;
                        else if (old < i && cue->frame > old && cue->frame <= i) --cue->frame;
                        else if (i < old && cue->frame >= i && cue->frame < old) ++cue->frame;
                    }
                    frame_ = i;
                    resetPlayback();
                }
            }
            ImGui::EndDragDropTarget();
        }
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(start,add(start,{66,75}),i==frame_ ? IM_COL32(40,53,72,255) : IM_COL32(21,30,43,255),5);
        draw->AddRect(start,add(start,{66,75}),i==frame_ ? accent : IM_COL32(52,68,90,255),5.0f,0,1.4f);
        thumbnail(draw,images_.texture(frame.image),add(start,{6,4}),{54,49});
        const auto timing = std::to_string(i)+"  ·  "+std::to_string(frame.ticks)+" cs";
        draw->AddText(add(start,{6,55}),i==frame_ ? ink : dim,timing.c_str());
        if (animation->projectile.enabled && animation->projectile.frame == i)
            draw->AddCircleFilled(add(start,{56,9}),3.4f,shotColor);
        if (animation->item.enabled && animation->item.frame == i)
            draw->AddCircleFilled(add(start,{46,9}),3.4f,itemColor);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void Editor::stageWorkspace(float left, float right, float height) {
    child("Stage library",{left,height});
    stageLibrary();
    ImGui::EndChild();
    ImGui::SameLine();
    const float center = ImGui::GetContentRegionAvail().x - right - ImGui::GetStyle().ItemSpacing.x;
    Stage* stage = project_.stages.empty() ? nullptr : &project_.stages[size_t(stage_)];
    ImGui::BeginGroup();
    child("Stage viewport",{center,height-154},ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    stageCanvas(stage,height-154);
    ImGui::EndChild();
    child("Stage events",{center,146},ImGuiWindowFlags_NoScrollbar);
    stageEvents(stage,146);
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::SameLine();
    child("Stage inspector",{right,height});
    stageInspector(stage);
    ImGui::EndChild();
}

void Editor::stageLibrary() {
    section("Stages");
    if (filled("+ New stage",{-1,31})) newStage();
    for (size_t i = 0; i < project_.stages.size(); ++i) {
        const auto& stage = project_.stages[i];
        ImGui::PushID(int(i));
        if (ImGui::Selectable(stage.title.c_str(),stage_==int(i),0,{0,28})) {
            stage_ = int(i);
            camera_ = spawn_ = panel_ = 0;
            stageView_ = {1,{}};
            placeSpawn_ = false;
        }
        ImGui::PopID();
    }
    if (project_.stages.empty()) return;
    auto& stage = project_.stages[size_t(stage_)];
    ImGui::Separator();
    section("Artwork");
    if (ImGui::Button(stage.background.empty() ? "Choose background" : "Replace background",{-1,29})) {
        try {
            const auto path = dialogPath(window_,false,false,true);
            if (!path.empty()) {
                (void)images_.read(path);
                checkpoint(); stage.background = path;
            }
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    if (!stage.background.empty()) {
        ImGui::TextWrapped("%s",utf8(stage.background.filename()).c_str());
        if (ImGui::SmallButton("Clear background")) { checkpoint(); stage.background.clear(); }
    }
    if (ImGui::Button("+ Add panels",{-1,29})) {
        try { addPanels(); } catch (const std::exception& error) { status_ = error.what(); }
    }
    for (size_t i = 0; i < stage.panels.size(); ++i) {
        ImGui::PushID(int(i));
        const auto label = std::string(1,char('A'+i))+"  "+utf8(stage.panels[i].filename());
        if (ImGui::Selectable(label.c_str(),panel_==int(i),0,{0,25})) panel_ = int(i);
        ImGui::PopID();
    }
    if (!stage.panels.empty()) {
        ImGui::BeginDisabled(panel_ == 0);
        if (ImGui::Button("Earlier")) {
            checkpoint();
            std::swap(stage.panels[size_t(panel_)],stage.panels[size_t(panel_-1)]);
            --panel_;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(panel_+1 == int(stage.panels.size()));
        if (ImGui::Button("Later")) {
            checkpoint();
            std::swap(stage.panels[size_t(panel_)],stage.panels[size_t(panel_+1)]);
            ++panel_;
        }
        ImGui::EndDisabled();
        if (ImGui::Button("Remove panel",{-1,26})) {
            checkpoint();
            stage.panels.erase(stage.panels.begin()+panel_);
            panel_ = clampIndex(panel_,stage.panels.size());
        }
    }
}

void Editor::stageCanvas(Stage* stage, float height) {
    (void)height;
    ImGui::TextUnformatted(stage ? stage->title.c_str() : "Stage preview");
    ImGui::SameLine();
    if (ImGui::Button("Reset view##Stage")) stageView_ = {1,{}};
    ImGui::SameLine();
    ImGui::Checkbox("Grid##Stage",&showGrid_);
    ImGui::Separator();

    int stageWidth = 0;
    if (stage) {
        for (const auto& path : stage->panels) if (const auto tex = images_.texture(path)) stageWidth += tex->width;
    }
    const int maxCamera = std::max(0,stageWidth-320);
    camera_ = std::clamp(camera_,0,maxCamera);
    ImGui::SetNextItemWidth(-142);
    ImGui::SliderInt("Camera X",&camera_,0,maxCamera);
    ImGui::SameLine();
    if (toggleButton("Place",placeSpawn_) && stage && !stage->spawns.empty()) placeSpawn_ = !placeSpawn_;
    if (stage && stage->spawns.empty()) placeSpawn_ = false;
    labelTip("Click the canvas to position the selected spawn.");

    const auto start = ImGui::GetCursorScreenPos();
    const auto size = ImGui::GetContentRegionAvail();
    const auto end = add(start,size);
    auto& view = stageView_;
    const auto canvasOrigin = [&]() {
        return add(start,{size.x*.5f - float(camera_+160)*view.zoom + view.pan.x,
                          size.y*.5f - 120.0f*view.zoom + view.pan.y});
    };
    ImGui::InvisibleButton("##Stage canvas",size,ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const auto& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    if (hovered && io.MouseWheel != 0) zoomAt(view.zoom,view.pan,io.MouseWheel,canvasOrigin(),.25f,5.0f);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) view.pan = add(view.pan,io.MouseDelta);
    const auto origin = canvasOrigin();
    const auto point = [&](float x,float y) { return add(origin,{x*view.zoom,y*view.zoom}); };
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(start,end,true);
    grid(draw,start,end,origin,view.zoom,showGrid_);
    if (!stage) {
        label(draw,add(start,{18,20}),"Create a stage to place panels and spawns",ink);
        draw->PopClipRect();
        return;
    }
    if (stageWidth == 0) {
        label(draw,add(start,{18,20}),"Add a panel image to define the stage width",dim);
    }

    const int width = std::max(320,stageWidth);
    if (const auto* back = images_.texture(stage->background)) {
        const int first = std::max(0,int(std::floor((start.x-origin.x)/view.zoom/back->width)));
        const int last = std::min(width/back->width,int(std::ceil((end.x-origin.x)/view.zoom/back->width)));
        for (int i = first; i <= last; ++i) {
            const float left = float(i*back->width);
            const float right = std::min(float(width),left+back->width);
            if (right <= left) continue;
            draw->AddImage(ImTextureID(reinterpret_cast<uintptr_t>(back->view.Get())),point(left,0),
                           point(right,float(back->height)),{0,0},{(right-left)/back->width,1});
        }
    }
    int left = 0;
    for (size_t i = 0; i < stage->panels.size(); ++i) {
        const auto tex = images_.texture(stage->panels[i]);
        if (!tex) continue;
        const auto a = point(float(left),0), b = point(float(left+tex->width),float(tex->height));
        image(draw,tex,a,b);
        draw->AddRect(a,b,int(i)==panel_ ? (accent&0xffffff)|0xa5000000 : guideColor);
        const auto id = std::string("Panel ")+char('A'+i);
        label(draw,add(a,{7,8}),id.c_str(),dim);
        left += tex->width;
    }
    draw->AddRectFilled(point(0,float(stage->laneTop)),point(float(width),float(stage->laneBottom)),IM_COL32(85,195,151,29));
    for (const int lane : {stage->laneTop,stage->laneBottom}) {
        draw->AddLine(point(0,float(lane)),point(float(width),float(lane)),(bodyColor&0xffffff)|0x8a000000);
    }
    draw->AddRect(point(float(camera_),0),point(float(camera_+320),240),shotColor,0.0f,0,1.3f);
    label(draw,add(point(float(camera_),240),{6,8}),"320 x 240 camera",shotColor);
    const auto player = point(float(stage->playerX),float(stage->playerZ));
    cross(draw,player,bodyColor,5);
    label(draw,add(player,{8,9}),"Player start",bodyColor);

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !stage->spawns.empty()) {
        int target = -1;
        for (int i = int(stage->spawns.size())-1; i >= 0; --i) {
            const auto& entry = stage->spawns[size_t(i)];
            const auto marker = point(float(entry.at+entry.x),float(entry.z-entry.altitude));
            const auto d = sub(marker,io.MousePos);
            if (std::abs(d.x)<=12 && std::abs(d.y)<=12) { target = i; break; }
        }
        if (target >= 0 || placeSpawn_) {
            if (target >= 0) spawn_ = target;
            checkpoint();
            dragSpawn_ = spawn_;
        }
    }
    if (dragSpawn_ >= 0 && dragSpawn_ < int(stage->spawns.size()) && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto& moving = stage->spawns[size_t(dragSpawn_)];
        const auto world = mul(sub(io.MousePos,origin),1/view.zoom);
        moving.x = int(std::lround(world.x)) - moving.at;
        moving.z = int(std::lround(world.y)) + moving.altitude;
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragSpawn_ = -1;

    for (const auto& wave : stage->waves) {
        if (!wave.wait) continue;
        draw->AddLine(point(float(wave.at),0),point(float(wave.at),240),itemColor,1.3f);
        label(draw,add(point(float(wave.at),240),{6,26}),"Wait",itemColor);
    }
    for (size_t i = 0; i < stage->spawns.size(); ++i) {
        const auto& entry = stage->spawns[i];
        const auto base = point(float(entry.at+entry.x),float(entry.z));
        const auto top = point(float(entry.at+entry.x),float(entry.z-entry.altitude));
        if (entry.altitude) draw->AddLine(base,top,guideColor,1.5f);
        const auto model = findCharacter(project_,entry.model);
        if (const auto frame = previewFrame(model)) sprite(draw,images_,*frame,top,view.zoom,!entry.flip);
        cross(draw,top,i==size_t(spawn_) ? accent : shotColor,5);
        const std::string name = model ? model->title : entry.model.empty() ? "Unassigned" : entry.model;
        if (i==size_t(spawn_)) {
            draw->AddLine(point(float(entry.at),0),point(float(entry.at),240),(accent&0xffffff)|0x85000000,1.4f);
        }
        label(draw,add(top,{8,8}),name.c_str(),i==size_t(spawn_) ? accent : ink);
    }
    const auto scale = std::to_string(stageWidth)+" px  ·  "+std::to_string(int(std::lround(view.zoom*100)))+"%";
    label(draw,{start.x+13,end.y-28},scale.c_str(),dim);
    draw->PopClipRect();
}

void Editor::stageEvents(Stage* stage, float height) {
    (void)height;
    ImGui::BeginDisabled(!stage);
    if (filled("+ Spawn") && stage) {
        checkpoint();
        auto& entry = stage->spawns.emplace_back();
        entry.at = camera_;
        entry.z = (stage->laneTop+stage->laneBottom)/2;
        for (const auto& model : project_.characters) {
            if (model.type == "enemy") { entry.model = model.id; break; }
        }
        if (entry.model.empty() && !project_.characters.empty()) entry.model = project_.characters.front().id;
        spawn_ = int(stage->spawns.size())-1;
        placeSpawn_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Wait at camera") && stage) {
        checkpoint();
        stage->waves.push_back({camera_,true});
    }
    ImGui::EndDisabled();
    if (!stage) { textDim("Create a stage to add spawn events."); return; }
    ImGui::BeginChild("Spawn list",{-1,-1},ImGuiChildFlags_None);
    for (size_t i = 0; i < stage->spawns.size(); ++i) {
        const auto& spawn = stage->spawns[i];
        ImGui::PushID(int(i));
        const auto model = findCharacter(project_,spawn.model);
        const auto name = model ? model->title : spawn.model.empty() ? "Unassigned model" : spawn.model;
        const auto entry = "AT " + std::to_string(spawn.at) + "     " + name
                         + "     (" + std::to_string(spawn.x) + ", " + std::to_string(spawn.z) + ")";
        if (ImGui::Selectable(entry.c_str(),spawn_==int(i),0,{0,24})) {
            spawn_ = int(i);
            camera_ = spawn.at;
        }
        ImGui::PopID();
    }
    for (size_t i = 0; i < stage->waves.size(); ++i) {
        ImGui::PushID(int(i));
        ImGui::TextColored({.98f,.78f,.44f,1},"WAIT   AT %d",stage->waves[i].at);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove##wait")) {
            checkpoint();
            stage->waves.erase(stage->waves.begin()+ptrdiff_t(i));
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void Editor::stageInspector(Stage* stage) {
    section("Inspector");
    if (!stage) { textDim("Select or create a stage."); return; }
    if (!ImGui::BeginTabBar("Stage settings")) return;
    if (ImGui::BeginTabItem("Spawn")) {
        if (!stage->spawns.empty()) {
            auto& spawn = stage->spawns[size_t(spawn_)];
            ImGui::Text("Spawn %d of %d",spawn_+1,int(stage->spawns.size()));
            chooseModel("##Stage spawn model",spawn.model,false);
            ImGui::SetNextItemWidth(-100);
            if (ImGui::DragInt("Trigger at",&spawn.at,1,0,999999)) dirty_ = true;
            int position[] = {spawn.x,spawn.z,spawn.altitude};
            ImGui::TextUnformatted("Screen X / Z / altitude");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragInt3("##Coordinates",position,1,-99999,99999)) {
                spawn.x=position[0]; spawn.z=position[1]; spawn.altitude=position[2]; dirty_ = true;
            }
            ImGui::TextWrapped("Preview world X = %d",spawn.at+spawn.x);
            if (ImGui::Checkbox("Flip facing",&spawn.flip)) dirty_ = true;
            ImGui::SameLine();
            if (ImGui::Checkbox("Boss",&spawn.boss)) dirty_ = true;
            ImGui::TextUnformatted("Carried item");
            chooseModel("##Carried item",spawn.item,true);
            if (ImGui::Button("Move camera to trigger",{-1,27})) camera_ = spawn.at;
            if (ImGui::Button("Remove spawn",{-1,27})) {
                checkpoint();
                stage->spawns.erase(stage->spawns.begin()+spawn_);
                spawn_ = clampIndex(spawn_,stage->spawns.size());
            }
        } else textDim("Add a spawn to set its model and position.");
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Stage")) {
        ImGui::TextUnformatted("Stage title");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Stage title",&stage->title)) dirty_ = true;
        int lane[] = {stage->laneTop,stage->laneBottom};
        ImGui::TextUnformatted("Walkable Z range");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragInt2("##Walkable lane",lane,1,0,239)) {
            stage->laneTop=std::clamp(lane[0],0,239);
            stage->laneBottom=std::clamp(lane[1],stage->laneTop,239);
            stage->playerZ=std::clamp(stage->playerZ,stage->laneTop,stage->laneBottom);
            dirty_ = true;
        }
        int player[] = {stage->playerX,stage->playerZ};
        ImGui::TextUnformatted("Player start X / Z");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragInt2("##Player start",player,1,0,319)) {
            stage->playerX=std::clamp(player[0],0,319);
            stage->playerZ=std::clamp(player[1],stage->laneTop,stage->laneBottom);
            dirty_ = true;
        }
        if (ImGui::Button("Preview stage TXT",{-1,29})) previewText(stageText(*stage,size_t(stage_)));
        if (ImGui::Button("Remove stage",{-1,27})) {
            checkpoint();
            project_.stages.erase(project_.stages.begin()+stage_);
            stage_ = clampIndex(stage_,project_.stages.size());
            camera_ = spawn_ = panel_ = 0;
        }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

} // namespace studio

#include "ui.hpp"
#include "imgui_stdlib.h"
#include "imgui_impl_dx11.h"
#include "media.hpp"
#include <shobjidl.h>
#include <shellapi.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <type_traits>

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
constexpr float inspectorGripWidth = 9;
constexpr auto inspectorTabs = ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton;

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

template<class T>
struct NumberField {
    T& value;
    T minimum;
    T maximum;
};

template<class T>
T stepValue(T value, int direction, T minimum, T maximum) {
    if (direction > 0 && value < maximum) return std::min(T(value + 1),maximum);
    if (direction < 0 && value > minimum) return std::max(T(value - 1),minimum);
    return value;
}

bool stepArrow(ImGuiDir direction, ImVec2 size, bool enabled) {
    ImGui::BeginDisabled(!enabled);
    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat,true);
    const bool pressed = ImGui::InvisibleButton(direction == ImGuiDir_Up ? "Increase" : "Decrease",
                                               size,ImGuiButtonFlags_EnableNav);
    ImGui::PopItemFlag();
    if (ImGui::IsItemVisible()) {
        const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        const auto center = mul(add(a,b),.5f);
        auto* draw = ImGui::GetWindowDrawList();
        const auto color = ImGui::IsItemActive() ? ImGuiCol_ButtonActive :
                           ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered : ImGuiCol_Button;
        draw->AddRectFilled(a,b,ImGui::GetColorU32(color),3);
        const float tip = direction == ImGuiDir_Up ? -2.5f : 2.5f;
        draw->AddTriangleFilled(add(center,{-3.5f,-tip}),add(center,{3.5f,-tip}),
                                add(center,{0,tip}),ImGui::GetColorU32(ImGuiCol_Text));
    }
    ImGui::EndDisabled();
    return pressed;
}

template<class T>
bool numberFields(const char* label, std::initializer_list<NumberField<T>> fields,
                  float dragSpeed = 1, const char* format = nullptr, bool slider = false) {
    constexpr auto dataType = std::is_same_v<T,int> ? ImGuiDataType_S32 : ImGuiDataType_Float;
    constexpr float arrowWidth = 16, arrowGap = 1;
    const auto spacing = ImGui::GetStyle().ItemSpacing.x;
    const auto count = float(fields.size());
    const auto width = (ImGui::CalcItemWidth() - spacing*(count-1)) / count;
    const auto height = ImGui::GetFrameHeight();
    const auto halfHeight = std::floor((height-arrowGap)*.5f);
    bool changed = false;
    ImGui::PushID(label);
    ImGui::BeginGroup();
    int index = 0;
    for (const auto& field : fields) {
        if (index) ImGui::SameLine();
        ImGui::PushID(index++);
        ImGui::SetNextItemWidth(std::max(1.0f,width-arrowWidth-arrowGap));
        if (slider) {
            changed |= ImGui::SliderScalar("##Value",dataType,&field.value,&field.minimum,&field.maximum,format);
        } else {
            changed |= ImGui::DragScalar("##Value",dataType,&field.value,dragSpeed,
                                         &field.minimum,&field.maximum,format);
        }
        ImGui::SameLine(0,arrowGap);
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{spacing,arrowGap});
        if (stepArrow(ImGuiDir_Up,{arrowWidth,halfHeight},field.value < field.maximum)) {
            field.value = stepValue(field.value,1,field.minimum,field.maximum);
            changed = true;
        }
        if (stepArrow(ImGuiDir_Down,{arrowWidth,height-halfHeight-arrowGap},field.value > field.minimum)) {
            field.value = stepValue(field.value,-1,field.minimum,field.maximum);
            changed = true;
        }
        ImGui::PopStyleVar();
        ImGui::EndGroup();
        ImGui::PopID();
    }
    if (label[0] != '#') {
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    }
    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}

std::array<int,2> viewerNudge(bool selected) {
    const auto& io = ImGui::GetIO();
    if (!selected || !ImGui::IsWindowFocused() || ImGui::IsAnyItemActive() ||
        io.WantTextInput || io.KeyCtrl || io.KeyAlt || io.KeySuper ||
        ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) return {};
    constexpr auto repeat = ImGuiInputFlags_Repeat;
    const bool right = ImGui::Shortcut(ImGuiKey_RightArrow,repeat);
    const bool left = ImGui::Shortcut(ImGuiKey_LeftArrow,repeat);
    const bool down = ImGui::Shortcut(ImGuiKey_DownArrow,repeat);
    const bool up = ImGui::Shortcut(ImGuiKey_UpArrow,repeat);
    return {int(right)-int(left),int(down)-int(up)};
}

template<class BeforeMove>
void nudgePosition(NumberField<int> x, NumberField<int> y, int dx, int dy, BeforeMove beforeMove) {
    const auto nextX = stepValue(x.value,dx,x.minimum,x.maximum);
    const auto nextY = stepValue(y.value,dy,y.minimum,y.maximum);
    if (nextX == x.value && nextY == y.value) return;
    beforeMove();
    x.value = nextX;
    y.value = nextY;
}

bool nearMarker(ImVec2 mouse, ImVec2 marker, float radius) {
    const auto delta = sub(mouse,marker);
    return std::abs(delta.x) <= radius && std::abs(delta.y) <= radius;
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

void effectedSprite(ImDrawList* draw, const Texture& tex, const Frame& frame,
                   ImVec2 origin, float zoom, bool left) {
    const auto& effect = frame.effects;
    const bool flipX = left != (effect.enabled && effect.flipX);
    const bool flipY = effect.enabled && effect.flipY;
    const float px = float(flipX ? tex.width-frame.originX : frame.originX);
    const float py = float(flipY ? tex.height-frame.originY : frame.originY);
    const float sx = zoom * (effect.enabled ? effect.scaleX/256.0f : 1);
    const float sy = zoom * (effect.enabled ? effect.scaleY/256.0f : 1);
    const float radians = effect.enabled ? float(effect.rotation)*std::numbers::pi_v<float>/180 : 0;
    const float c = std::cos(radians), s = std::sin(radians);
    const auto point = [&](float x,float y) {
        x = (x-px)*sx;
        y = (y-py)*sy;
        return add(origin,{x*c-y*s,x*s+y*c});
    };
    const float u = flipX ? 1.0f : 0.0f, v = flipY ? 1.0f : 0.0f;
    draw->AddImageQuad(ImTextureID(reinterpret_cast<uintptr_t>(tex.view.Get())),
                       point(0,0),point(float(tex.width),0),point(float(tex.width),float(tex.height)),
                       point(0,float(tex.height)),{u,v},{1-u,v},{1-u,1-v},{u,1-v});
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

fs::path dialogPath(HWND window, bool folder, bool saving, bool imageFile = false, bool modelFile = false,
                    const COMDLG_FILTERSPEC* filter = nullptr, const wchar_t* title = nullptr) {
    ComPtr<IFileDialog> dialog;
    const CLSID clsid = saving ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
    if (FAILED(CoCreateInstance(clsid,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
        throw std::runtime_error("Windows could not open the file dialog.");
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : 0));
    if (title) dialog->SetTitle(title);
    const COMDLG_FILTERSPEC project[] = {{L"OpenBOR Studio project",L"*.borstudio"}};
    const COMDLG_FILTERSPEC images[] = {{L"Image files",L"*.png;*.gif;*.bmp;*.jpg;*.jpeg;*.tif;*.tiff;*.webp"},{L"All files",L"*.*"}};
    const COMDLG_FILTERSPEC models[] = {{L"OpenBOR entity model",L"*.txt"}};
    if (!folder) {
        if (filter) dialog->SetFileTypes(1,filter);
        else if (imageFile) dialog->SetFileTypes(2, images);
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
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxAnisotropy = 1;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    if (device && FAILED(device->CreateSamplerState(&sampler,&stageBackgroundSampler_)))
        throw std::runtime_error("Could not create the stage background sampler.");
    executeReplace("New");
}

void Editor::setCaptureView(bool stage) {
    if (fs::is_regular_file(sample_)) replace("Demo");
    stageMode_ = stage;
}

void Editor::initializeSession() {
    try { playtest_.loadEngine(); }
    catch (const std::exception& error) { status_ = error.what(); }
    try {
        recovery_.initialize();
        recoveryOffer_ = recovery_.latest();
        showRecovery_ = !recoveryOffer_.empty();
    } catch (const std::exception& error) { status_ = error.what(); }
}

void Editor::updateBackgroundTasks() {
    if (auto message = playtest_.update(); !message.empty()) {
        status_ = std::move(message);
        testError_ = status_.starts_with("Test play failed.") || status_.starts_with("OpenBOR test exited with code")
            ? status_ : std::string{};
    }
    if (auto message = recovery_.finish(); !message.empty()) status_ = std::move(message);
    const auto now = std::chrono::steady_clock::now();
    if (now < nextAutosave_) return;
    nextAutosave_ = now+Recovery::interval;
    if (!dirty_) return;
    try { recovery_.autosave(project_); }
    catch (const std::exception& error) { status_ = "Autosave failed. "+std::string(error.what()); }
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
    revealModel_ = revealAnimation_ = true;
    stageArtworkDrag_.reset();
    terrainDrag_.reset();
    terrainPlacement_.reset();
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
        terrain_ = clampIndex(terrain_,stage.terrain.size());
    }
}

void Editor::resetPlayback(bool play) {
    spriteDrag_.reset();
    playing_ = play;
    playbackSeconds_ = 0;
    cueAge_ = itemAge_ = -1.0f;
}

void Editor::stepFrame(const Animation& animation, int direction) {
    const int count = int(animation.frames.size());
    if (!count) return;
    const int next = frame_+direction;
    frame_ = animation.loop ? (next+count)%count : std::clamp(next,0,count-1);
    resetPlayback();
    revealFrame_ = true;
}

void Editor::setModel(int index) {
    model_ = index;
    animation_ = frame_ = 0;
    revealModel_ = revealAnimation_ = true;
    characterSelection_ = EditTool::select;
    paletteIndex_ = 0;
    paletteColor_ = 1;
    normalizeSelection();
    renameInput_ = project_.characters.empty() ? "" : project_.characters[size_t(model_)].id;
    resetPlayback();
}

void Editor::setAnimation(int index) {
    animation_ = index;
    frame_ = 0;
    revealAnimation_ = true;
    characterSelection_ = EditTool::select;
    resetPlayback();
}

void Editor::insertModel(Character model) {
    const auto base = model.id.empty() ? std::string("fighter") : model.id.substr(0,35);
    std::string id = base;
    for (int suffix = 2; findCharacter(project_,id); ++suffix) id = base + "_" + std::to_string(suffix);
    const auto old = model.id;
    model.id = id;
    for (auto& animation : model.animations) {
        if (animation.subEntity.model == old && !old.empty()) animation.subEntity.model = id;
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
        return std::any_of(animations.begin(), animations.end(),
                           [&](const Animation& a) { return animationId(a.name) == name; });
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
    stageSelection_ = StageSelection::spawn;
    stageCameraPreview_ = false;
    stageArtworkDrag_.reset();
    terrainDrag_.reset();
    terrainPlacement_.reset();
    terrain_ = 0;
    stageView_ = {1.0f,{}};
    status_ = "Add a panel image to define the stage.";
}

void Editor::load(const fs::path& path, bool asSample) {
    auto next = openProject(path);
    recovery_.clear();
    adoptProject(std::move(next),path,asSample);
    status_ = asSample ? "Demo project loaded. Save a copy to keep your changes." : "Opened " + utf8(path.filename());
}

void Editor::adoptProject(Project next, const fs::path& path, bool asSample) {
    images_.clear();
    glb_.reset();
    meshPreview_.reset();
    project_ = std::move(next);
    playableExport_ = !project_.nativeFiles.empty();
    nativeSelection_.clear();
    nativeDraft_.clear();
    nativeSaved_.clear();
    nativeFilter_.clear();
    nativeError_.clear();
    nativeOpen_ = false;
    file_ = asSample ? fs::path{} : path;
    dirty_ = false;
    nextAutosave_ = std::chrono::steady_clock::now()+Recovery::interval;
    undo_.clear(); redo_.clear();
    model_ = animation_ = frame_ = stage_ = spawn_ = panel_ = camera_ = 0;
    revealModel_ = revealAnimation_ = true;
    characterSelection_ = EditTool::select;
    stageSelection_ = StageSelection::spawn;
    stageCameraPreview_ = false;
    stageArtworkDrag_.reset();
    terrainDrag_.reset();
    terrainPlacement_.reset();
    terrain_ = 0;
    characterView_ = {asSample ? 6.0f : 4.0f,{}};
    stageView_ = {asSample ? 1.45f : 1.0f,{}};
    stageMode_ = project_.characters.empty() && !project_.stages.empty();
    moduleMode_ = false;
    paletteIndex_ = 0;
    paletteColor_ = 1;
    normalizeSelection();
    renameInput_ = project_.characters.empty() ? "" : project_.characters[size_t(model_)].id;
    resetPlayback();
    if (asSample && !project_.characters.empty() && !project_.characters[0].animations.empty()) {
        const auto& clip = project_.characters[0].animations[0];
        if (clip.subEntity.enabled) {
            frame_ = clampIndex(clip.subEntity.frame, clip.frames.size());
            selectSpawnTab_ = true;
        }
    }
}

void Editor::save(bool as) {
    auto output = file_;
    if (output.empty() || as) output = dialogPath(window_,false,true);
    if (output.empty()) return;
    saveProject(project_,output);
    project_ = openProject(output);
    images_.clear();
    glb_.reset();
    meshPreview_.reset();
    file_ = output;
    dirty_ = false;
    recovery_.clear();
    nextAutosave_ = std::chrono::steady_clock::now()+Recovery::interval;
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
        recovery_.clear();
        project_ = {};
        undo_.clear(); redo_.clear(); images_.clear();
        glb_.reset();
        meshPreview_.reset();
        file_.clear();
        model_ = animation_ = frame_ = stage_ = spawn_ = panel_ = camera_ = 0;
        revealModel_ = revealAnimation_ = true;
        characterSelection_ = EditTool::select;
        stageSelection_ = StageSelection::spawn;
        stageCameraPreview_ = false;
        stageArtworkDrag_.reset();
        terrainDrag_.reset();
        terrainPlacement_.reset();
        terrain_ = 0;
        dirty_ = false;
        nextAutosave_ = std::chrono::steady_clock::now()+Recovery::interval;
        moduleMode_ = false;
        resetPlayback();
        status_ = "Empty project. Add a model or stage.";
    } else if (action == "Open") {
        const auto path = dialogPath(window_,false,false);
        if (!path.empty()) load(path,false);
    } else if (action == "Open PAK" || action == "Open data") {
        fs::path folder;
        if (action == "Open PAK") {
            static const COMDLG_FILTERSPEC pak{L"OpenBOR PAK module",L"*.pak"};
            const auto file = dialogPath(window_,false,false,false,false,&pak);
            if (file.empty()) return;
            const auto parent = dialogPath(window_,true,false,false,false,nullptr,L"Choose where to unpack a working copy");
            if (parent.empty()) return;
            folder = unpackModule(file,parent);
        } else {
            folder = dialogPath(window_,true,false,false,false,nullptr,L"Choose the OpenBOR data or module folder");
            if (folder.empty()) return;
        }
        int skipped = 0;
        auto imported = importModule(folder,images_,skipped);
        recovery_.clear();
        adoptProject(std::move(imported),{},false);
        dirty_ = true;
        playableExport_ = true;
        status_ = "Opened "+std::to_string(project_.characters.size())+" models and "+
            std::to_string(project_.stages.size())+" stages. Use Native TXT to edit original files.";
        if (skipped) status_ += " "+std::to_string(skipped)+" entries available only in Native TXT.";
    } else if (action == "Demo") load(sample_,true);
    else if (action == "Close") {
        recovery_.clear();
        DestroyWindow(window_);
    }
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
    for (const auto& path : paths) stage.panels.push_back({path,{}});
    selectStageArtwork(StageSelection::panel);
    status_ = "Panels added to " + stage.title + ".";
}

void Editor::importModel() {
    const auto path = dialogPath(window_,false,false,false,true);
    if (path.empty()) return;
    int skipped = 0;
    auto model = importCharacterFile(path,images_,skipped);
    selectGlbTab_ = !model.mesh.file.empty();
    insertModel(std::move(model));
    status_ = "Imported editable model settings. " + std::to_string(skipped) + " unsupported commands skipped.";
}

GlbModel* Editor::meshAsset(const fs::path& path) {
    if (path.empty()) return nullptr;
    if (!glb_ || path != glbPath_) {
        auto next = std::make_unique<GlbModel>(path,images_);
        meshPreview_.reset();
        glbPath_ = path;
        glb_ = std::move(next);
    }
    return glb_.get();
}

void Editor::importGlb() {
    static const COMDLG_FILTERSPEC filter{L"glTF binary model",L"*.glb"};
    const auto path = dialogPath(window_,false,false,false,false,&filter);
    if (path.empty()) return;
    auto& asset = *meshAsset(path);
    Character model;
    model.title = utf8(path.stem());
    model.id = modelToken(model.title);
    model.mesh.file = path;
    const float height = asset.dimensions(model.mesh)[1];
    if (std::isfinite(height) && height > 0) model.mesh.scale = std::clamp(60.f/height,.000001f,1'000'000.f);
    const auto& clips = asset.clips();
    const auto idle = std::find_if(clips.begin(),clips.end(),[](const MeshClip& clip) {
        const auto name = lower(clip.name);
        return name == "idle" || name == "battle_idle";
    });
    auto& animation = model.animations.emplace_back();
    animation.meshClip = idle == clips.end() ? 0 : int(idle-clips.begin());
    animation.frames = meshTimingFrames(clips[size_t(animation.meshClip)].seconds);
    insertModel(std::move(model));
    characterView_ = {4,{}};
    selectGlbTab_ = true;
    status_ = "Imported "+std::to_string(clips.size())+" GLB clips. Choose their OpenBOR animation IDs in the GLB tab.";
}

void Editor::meshInspector(Character& model, Animation* animation) {
    auto& mesh = model.mesh;
    if (ImGui::Button(mesh.file.empty() ? "Attach GLB" : "Replace GLB",{-1,27})) {
        try {
            static const COMDLG_FILTERSPEC filter{L"glTF binary model",L"*.glb"};
            const auto path = dialogPath(window_,false,false,false,false,&filter);
            if (!path.empty()) {
                const bool first = mesh.file.empty();
                std::vector<MeshClip> old;
                if (!first && fs::is_regular_file(mesh.file)) old = meshAsset(mesh.file)->clips();
                auto next = std::make_unique<GlbModel>(path,images_);
                checkpoint();
                mesh.file = path;
                const auto& clips = next->clips();
                if (first) {
                    const auto height = next->dimensions(mesh)[1];
                    if (std::isfinite(height) && height > 0)
                        mesh.scale = std::clamp(60.f/height,.000001f,1'000'000.f);
                } else {
                    for (auto& state : model.animations) {
                        if (state.meshClip < 0) continue;
                        const auto index = size_t(state.meshClip);
                        if (index < old.size() && index < clips.size() && old[index].name == clips[index].name) continue;
                        const auto match = std::find_if(clips.begin(),clips.end(),[&](const MeshClip& clip) {
                            return index < old.size() && !old[index].name.empty() && clip.name == old[index].name;
                        });
                        state.meshClip = match == clips.end() ? -1 : int(match-clips.begin());
                    }
                }
                glbPath_ = path;
                glb_ = std::move(next);
                meshPreview_.reset();
                resetPlayback();
            }
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    if (mesh.file.empty()) return;
    ImGui::TextWrapped("%s",utf8(mesh.file.filename()).c_str());
    if (ImGui::SmallButton("Remove GLB")) {
        checkpoint();
        mesh = {};
        for (auto& clip : model.animations) clip.meshClip = -1;
        resetPlayback();
        return;
    }
    GlbModel* asset = nullptr;
    try { asset = meshAsset(mesh.file); }
    catch (const std::exception& error) { ImGui::TextWrapped("%s",error.what()); return; }

    section("Size");
    if (validMeshAxes(mesh.up,mesh.forward)) {
        const auto dimensions = asset->dimensions(mesh);
        ImGui::Text("%.1f × %.1f × %.1f",dimensions[0],dimensions[1],dimensions[2]);
        textDim("Rest pose width × height × depth");
        textDim("OpenBOR units");
        if (dimensions[1] > 0 && std::isfinite(dimensions[1])) {
            auto height = dimensions[1];
            ImGui::SetNextItemWidth(-121);
            if (numberFields<float>("Target height",{{height,.1f,10000.f}},.25f,"%.1f") && std::isfinite(height)) {
                checkpoint();
                mesh.scale = std::clamp(mesh.scale*std::clamp(height,.1f,10000.f)/dimensions[1],.000001f,1'000'000.f);
            }
        }
    }
    auto scale = mesh.scale;
    ImGui::SetNextItemWidth(-121);
    if (numberFields<float>("Scale",{{scale,.000001f,1'000'000.f}},std::max(.001f,scale*.005f),"%.6g") && std::isfinite(scale)) {
        checkpoint();
        mesh.scale = std::clamp(scale,.000001f,1'000'000.f);
    }
    labelTip("OpenBOR units per source GLB unit.");

    section("Orientation");
    static constexpr const char* axes[]{"+X","+Y","+Z","-X","-Y","-Z"};
    const auto chooseAxis = [&](const char* label, MeshAxis& current, MeshAxis other, bool up) {
        ImGui::SetNextItemWidth(91);
        const int value = int(current);
        if (!ImGui::BeginCombo(label,value >= 0 && value < 6 ? axes[value] : "Choose axis")) return;
        for (int i = 0; i < IM_ARRAYSIZE(axes); ++i) {
            const auto next = MeshAxis(i);
            const bool valid = up ? validMeshAxes(next,other) : validMeshAxes(other,next);
            if (ImGui::Selectable(axes[i],value == i,valid ? 0 : ImGuiSelectableFlags_Disabled) && value != i) {
                checkpoint();
                current = next;
            }
        }
        ImGui::EndCombo();
    };
    chooseAxis("Source up",mesh.up,mesh.forward,true);
    chooseAxis("Source forward",mesh.forward,mesh.up,false);
    auto pitch = mesh.pitch, yaw = mesh.yaw, roll = mesh.roll;
    ImGui::TextUnformatted("Pitch X / Yaw Y / Roll Z (°)");
    ImGui::SetNextItemWidth(-1);
    if (numberFields<float>("##Angles",{{pitch,-360,360},{yaw,-360,360},{roll,-360,360}},.5f,"%.1f") &&
        std::isfinite(pitch) && std::isfinite(yaw) && std::isfinite(roll)) {
        checkpoint();
        mesh.pitch = std::clamp(pitch,-360.f,360.f);
        mesh.yaw = std::clamp(yaw,-360.f,360.f);
        mesh.roll = std::clamp(roll,-360.f,360.f);
    }
    bool planar = mesh.planar;
    if (ImGui::Checkbox("Planar projection",&planar)) { checkpoint(); mesh.planar = planar; }
    labelTip("Uses the character's foot position for depth against other stage objects.");

    section("Animation mapping");
    if (ImGui::Button("+ OpenBOR animation",{-1,26})) ImGui::OpenPopup("Native animation slots");
    bool switched = false;
    if (ImGui::BeginPopup("Native animation slots")) {
        static constexpr const char* slots[]{"idle","walk","backwalk","run","jump","jumpattack","land","block",
            "attack1","attack2","attack3","attack4","special","special2","freespecial1",
            "charge","pain","fall","rise","death","victory"};
        if (ImGui::MenuItem("Custom slot")) { addAnimation(); switched = true; }
        for (const char* id : slots) {
            if (!ImGui::MenuItem(id)) continue;
            auto& states = model.animations;
            const auto existing = std::find_if(states.begin(),states.end(),[&](const Animation& state) {
                return animationId(state.name) == id;
            });
            auto index = int(existing-states.begin());
            if (existing == states.end()) {
                checkpoint();
                auto& state = states.emplace_back();
                state.name = id;
                state.loop = state.name == "idle" || state.name == "walk" || state.name == "backwalk" || state.name == "run";
            }
            setAnimation(index);
            switched = true;
            break;
        }
        ImGui::EndPopup();
    }
    if (switched) return;
    if (!animation) return;
    ImGui::TextUnformatted("OpenBOR animation ID");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##OpenBOR animation",&animation->name)) dirty_ = true;
    const auto& clips = asset->clips();
    const auto caption = [&](size_t index) {
        const auto& clip = clips[index];
        if (clip.name.empty()) return "Unnamed clip "+std::to_string(index+1);
        if (clip.exportName != clip.name) return clip.name+"  (#"+std::to_string(index+1)+")";
        return clip.name;
    };
    const bool mapped = animation->meshClip >= 0 && size_t(animation->meshClip) < clips.size();
    ImGui::TextUnformatted("GLB clip");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##GLB clip",mapped ? caption(size_t(animation->meshClip)).c_str() : "Choose a clip")) {
        if (ImGui::Selectable("Use sprite frames only",!mapped) && mapped) {
            checkpoint();
            animation->meshClip = -1;
            if (std::all_of(animation->frames.begin(),animation->frames.end(),[](const Frame& frame) { return frame.image.empty(); })) {
                animation->frames.clear();
                animation->subEntity.enabled = animation->item.enabled = false;
                frame_ = 0;
            }
            resetPlayback();
        }
        for (size_t i = 0; i < clips.size(); ++i) {
            ImGui::PushID(int(i));
            if (ImGui::Selectable(caption(i).c_str(),animation->meshClip == int(i)) && animation->meshClip != int(i)) {
                checkpoint();
                animation->meshClip = int(i);
                if (animation->frames.empty()) animation->frames = meshTimingFrames(clips[i].seconds);
                resetPlayback();
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (animation->meshClip >= 0 && size_t(animation->meshClip) < clips.size())
        ImGui::Text("%.2f s source clip",clips[size_t(animation->meshClip)].seconds);
    if (ImGui::Checkbox("Loop",&animation->loop)) dirty_ = true;
}

void Editor::drawMesh(ImDrawList* draw, const MeshSettings& settings, const Animation& animation,
                      ImVec2 start, ImVec2 size, ImVec2 origin, float zoom, size_t frameIndex) {
    const auto& asset = *meshAsset(settings.file);
    const float resolution = std::min({1.f,640/std::max(1.f,size.x),480/std::max(1.f,size.y)});
    const auto pivot = mul(sub(origin,start),resolution);
    const MeshView view{std::max(1,int(std::ceil(size.x*resolution))),std::max(1,int(std::ceil(size.y*resolution))),
                        pivot.x,pivot.y,zoom*resolution,facingLeft_};
    const int denominator = int(animation.frames.size())-!animation.loop;
    const float phase = denominator > 0 ? float(frameIndex)/denominator : 0;
    const int clip = animation.meshClip;
    if (clip < 0 || size_t(clip) >= asset.clips().size()) throw std::runtime_error("Choose an existing GLB clip.");
    if (!meshPreview_ || meshPreview_->settings != settings || meshPreview_->view != view ||
        meshPreview_->clip != clip || meshPreview_->phase != phase) {
        meshPreview_ = MeshPreview{settings,view,clip,phase,images_.createTexture(asset.render(settings,clip,phase,view))};
    }
    image(draw,&meshPreview_->texture,start,add(start,size));
}

void Editor::tick(Animation& animation) {
    if (!playing_ || animation.frames.empty()) return;
    const float dt = std::min(ImGui::GetIO().DeltaTime, .1f) * playbackSpeed_;
    if (cueAge_ >= 0) cueAge_ += dt;
    if (itemAge_ >= 0) itemAge_ += dt;
    playbackSeconds_ += dt;
    const auto trigger = [&]() {
        if (animation.subEntity.enabled && frame_ == animation.subEntity.frame) cueAge_ = 0;
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
            if (ImGui::IsKeyPressed(ImGuiKey_F1,false)) { stageMode_ = false; moduleMode_ = false; }
            if (ImGui::IsKeyPressed(ImGuiKey_F2,false)) { stageMode_ = true; moduleMode_ = false; resetPlayback(); }
        }
        const auto area = ImGui::GetContentRegionAvail();
        const float left = std::clamp(area.x*.17f,181.0f,235.0f);
        const float height = std::max(340.0f,area.y-30);
        if (moduleMode_) moduleWorkspace(height);
        else if (stageMode_) stageWorkspace(left,height);
        else {
            const auto& io = ImGui::GetIO();
            if (!project_.characters.empty() && !io.WantTextInput && !io.KeyCtrl && !io.KeyAlt &&
                !io.KeySuper && !ImGui::IsAnyItemActive() &&
                ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)) {
                const auto& clips = project_.characters[size_t(model_)].animations;
                if (!clips.empty()) {
                    const int direction = int(ImGui::IsKeyPressed(ImGuiKey_Period,true)) -
                                          int(ImGui::IsKeyPressed(ImGuiKey_Comma,true));
                    if (direction) stepFrame(clips[size_t(animation_)],direction);
                }
            }
            characterWorkspace(left,height);
        }
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
    if (toggleButton("Characters",!stageMode_ && !moduleMode_,{93,35})) {
        stageMode_ = false;
        moduleMode_ = false;
    }
    ImGui::SameLine();
    if (toggleButton("Stages",stageMode_ && !moduleMode_,{72,35})) {
        stageMode_ = true;
        moduleMode_ = false;
        resetPlayback();
    }
    ImGui::SameLine();
    if (toggleButton("Module",moduleMode_,{72,35})) {
        moduleMode_ = true;
        resetPlayback();
    }
    ImGui::SameLine();
    if (ImGui::Button("TXT",{51,35})) {
        const auto context = moduleMode_ ? CommandContext::models :
            stageMode_ && !project_.stages.empty() ? CommandContext::stage :
            !project_.characters.empty() ? CommandContext::model : CommandContext::models;
        openCommands(context);
    }
    if (!project_.nativeFiles.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Native TXT",{92,35})) {
            const auto target = moduleMode_ ? std::string("data/models.txt") :
                stageMode_ && !project_.stages.empty() ? project_.stages[size_t(stage_)].nativeFile :
                !project_.characters.empty() ? project_.characters[size_t(model_)].nativeFile : "";
            selectNativeFile(target.empty() ? "data/models.txt" : target);
            nativeOpen_ = true;
        }
    }
    ImGui::SameLine();
    const float toolbar = 350;
    if (ImGui::GetContentRegionAvail().x > toolbar) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - toolbar);
    }
    if (ImGui::Button("Project",{70,35})) ImGui::OpenPopup("Project menu");
    if (ImGui::BeginPopup("Project menu")) {
        if (ImGui::MenuItem("New project","Ctrl+N")) replace("New");
        if (ImGui::MenuItem("Open project","Ctrl+O")) replace("Open");
        if (ImGui::MenuItem("Recover autosave",nullptr,false,!dirty_)) {
            try {
                recoveryOffer_ = recovery_.latest();
                showRecovery_ = !recoveryOffer_.empty();
                if (!showRecovery_) status_ = "No earlier autosave is available.";
            } catch (const std::exception& error) { status_ = error.what(); }
        }
        if (ImGui::MenuItem("Open OpenBOR PAK…")) replace("Open PAK");
        if (ImGui::MenuItem("Open OpenBOR data folder…")) replace("Open data");
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
        exportedTo_.clear();
        exportError_.clear();
        playableExport_ = !project_.stages.empty() &&
            std::any_of(project_.characters.begin(),project_.characters.end(),
                        [](const Character& model) { return model.type == "player"; });
        showExport_ = true;
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
    exportDialog();
    commandsDialog();
    nativeDialog();
    jumpSimulator();
    recoveryDialog();
    playtestDialog();
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
}

void Editor::playtestDialog() {
    if (showPlaytest_) {
        ImGui::OpenPopup("Test Play");
        showPlaytest_ = false;
    }
    ImGui::SetNextWindowSize({520,0},ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Test Play",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (!project_.stages.empty()) ImGui::TextUnformatted(project_.stages[size_t(stage_)].title.c_str());
    ImGui::Separator();
    ImGui::BeginDisabled(playtest_.active());
    if (ImGui::Button(playtest_.engine().empty() ? "Link OpenBOR executable" : "Change OpenBOR executable")) {
        try {
            static const COMDLG_FILTERSPEC exe{L"Windows executable",L"*.exe"};
            const auto path = dialogPath(window_,false,false,false,false,&exe,L"Choose an OpenBOR executable");
            if (!path.empty()) { playtest_.linkEngine(path); testError_.clear(); }
        } catch (const std::exception& error) { testError_ = error.what(); }
    }
    if (!playtest_.engine().empty()) ImGui::TextWrapped("%s",utf8(playtest_.engine()).c_str());
    const auto playable = [](const Character& model) { return model.type == "player" && hasFrames(model); };
    const auto* selected = findCharacter(project_,testPlayer_);
    if (!selected || !playable(*selected)) {
        const auto first = std::find_if(project_.characters.begin(),project_.characters.end(),playable);
        selected = first == project_.characters.end() ? nullptr : &*first;
        testPlayer_ = selected ? selected->id : "";
    }
    ImGui::TextUnformatted("Player character");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Test player",selected ? selected->title.c_str() : "Add a player model with frames")) {
        for (const auto& model : project_.characters) {
            if (!playable(model)) continue;
            ImGui::PushID(model.id.c_str());
            if (ImGui::Selectable(model.title.c_str(),model.id == testPlayer_)) testPlayer_ = model.id;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (!testError_.empty()) ImGui::TextWrapped("%s",testError_.c_str());
    ImGui::Spacing();
    if (playtest_.building()) {
        ImGui::TextUnformatted("Building the selected stage...");
    } else if (playtest_.active()) {
        if (ImGui::Button("Stop test")) playtest_.stop();
        ImGui::SameLine();
    } else {
        ImGui::BeginDisabled(testPlayer_.empty() || playtest_.engine().empty() || project_.stages.empty());
        if (filled("Play selected stage")) {
            try {
                playtest_.start(project_,{size_t(stage_),testPlayer_});
                testError_.clear();
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& error) { testError_ = error.what(); }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    }
    const auto logs = playtest_.logs();
    if (!logs.empty() && fs::is_directory(logs)) {
        if (ImGui::Button("Open test logs"))
            ShellExecuteW(window_,L"open",logs.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        ImGui::SameLine();
    }
    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Editor::recoveryDialog() {
    if (showRecovery_) {
        recoveryError_.clear();
        ImGui::OpenPopup("Recover unsaved work");
        showRecovery_ = false;
    }
    if (!ImGui::BeginPopupModal("Recover unsaved work",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::TextUnformatted("Studio found a recovery snapshot from an earlier session.");
    if (!recoveryError_.empty()) ImGui::TextWrapped("%s",recoveryError_.c_str());
    if (filled("Recover latest")) {
        try {
            auto recovered = recovery_.restore(recoveryOffer_);
            adoptProject(std::move(recovered),{},false);
            dirty_ = true;
            recoveryOffer_.clear();
            status_ = "Recovered unsaved work. Use Save to keep it as a project.";
            ImGui::CloseCurrentPopup();
        } catch (const std::exception& error) { recoveryError_ = error.what(); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Skip recovery")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Editor::jumpCanvas(const Character& model, const Animation* animation, size_t frameIndex,
                        const JumpArc& arc, const JumpPoint& point) {
    const auto start = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    const ImVec2 size{std::max(1.f,available.x),std::max(1.f,available.y)};
    const auto end = add(start,size);
    ImGui::Dummy(size);
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(start,end,true);
    draw->AddRectFilled(start,end,ImGui::GetColorU32(ImGuiCol_WindowBg),5);
    const auto& peak = arc.points[arc.apex];
    const float distance = arc.points.back().x;
    const float direction = facingLeft_ ? -1.f : 1.f;
    const float zoom = std::min({5.f,size.x/(std::abs(distance)+104),size.y/(peak.altitude+96)});
    const auto origin = add(start,{(size.x-distance*direction*zoom)*.5f,size.y-38});
    const auto at = [&](const JumpPoint& p) {
        return add(origin,{p.x*direction*zoom,-p.altitude*zoom});
    };
    draw->AddLine({start.x+8,origin.y},{end.x-8,origin.y},guideColor,2);
    draw->AddLine({start.x+8,at(peak).y},{end.x-8,at(peak).y},guideColor);
    const size_t stride = std::max<size_t>(1,arc.points.size()/450);
    for (size_t i=0; i+1<arc.points.size(); i+=stride) {
        const size_t next = std::min(i+stride,arc.points.size()-1);
        draw->AddLine(at(arc.points[i]),at(arc.points[next]),shotColor,2);
    }
    draw->AddCircleFilled(origin,3,dim);
    draw->AddCircleFilled(at(peak),4,shotColor);
    if (arc.landed) draw->AddCircleFilled(at(arc.points.back()),4,bodyColor);
    const auto pivot = at(point);
    draw->AddCircleFilled({pivot.x,origin.y},6,IM_COL32(103,122,143,105));
    try {
        if (animation && animation->meshClip >= 0 && !model.mesh.file.empty()) {
            drawMesh(draw,model.mesh,*animation,start,size,pivot,zoom,frameIndex);
        } else if (const auto* frame = animation ? &animation->frames[frameIndex] : previewFrame(&model)) {
            if (const auto* texture = images_.texture(frame->image)) effectedSprite(draw,*texture,*frame,pivot,zoom,facingLeft_);
        }
    } catch (const std::exception& error) {
        label(draw,add(start,{12,10}),error.what(),attackColor);
    }
    cross(draw,pivot,accent);
    draw->PopClipRect();
}

void Editor::jumpSimulator() {
    if (jumpOpen_) {
        ImGui::OpenPopup("Jump simulator");
        jumpOpen_ = false;
    }
    const auto work = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(1020.f,work.x-32),std::min(680.f,work.y-32)},ImGuiCond_Appearing);
    bool open = true;
    if (!ImGui::BeginPopupModal("Jump simulator",&open)) {
        jumpPlaying_ = false;
        return;
    }
    if (project_.characters.empty()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    auto& model = project_.characters[size_t(model_)];
    const auto jump = [&](const char* title, JumpMode mode) {
        if (toggleButton(title,jumpMode_ == mode)) {
            jumpMode_ = mode;
            jumpTime_ = 0;
            jumpPlaying_ = false;
        }
    };
    jump("Standing",JumpMode::standing);
    ImGui::SameLine();
    jump("Forward",JumpMode::forward);
    ImGui::SameLine();
    jump("Running",JumpMode::running);
    ImGui::SameLine();
    if (ImGui::Button(facingLeft_ ? "Facing left" : "Facing right")) facingLeft_ = !facingLeft_;

    const float height = std::max(120.f,ImGui::GetContentRegionAvail().y-40);
    constexpr float sidebar = 256;
    const float canvasWidth = std::max(1.f,ImGui::GetContentRegionAvail().x-sidebar-ImGui::GetStyle().ItemSpacing.x);
    child("Jump settings",{sidebar,height});
    section("Character settings");
    if (jumpStage_ >= int(project_.stages.size())) jumpStage_ = -1;
    const auto* stage = jumpStage_ < 0 ? nullptr : &project_.stages[size_t(jumpStage_)];
    JumpSettings settings;
    try {
        settings = jumpSettings(project_,model,stage);
    } catch (const std::exception& error) {
        ImGui::TextWrapped("%s",error.what());
    }
    const bool nativeOverride = project_.nativeEdits.contains(model.nativeFile);
    ImGui::BeginDisabled(nativeOverride);
    const auto tune = [&](const char* name, float& value, float maximum) {
        ImGui::PushID(name);
        ImGui::TextUnformatted(name);
        ImGui::SetNextItemWidth(-1);
        auto next = value;
        if (numberFields<float>("##Value",{{next,-1,maximum}},.1f,"%.2f")) {
            checkpoint();
            value = next < 0 ? -1.f : next;
        }
        ImGui::PopID();
    };
    if (jumpMode_ == JumpMode::running) {
        tune("Running jump height",model.entity.runningJumpHeight,100);
        tune("Running distance multiplier",model.entity.runningJumpDistance,20);
    } else {
        tune("Jump height",model.entity.jumpHeight,100);
    }
    if (jumpMode_ != JumpMode::standing) tune("Forward jump speed",model.entity.jumpSpeed,100);
    ImGui::EndDisabled();
    if (nativeOverride) ImGui::TextWrapped("This model uses a Native TXT override. Edit its jump settings there.");
    else textDim("-1 uses the engine or module default.");
    if (jumpMode_ == JumpMode::running)
        ImGui::Text("Running launch  %.2f Y  /  %.2f X",
                    settings.runningHeight,settings.speed*settings.runningDistance);
    else if (jumpMode_ == JumpMode::standing)
        ImGui::Text("Effective launch  %.2f Y",settings.height);
    else
        ImGui::Text("Effective launch  %.2f Y  /  %.2f X",settings.height,settings.speed);

    ImGui::Separator();
    section("Preview floor");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Jump stage",stage ? stage->title.c_str() : "Module defaults")) {
        if (ImGui::Selectable("Module defaults",jumpStage_ < 0)) jumpStage_ = -1;
        for (size_t i=0; i<project_.stages.size(); ++i) {
            ImGui::PushID(int(i));
            if (ImGui::Selectable(project_.stages[i].title.c_str(),jumpStage_ == int(i))) jumpStage_ = int(i);
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    bool overrideGravity = jumpGravity_.has_value();
    if (ImGui::Checkbox("Preview gravity override",&overrideGravity)) {
        if (overrideGravity) jumpGravity_ = settings.gravity;
        else jumpGravity_.reset();
    }
    if (jumpGravity_) {
        ImGui::SetNextItemWidth(-1);
        numberFields<float>("##Preview gravity",{{*jumpGravity_,-10,10}},.01f,"%.3f");
        settings.gravity = *jumpGravity_;
    } else {
        ImGui::Text("Gravity  %.3f",settings.gravity);
    }
    textDim("Flat ground preview");
    ImGui::EndChild();
    ImGui::SameLine();

    const auto* airborne = jumpAnimation(model,jumpMode_);
    const auto* takeoff = jumpClip(model,"jumpdelay");
    const auto* landing = jumpClip(model,"jumpland");
    const auto arc = simulateJump(settings,jumpMode_);
    const float before = jumpClipSeconds(takeoff);
    const float duration = before+arc.seconds+(arc.landed ? jumpClipSeconds(landing) : 0);
    jumpTime_ = std::clamp(jumpTime_,0.f,duration);
    if (jumpPlaying_) {
        jumpTime_ = std::min(duration,jumpTime_+std::min(.1f,ImGui::GetIO().DeltaTime)*playbackSpeed_);
        if (jumpTime_ == duration) jumpPlaying_ = false;
    }
    child("Jump arc",{canvasWidth,height},ImGuiWindowFlags_NoScrollbar);
    ImGui::BeginDisabled(duration <= 0);
    if (toggleButton(jumpPlaying_ ? "Pause" : "Play jump",jumpPlaying_,{94,0})) {
        if (!jumpPlaying_ && jumpTime_ == duration) jumpTime_ = 0;
        jumpPlaying_ = !jumpPlaying_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Apex")) {
        jumpTime_ = before+float(arc.apex)/settings.ticksPerSecond;
        jumpPlaying_ = false;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##Jump time",&jumpTime_,0,duration,"%.2f s")) jumpPlaying_ = false;
    ImGui::EndDisabled();

    const float flightTime = std::clamp(jumpTime_-before,0.f,arc.seconds);
    const float step = flightTime*settings.ticksPerSecond;
    const auto tick = std::min(size_t(step),arc.points.size()-1);
    const auto& from = arc.points[tick];
    const auto& to = arc.points[std::min(tick+1,arc.points.size()-1)];
    const float blend = step-float(tick);
    const JumpPoint point{std::lerp(from.x,to.x,blend),std::lerp(from.altitude,to.altitude,blend)};
    const Animation* clip = airborne;
    float clipTime = flightTime;
    if (jumpTime_ < before) {
        clip = takeoff;
        clipTime = jumpTime_;
    } else if (arc.landed && jumpTime_ >= before+arc.seconds && landing) {
        clip = landing;
        clipTime = jumpTime_-before-arc.seconds;
    }
    const size_t pose = clip ? jumpClipFrame(*clip,clipTime) : 0;
    if (clip) ImGui::Text("%s  /  Frame %02zu",clip->name.c_str(),pose);
    else ImGui::TextUnformatted("Add a Jump animation to preview its frames.");
    if (arc.landed)
        ImGui::Text("Apex %.1f px   |   Travel %.1f px   |   Air time %.2f s",
                    arc.points[arc.apex].altitude,std::abs(arc.points.back().x),arc.seconds);
    else ImGui::TextUnformatted(arc.seconds > 0 ? "No landing within 12 seconds" : "No upward launch velocity");
    jumpCanvas(model,clip,pose,arc,point);
    ImGui::EndChild();
    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

std::string* Editor::commandText(Project& project, CommandContext context) const {
    if (context == CommandContext::models) return &project.module.modelsHeader;
    if (context == CommandContext::levels) return &project.module.levelsHeader;
    if (context == CommandContext::levelSet) return project.stages.empty() ? nullptr : &project.module.levelSetCommands;
    if (context == CommandContext::video) return &project.module.videoHeader;
    if (context == CommandContext::stage)
        return project.stages.empty() ? nullptr : &project.stages[size_t(stage_)].extraHeader;
    if (project.characters.empty()) return nullptr;
    auto& model = project.characters[size_t(model_)];
    if (context == CommandContext::model) return &model.extraHeader;
    if (model.animations.empty()) return nullptr;
    auto& animation = model.animations[size_t(animation_)];
    if (context == CommandContext::animation) return &animation.extraCommands;
    return animation.frames.empty() ? nullptr : &animation.frames[size_t(frame_)].extraCommands;
}

void Editor::openCommands(CommandContext context) {
    const auto* text = commandText(project_,context);
    if (!text) return;
    commandContext_ = context;
    commandDraft_ = *text;
    commandFilter_.clear();
    commandReference_.clear();
    commandGenerated_.clear();
    commandCursor_ = int(commandDraft_.size());
    commandShowGenerated_ = false;
    commandOpen_ = true;
}

namespace {
struct CommandLine {
    size_t start, first, end, tokenEnd;
    std::string token;
};

CommandLine lineAt(std::string_view text, int position) {
    const auto cursor = std::min(text.size(),size_t(std::max(0,position)));
    const auto previous = cursor ? text.rfind('\n',cursor-1) : std::string_view::npos;
    const auto start = previous == std::string_view::npos ? 0 : previous+1;
    const auto end = std::min(text.size(),text.find('\n',cursor));
    const auto first = std::min(end,text.find_first_not_of(" \t\r",start));
    const auto tokenEnd = std::min(end,text.find_first_of(" \t\r#",first));
    return {start,first,end,tokenEnd,lower(std::string(text.substr(first,tokenEnd-first)))};
}

struct CommandInput {
    std::span<const CommandHelp> catalog;
    int& cursor;
};

int completeCommand(ImGuiInputTextCallbackData* input) {
    auto& editor = *static_cast<CommandInput*>(input->UserData);
    const auto line = lineAt(std::string_view(input->Buf,size_t(input->BufTextLen)),input->CursorPos);
    if (input->EventFlag == ImGuiInputTextFlags_CallbackCompletion && !line.token.empty() &&
        size_t(input->CursorPos) == line.tokenEnd) {
        const auto match = std::find_if(editor.catalog.begin(),editor.catalog.end(),[&](const CommandHelp& command) {
            return command.name.starts_with(line.token);
        });
        if (match != editor.catalog.end()) {
            input->DeleteChars(int(line.first),int(line.tokenEnd-line.first));
            const auto completion = std::string(match->name)+(line.tokenEnd == line.end ? " " : "");
            input->InsertChars(int(line.first),completion.c_str());
        }
    }
    editor.cursor = input->CursorPos;
    return 0;
}
}

void Editor::commandsDialog() {
    if (commandOpen_) {
        ImGui::OpenPopup("OpenBOR TXT editor");
        commandOpen_ = false;
    }
    const auto work = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(960.f,work.x-24),std::min(620.f,work.y-24)},ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("OpenBOR TXT editor")) return;
    static constexpr const char* contexts[]{"Model header","Selected animation","Selected frame","Stage",
                                             "models.txt","levels.txt header","Level set","video.txt"};
    static constexpr const char* locations[]{
        "Written before the model’s animation blocks.",
        "Written inside the selected animation, before its frames.",
        "Written immediately before the selected frame line.",
        "Written before visually placed stage events.",
        "Written before the generated model registrations.",
        "Written before the generated stage set.",
        "Written inside Adventure, before its generated stages.",
        "Written after the default video mode."
    };
    auto* destination = commandText(project_,commandContext_);
    const bool changed = destination && commandDraft_ != *destination;
    ImGui::SetNextItemWidth(180);
    ImGui::BeginDisabled(changed);
    if (ImGui::BeginCombo("##Command context",contexts[int(commandContext_)])) {
        for (int i = 0; i < IM_ARRAYSIZE(contexts); ++i) {
            const auto context = CommandContext(i);
            const auto* next = commandText(project_,context);
            if (ImGui::Selectable(contexts[i],context == commandContext_,next ? 0 : ImGuiSelectableFlags_Disabled) &&
                context != commandContext_ && next) {
                commandContext_ = context;
                destination = commandText(project_,context);
                commandDraft_ = *next;
                commandFilter_.clear();
                commandReference_.clear();
                commandCursor_ = int(commandDraft_.size());
                commandShowGenerated_ = false;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (changed) labelTip("Apply your edits before changing context.");
    ImGui::SameLine();
    if (toggleButton("Commands",!commandShowGenerated_)) commandShowGenerated_ = false;
    ImGui::SameLine();
    if (toggleButton("Generated TXT",commandShowGenerated_) && !commandShowGenerated_) {
        commandShowGenerated_ = true;
        try {
            auto preview = project_;
            if (auto* target = commandText(preview,commandContext_)) *target = commandDraft_;
            switch (commandContext_) {
            case CommandContext::model:
            case CommandContext::animation:
            case CommandContext::frame: {
                const auto& model = preview.characters[size_t(model_)];
                commandGenerated_ = modelText(model,meshAsset(model.mesh.file));
                break;
            }
            case CommandContext::stage: commandGenerated_ = stageText(preview.stages[size_t(stage_)],size_t(stage_),images_); break;
            case CommandContext::models: commandGenerated_ = modelsConfigurationText(preview); break;
            case CommandContext::levels:
            case CommandContext::levelSet: commandGenerated_ = levelsConfigurationText(preview); break;
            case CommandContext::video: commandGenerated_ = "video 0\n"+preview.module.videoHeader+"\n"; break;
            }
        } catch (const std::exception& error) { commandGenerated_ = error.what(); }
    }
    textDim(locations[int(commandContext_)]);
    const float height = std::max(50.f,ImGui::GetContentRegionAvail().y-40);
    if (commandShowGenerated_) {
        ImGui::InputTextMultiline("##Generated TXT",&commandGenerated_,{-1,height},ImGuiInputTextFlags_ReadOnly);
    } else {
        const auto catalog = commandHelp(commandContext_);
        const float sidebar = std::min(300.f,ImGui::GetContentRegionAvail().x*.42f);
        const float editor = std::max(1.f,ImGui::GetContentRegionAvail().x-sidebar-ImGui::GetStyle().ItemSpacing.x);
        CommandInput input{catalog,commandCursor_};
        const int previousCursor = commandCursor_;
        const bool edited = ImGui::InputTextMultiline("##Native TXT",&commandDraft_,{editor,height},
            ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCompletion,completeCommand,&input);
        if (edited || commandCursor_ != previousCursor) commandReference_.clear();
        ImGui::SameLine();
        child("OpenBOR parameter help",{sidebar,height});
        const auto line = lineAt(commandDraft_,commandCursor_);
        const auto lookup = commandReference_.empty() ? line.token : commandReference_;
        const auto help = std::find_if(catalog.begin(),catalog.end(),[&](const CommandHelp& entry) { return entry.name == lookup; });
        if (help != catalog.end()) {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(accent),"%.*s",int(help->name.size()),help->name.data());
            ImGui::TextWrapped("%.*s",int(help->syntax.size()),help->syntax.data());
            ImGui::TextWrapped("%.*s",int(help->description.size()),help->description.data());
            if (ImGui::Button("Insert command")) {
                const bool complete = line.first == line.end || (!line.token.empty() && help->name.starts_with(line.token) &&
                                      line.tokenEnd == line.end);
                if (complete) {
                    commandDraft_.replace(line.first,line.tokenEnd-line.first,std::string(help->name)+" ");
                    commandCursor_ = int(line.first+help->name.size()+1);
                } else {
                    commandDraft_.insert(line.end,"\n"+std::string(help->name)+" ");
                    commandCursor_ = int(line.end+help->name.size()+2);
                }
            }
            ImGui::Separator();
        } else textDim("Tab completes a command name.");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##Search parameters","Filter parameters",&commandFilter_);
        const auto filter = lower(commandFilter_);
        for (const auto& entry : catalog) {
            if (!filter.empty() && entry.name.find(filter) == std::string_view::npos &&
                lower(std::string(entry.description)).find(filter) == std::string::npos) continue;
            if (ImGui::Selectable(entry.name.data(),entry.name == lookup)) commandReference_ = entry.name;
        }
        ImGui::EndChild();
    }
    ImGui::BeginDisabled(!destination || commandDraft_ == *destination);
    if (filled("Apply commands")) {
        checkpoint();
        *destination = commandDraft_;
        status_ = "Updated OpenBOR commands.";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (commandShowGenerated_) {
        if (ImGui::Button("Copy generated TXT")) ImGui::SetClipboardText(commandGenerated_.c_str());
        ImGui::SameLine();
    }
    if (ImGui::Button(commandDraft_ == (destination ? *destination : std::string{}) ? "Close" : "Discard draft"))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Editor::selectNativeFile(const std::string& name) {
    nativeSelection_ = name;
    nativeDraft_.clear();
    nativeSaved_.clear();
    nativeError_.clear();
    try {
        nativeSaved_ = nativeDraft_ = nativeFileText(project_,name);
    } catch (const std::exception& error) { nativeError_ = error.what(); }
}

void Editor::nativeDialog() {
    if (nativeOpen_) {
        ImGui::OpenPopup("OpenBOR native files");
        nativeOpen_ = false;
    }
    ImGui::SetNextWindowSize({965,645},ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("OpenBOR native files")) return;
    textDim("Applied text is saved in the Studio project and used for this file on export.");
    const float height = std::max(80.f,ImGui::GetContentRegionAvail().y-40);
    ImGui::BeginChild("Native files",{275,height},ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Native filter","Filter files",&nativeFilter_);
    const bool changed = nativeDraft_ != nativeSaved_;
    const auto filter = lower(nativeFilter_);
    ImGui::BeginDisabled(changed);
    for (const auto& [name,file] : project_.nativeFiles) {
        const auto extension = mediaExtension(file);
        if (extension != ".txt" && extension != ".c" && extension != ".h" && extension != ".inc") continue;
        if (!filter.empty() && name.find(filter) == std::string::npos) continue;
        if (ImGui::Selectable(name.c_str(),name == nativeSelection_)) selectNativeFile(name);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s",name.c_str());
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("Native text",{0,height},ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(nativeSelection_.c_str());
    if (!nativeError_.empty()) ImGui::TextWrapped("%s",nativeError_.c_str());
    else ImGui::InputTextMultiline("##Native source",&nativeDraft_,{-1,-1},ImGuiInputTextFlags_AllowTabInput);
    ImGui::EndChild();
    ImGui::BeginDisabled(!changed || !nativeError_.empty());
    if (filled("Apply text")) {
        checkpoint();
        project_.nativeEdits[nativeSelection_] = nativeDraft_;
        nativeSaved_ = nativeDraft_;
        status_ = "Updated "+nativeSelection_+".";
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (changed) {
        if (ImGui::Button("Discard draft")) nativeDraft_ = nativeSaved_;
    } else {
        if (project_.nativeEdits.contains(nativeSelection_)) {
            if (ImGui::Button("Revert to imported text")) {
                checkpoint();
                project_.nativeEdits.erase(nativeSelection_);
                selectNativeFile(nativeSelection_);
            }
            ImGui::SameLine();
        }
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void Editor::exportDialog() {
    if (showExport_) {
        ImGui::OpenPopup("Export OpenBOR");
        showExport_ = false;
        exportTarget_.clear();
    }
    ImGui::SetNextWindowSize({530,0},ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Export OpenBOR",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) return;
    if (exportedTo_.empty()) {
        constexpr const char* destinations[]{"New export folder","Replace an existing PAK","Update an existing data folder"};
        ImGui::SetNextItemWidth(-1);
        int mode = int(exportDestination_);
        ImGui::BeginDisabled(!exportTarget_.empty());
        if (ImGui::Combo("##Export destination",&mode,destinations,IM_ARRAYSIZE(destinations))) {
            exportDestination_ = ExportDestination(mode);
            exportError_.clear();
        }
        if (exportDestination_ == ExportDestination::newFolder) {
            if (ImGui::RadioButton("Playable module",playableExport_)) playableExport_ = true;
            ImGui::SameLine();
            if (ImGui::RadioButton(project_.nativeFiles.empty() ? "Content for an existing module" : "Editable data folder",
                                   !playableExport_)) playableExport_ = false;
        }
        ImGui::EndDisabled();
        if (exportDestination_ == ExportDestination::existingPak) {
            ImGui::TextWrapped("Replaces the selected PAK with a complete export of this project.");
        } else if (exportDestination_ == ExportDestination::existingData) {
            ImGui::TextWrapped("Overwrites matching files in the selected data folder and keeps files outside the export.");
        } else if (!project_.nativeFiles.empty()) {
            ImGui::TextWrapped("%s",playableExport_ ? "Creates a PAK and a copy of the complete data folder." :
                                                    "Exports a copy of the complete data folder.");
            ImGui::TextWrapped("Frame sounds and stage music update native commands in place. Other visual model or stage edits can regenerate the corresponding TXT.");
        } else ImGui::TextWrapped("%s",playableExport_ ?
            "Creates a PAK plus editable data files. Requires a player model and a stage." :
            "Exports models, stages, and any module layout settings for merging into your game.");
        if (std::any_of(project_.characters.begin(),project_.characters.end(),[](const Character& model) {
                return !model.mesh.file.empty();
            }))
            ImGui::TextWrapped("GLB characters require an OpenBOR3D build with meshmodel and meshclip support.");
        if (!exportError_.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text,ImGui::ColorConvertU32ToFloat4(attackColor));
            ImGui::TextWrapped("%s",exportError_.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        const auto runExport = [&](const fs::path& target) {
            exportedTo_ = exportProject(project_,target,images_,playableExport_,exportDestination_);
            status_ = "Exported to " + utf8(exportedTo_);
            exportError_.clear();
            exportTarget_.clear();
        };
        const auto report = [&](const std::exception& error) {
            exportError_ = error.what();
            status_ = exportError_;
        };
        if (!exportTarget_.empty()) {
            ImGui::TextWrapped("%s",utf8(exportTarget_).c_str());
            if (filled(exportDestination_ == ExportDestination::existingPak ? "Replace PAK" : "Update data folder")) {
                try { runExport(exportTarget_); }
                catch (const std::exception& error) { report(error); }
            }
            ImGui::SameLine();
            if (ImGui::Button("Choose another")) exportTarget_.clear();
        } else if (filled(exportDestination_ == ExportDestination::newFolder ? "Choose folder and export" :
                         exportDestination_ == ExportDestination::existingPak ? "Choose PAK" : "Choose data folder")) {
            try {
                if (exportDestination_ == ExportDestination::existingPak) {
                    static const COMDLG_FILTERSPEC pak{L"OpenBOR PAK",L"*.pak"};
                    exportTarget_ = dialogPath(window_,false,false,false,false,&pak,L"Choose the PAK to replace");
                } else {
                    const auto folder = dialogPath(window_,true,false);
                    if (!folder.empty()) {
                        if (exportDestination_ == ExportDestination::newFolder) runExport(folder);
                        else exportTarget_ = existingExportData(folder);
                    }
                }
            } catch (const std::exception& error) { report(error); }
        }
    } else {
        ImGui::TextUnformatted("Export complete");
        ImGui::TextWrapped("%s",utf8(exportedTo_).c_str());
        if (filled("Open export folder")) {
            const auto folder = exportDestination_ == ExportDestination::existingPak ? exportedTo_.parent_path() : exportedTo_;
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(window_,L"open",folder.c_str(),
                                                       nullptr,nullptr,SW_SHOWNORMAL)) <= 32)
                status_ = "Could not open the export folder.";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void Editor::chooseArt(fs::path& path, const char* button) {
    ImGui::PushID(button);
    if (ImGui::Button(button,{-1,27})) {
        try {
            const auto next = dialogPath(window_,false,false,true);
            if (!next.empty()) {
                (void)images_.read(next);
                checkpoint();
                path = next;
            }
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    if (!path.empty()) {
        ImGui::TextWrapped("%s",utf8(path.filename()).c_str());
        if (ImGui::SmallButton("Clear image")) { checkpoint(); path.clear(); }
    }
    ImGui::PopID();
}

Editor::InspectorLayout Editor::inspectorLayout() const {
    const float available = ImGui::GetContentRegionAvail().x - inspectorGripWidth;
    const float maximum = std::max(1.0f,available - 360);
    const float minimum = std::min(255.0f,maximum);
    const float preferred = inspectorWidth_.value_or(std::clamp(available*.26f,305.0f,390.0f));
    const float width = std::clamp(preferred,minimum,maximum);
    return {std::max(1.0f,available-width),width,minimum,maximum};
}

void Editor::inspectorDivider(const InspectorLayout& layout, float height) {
    ImGui::SameLine(0,0);
    ImGui::InvisibleButton("##Resize inspector",{inspectorGripWidth,height});
    if (ImGui::IsItemActivated()) inspectorDragWidth_ = layout.width;
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        inspectorWidth_.reset();
    } else if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const float delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x;
        inspectorWidth_ = std::clamp(inspectorDragWidth_-delta,layout.minimum,layout.maximum);
    }
    if (hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (!active) labelTip("Drag to resize the inspector. Double-click to reset its width.");
    const auto min = ImGui::GetItemRectMin();
    const auto max = ImGui::GetItemRectMax();
    const float x = (min.x+max.x)*.5f;
    const float y = (min.y+max.y)*.5f;
    auto* draw = ImGui::GetWindowDrawList();
    if (hovered || active) draw->AddLine({x,min.y+2},{x,max.y-2},accent);
    for (int dot = -1; dot <= 1; ++dot)
        draw->AddCircleFilled({x,y+dot*6.0f},1.5f,(hovered || active) ? accent : dim);
    ImGui::SameLine(0,0);
}

void Editor::moduleWorkspace(float height) {
    if (!ImGui::BeginTabBar("Module settings")) return;
    if (ImGui::BeginTabItem("HUD")) {
        layoutEditor(false,height-30);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Character Select")) {
        layoutEditor(true,height-30);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Screens")) {
        screensEditor(height-30);
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Commands")) {
        child("Module commands",{-1,height-30});
        if (ImGui::Button("Edit models.txt commands",{240,30})) openCommands(CommandContext::models);
        if (ImGui::Button("Edit levels.txt commands",{240,30})) openCommands(CommandContext::levels);
        ImGui::BeginDisabled(project_.stages.empty());
        if (ImGui::Button("Edit level set commands",{240,30})) openCommands(CommandContext::levelSet);
        ImGui::EndDisabled();
        if (ImGui::Button("Edit video.txt commands",{240,30})) openCommands(CommandContext::video);
        ImGui::EndChild();
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

bool Editor::pickMedia(fs::path& path, const char* label, bool scene) {
    static const COMDLG_FILTERSPEC scenes{L"OpenBOR cutscenes",L"*.gif;*.webm"};
    static const COMDLG_FILTERSPEC audio{L"OpenBOR audio",L"*.wav;*.ogg;*.oga"};
    ImGui::PushID(label);
    bool changed = false;
    if (ImGui::Button(label,{-1,27})) {
        try {
            const auto file = dialogPath(window_,false,false,false,false,scene ? &scenes : &audio);
            if (!file.empty()) {
                if (scene) validateScene(file);
                else validateAudio(file);
                checkpoint();
                path = file;
                changed = true;
            }
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    if (!path.empty()) {
        ImGui::TextWrapped("%s",utf8(path.filename()).c_str());
        if (ImGui::SmallButton("Clear")) {
            checkpoint();
            path.clear();
            changed = true;
        } else if (!scene && mediaExtension(path) == ".wav") {
            ImGui::SameLine();
            if (ImGui::SmallButton("Play WAV") &&
                !PlaySoundW(path.c_str(),nullptr,SND_FILENAME | SND_ASYNC | SND_NODEFAULT))
                status_ = "Windows could not preview this WAV.";
            ImGui::SameLine();
            if (ImGui::SmallButton("Stop")) PlaySoundW(nullptr,nullptr,0);
        }
    }
    ImGui::PopID();
    return changed;
}

bool Editor::sceneInspector(ScreenScene& scene, const char* label) {
    ImGui::PushID(label);
    bool changed = pickMedia(scene.file,label,true);
    if (!scene.file.empty()) {
        if (mediaExtension(scene.file) == ".gif") {
            ImGui::TextUnformatted("Position (X / Y)");
            auto position = scene.position;
            ImGui::SetNextItemWidth(-1);
            if (numberFields<int>("##Scene position",{{position.x,-2048,2048},{position.y,-2048,2048}})) {
                checkpoint();
                scene.position = position;
            }
        } else if (ImGui::Button("Open in media player",{-1,26})) {
            if (reinterpret_cast<INT_PTR>(ShellExecuteW(window_,L"open",scene.file.c_str(),nullptr,nullptr,SW_SHOWNORMAL)) <= 32)
                status_ = "Windows could not open a media player for this WebM.";
        }
        bool skippable = scene.skippable;
        if (ImGui::Checkbox("Allow skipping",&skippable)) {
            checkpoint();
            scene.skippable = skippable;
        }
    }
    ImGui::PopID();
    return changed;
}

void Editor::screensEditor(float height) {
    static constexpr const char* screenNames[]{
        "Title / Press Start","Main menu","Options","Startup background","Startup scene","Idle intro","How To Play"
    };
    static constexpr const char* mainRows[]{"Start Game","Options","How To Play","Hall Of Fame","Quit"};
    static constexpr const char* optionsRows[]{"Video Options...","Sound Options...","Control Options...","System Options...","Back"};
    static constexpr std::array backgroundIndices{1,2,2,0};
    auto& screens = project_.module.screens;
    const auto layout = inspectorLayout();
    child("Screen preview",{layout.preview,height},ImGuiWindowFlags_NoScrollbar);
    ImGui::SetNextItemWidth(225);
    if (ImGui::Combo("##Screen",&screenPreview_,screenNames,IM_ARRAYSIZE(screenNames))) {
        screenRow_ = 0;
        sceneTime_ = 0;
        scenePlaying_ = false;
        sceneDrag_.reset();
    }
    ImGui::SameLine();
    textDim("320 × 240");
    const bool isScene = screenPreview_ >= int(backgroundIndices.size());
    auto* scene = isScene ? &screens.scenes[size_t(screenPreview_-int(backgroundIndices.size()))] : nullptr;
    auto* background = isScene ? nullptr : &screens.backgrounds[size_t(backgroundIndices[size_t(screenPreview_)])];
    const bool hasRows = screenPreview_ == 1 || screenPreview_ == 2;
    const auto available = ImGui::GetContentRegionAvail();
    const float zoom = std::max(.125f,std::min(available.x/320,(available.y-(scene ? 38 : 0))/240));
    const ImVec2 viewport{320*zoom,240*zoom};
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.0f,(available.x-viewport.x)*.5f));
    const auto origin = ImGui::GetCursorScreenPos();
    const auto end = add(origin,viewport);
    ImGui::InvisibleButton("##Screen canvas",viewport);
    const bool clicked = ImGui::IsItemClicked();
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin,end,true);
    draw->AddRectFilled(origin,end,IM_COL32_BLACK);
    float duration = 0;
    try {
        if (scene && mediaExtension(scene->file) == ".gif") {
            const auto& frame = images_.animatedGif(scene->file,sceneTime_,duration);
            const auto point = [&] {
                return add(origin,{scene->position.x*zoom,scene->position.y*zoom});
            };
            const auto size = ImVec2{frame.width*zoom,frame.height*zoom};
            const auto mouse = ImGui::GetIO().MousePos;
            const auto a = point();
            const auto b = add(a,size);
            if (clicked && mouse.x >= a.x && mouse.x < b.x && mouse.y >= a.y && mouse.y < b.y)
                sceneDrag_ = SpriteDrag{mouse,scene->position.x,scene->position.y};
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) sceneDrag_.reset();
            if (sceneDrag_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const auto delta = mul(sub(mouse,sceneDrag_->mouseStart),1/zoom);
                const auto x = std::clamp(sceneDrag_->originX+int(std::lround(delta.x)),-2048,2048);
                const auto y = std::clamp(sceneDrag_->originY+int(std::lround(delta.y)),-2048,2048);
                if (x != scene->position.x || y != scene->position.y) {
                    if (!sceneDrag_->changed) checkpoint();
                    sceneDrag_->changed = true;
                    scene->position = {x,y};
                }
            }
            if (const auto [dx,dy] = viewerNudge(!sceneDrag_); dx || dy)
                nudgePosition({scene->position.x,-2048,2048},{scene->position.y,-2048,2048},dx,dy,[&] { checkpoint(); });
            image(draw,&frame,point(),add(point(),size));
            if (sceneDrag_)
                draw->AddRect(point(),add(point(),size),accent,0,0,2);
        } else if (scene) {
            label(draw,add(origin,{13,13}),scene->file.empty() ? "Choose a GIF or WebM" :
                  "WebM plays in OpenBOR or your media player",ink);
        } else if (!background->empty()) {
            const auto* art = images_.texture(*background);
            if (!art) throw std::runtime_error("Screen artwork is unavailable.");
            image(draw,art,origin,end);
        } else {
            const bool title = screenPreview_ != 3;
            auto& art = screenBackgroundPreviews_[size_t(title)];
            if (!art) art = images_.createTexture(starterBackground(title));
            image(draw,&*art,origin,end);
        }
        if (hasRows || screenPreview_ == 0) {
            const auto font = [&](size_t index) -> const ScreenFontPreview& {
                auto& cache = screenFontPreviews_[index];
                const auto& settings = screens.fonts[index];
                if (!cache || cache->settings != settings) {
                    const auto pixels = screenFontPixels(settings,images_);
                    const int w = pixels.width/16, h = pixels.height/16;
                    ScreenFontPreview preview{settings,images_.createTexture(pixels)};
                    for (int glyph = 0; glyph < 256; ++glyph) {
                        const int left = glyph%16*w, top = glyph/16*h;
                        int first = w, last = -1;
                        for (int y = top; y < top+h; ++y) {
                            for (int x = 0; x < w; ++x) {
                                if (pixels.rgba[(size_t(y)*pixels.width+left+x)*4+3] < 128) continue;
                                first = std::min(first,x);
                                last = std::max(last,x);
                            }
                        }
                        const int advance = last >= first ? last-first+1+w/10 : 0;
                        preview.advances[size_t(glyph)] = advance > 1 ? advance : w/3;
                    }
                    cache = std::move(preview);
                }
                return *cache;
            };
            const auto text = [&](std::string_view line, int row, bool selected, int rowIndex) {
                const auto& bitmap = font(selected ? 1 : 0);
                const int w = bitmap.texture.width/16, h = bitmap.texture.height/16;
                int width = 0;
                for (unsigned char c : line) width += bitmap.advances[c];
                const auto start = add(origin,{((320-width)/2)*zoom,(120+row*(h+1))*zoom});
                auto glyph = start;
                const auto id = ImTextureID(reinterpret_cast<uintptr_t>(bitmap.texture.view.Get()));
                for (unsigned char c : line) {
                    const ImVec2 uv{(c%16)/16.0f,(c/16)/16.0f};
                    draw->AddImage(id,glyph,add(glyph,{w*zoom,h*zoom}),uv,add(uv,{1/16.0f,1/16.0f}));
                    glyph.x += bitmap.advances[c]*zoom;
                }
                const auto mouse = ImGui::GetIO().MousePos;
                if (clicked && rowIndex >= 0 && mouse.x >= start.x && mouse.x < glyph.x &&
                    mouse.y >= start.y && mouse.y < start.y+(h+1)*zoom)
                    screenRow_ = rowIndex;
            };
            if (hasRows) {
                if (const auto move = viewerNudge(true)[1]) screenRow_ = (screenRow_+move+5)%5;
                for (int i = 0; i < 5; ++i) {
                    const bool main = screenPreview_ == 1;
                    text(main ? mainRows[i] : optionsRows[i],main ? i+2 : i == 4 ? 5 : i-1,screenRow_ == i,i);
                }
            } else text("PRESS START",0,false,-1);
        }
    } catch (const std::exception& error) {
        label(draw,add(origin,{13,13}),error.what(),attackColor);
    }
    draw->PopClipRect();
    draw->AddRect(origin,end,guideColor);
    if (scene && duration > 0) {
        if (toggleButton(scenePlaying_ ? "Pause" : "Play",scenePlaying_,{67,0})) scenePlaying_ = !scenePlaying_;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(1.0f,ImGui::GetContentRegionAvail().x));
        if (ImGui::SliderFloat("##Scene time",&sceneTime_,0,duration,"%.2f s")) scenePlaying_ = false;
        if (scenePlaying_) sceneTime_ = std::fmod(sceneTime_+ImGui::GetIO().DeltaTime,duration);
    }
    ImGui::EndChild();
    inspectorDivider(layout,height);
    child("Screen inspector",{layout.width,height});
    const auto pick = [&](fs::path& path, const char* button, const COMDLG_FILTERSPEC* filter, const auto& validate) {
        ImGui::PushID(button);
        if (ImGui::Button(button,{-1,27})) {
            try {
                const auto file = dialogPath(window_,false,false,!filter,false,filter);
                if (!file.empty()) {
                    validate(file);
                    checkpoint();
                    path = file;
                    ImGui::PopID();
                    return true;
                }
            } catch (const std::exception& error) { status_ = error.what(); }
        }
        if (!path.empty()) {
            ImGui::TextWrapped("%s",utf8(path.filename()).c_str());
            if (ImGui::SmallButton("Clear")) {
                checkpoint();
                path.clear();
                ImGui::PopID();
                return true;
            }
        }
        ImGui::PopID();
        return false;
    };
    if (filled("Use main-openbor 1.1 assets",{-1,29})) {
        try {
            const auto folder = sample_.parent_path().parent_path()/"ScreenTemplate";
            auto setup = loadScreenTemplate(folder);
            auto selectArt = folder/"bgs/select.gif";
            if (!fs::is_regular_file(selectArt)) throw std::runtime_error("Missing template Character Select artwork.");
            checkpoint();
            screens = std::move(setup);
            project_.module.select.background = std::move(selectArt);
            project_.module.select.enabled = true;
            sceneTime_ = 0;
            scenePlaying_ = false;
            status_ = "Applied main-openbor 1.1 screen assets.";
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    if (scene) {
        section(screenNames[screenPreview_]);
        if (sceneInspector(*scene,"Choose cutscene")) {
            sceneTime_ = 0;
            scenePlaying_ = false;
            sceneDrag_.reset();
        }
    } else {
        section(screenPreview_ == 2 ? "Main menu and Options" : screenNames[screenPreview_]);
        chooseArt(*background,"Choose background");
        if (screenPreview_ != 3) {
            section("Menu music");
            static const COMDLG_FILTERSPEC music{L"OpenBOR music",L"*.bor;*.ogg"};
            pick(screens.music,"Choose menu music",&music,[](const fs::path& file) {
                const auto extension = lower(utf8(file.extension()));
                if (extension != ".bor" && extension != ".ogg")
                    throw std::runtime_error("Menu music must be an OpenBOR .bor or .ogg file.");
            });
            section("Menu fonts");
            static constexpr const char* roles[]{"Regular","Selected","Heading","Large"};
            for (size_t i = 0; i < screens.fonts.size(); ++i) {
                ImGui::PushID(int(i));
                if (ImGui::CollapsingHeader(roles[i],i == 1 ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
                    auto& font = screens.fonts[i];
                    pick(font.bitmap,"Choose font sheet",nullptr,[&](const fs::path& file) {
                        auto next = font;
                        next.bitmap = file;
                        (void)screenFontPixels(next,images_);
                    });
                    bool tint = font.tintBitmap;
                    if (!font.bitmap.empty() && ImGui::Checkbox("Tint bitmap",&tint)) {
                        checkpoint();
                        font.tintBitmap = tint;
                    }
                    if (font.bitmap.empty() || font.tintBitmap) {
                        float rgb[]{float((font.color>>16)&255)/255,float((font.color>>8)&255)/255,float(font.color&255)/255};
                        ImGui::SetNextItemWidth(-70);
                        if (ImGui::ColorEdit3("Color",rgb,ImGuiColorEditFlags_DisplayHex)) {
                            checkpoint();
                            const auto packed = ImGui::ColorConvertFloat4ToU32({rgb[0],rgb[1],rgb[2],1});
                            font.color = 0xff000000u | (packed&255)<<16 | (packed&0xff00) | (packed>>16&255);
                        }
                    }
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
}

void Editor::layoutEditor(bool select, float height) {
    struct Element {
        std::string name, sample;
        ScreenPoint* position;
        ImVec2 offset, size;
        ImU32 color;
        const Texture* art = nullptr;
        const Frame* actor = nullptr;
        bool flip = false;
        const Gauge* gauge = nullptr;
        bool timer = false;
    };
    auto& module = project_.module;
    auto& hud = module.hud;
    auto& ccs = module.select;
    if (layoutSelectMode_ != select) {
        layoutSelectMode_ = select;
        layoutSelection_ = 0;
        layoutDragging_ = false;
    }
    const auto playerModel = std::find_if(project_.characters.begin(),project_.characters.end(),
                                         [](const Character& model) { return model.type == "player"; });
    const Character* model = playerModel == project_.characters.end() ? nullptr : &*playerModel;
    const auto* portrait = model ? images_.texture(model->icon) : nullptr;
    auto* actor = previewFrame(model);
    if (model) {
        const auto waiting = std::find_if(model->animations.begin(),model->animations.end(),
            [](const Animation& a) { return !a.frames.empty() && animationId(a.name) == "waiting"; });
        if (waiting != model->animations.end()) actor = &waiting->frames.front();
    }
    const auto* actorTexture = actor ? images_.texture(actor->image) : nullptr;
    std::vector<Element> items;
    const auto gaugeSize = [](const Gauge& gauge) { return ImVec2(float(gauge.width),float(gauge.height)); };
    if (select) {
        for (int i=0; i<module.maxPlayers && i<4; ++i) {
            auto& slot = ccs.slots[size_t(i)];
            const auto player = "P"+std::to_string(i+1);
            ImVec2 offset{-12,-26}, size{24,26};
            if (actorTexture) {
                const int pivot = slot.right ? actor->originX : actorTexture->width-actor->originX;
                offset = {-float(pivot),-float(actor->originY)};
                size = {float(actorTexture->width),float(actorTexture->height)};
            }
            items.push_back({player+" character",player,&slot.actor,offset,size,shotColor,
                             nullptr,actorTexture ? actor : nullptr,!slot.right});
            items.push_back({player+" Ready text","Ready!",&slot.ready,{}, {34,11},ink});
        }
    } else {
        if (const auto* overlay = images_.texture(hud.overlay))
            items.push_back({"HUD overlay","",&hud.overlayPosition,{},
                             {float(overlay->width),float(overlay->height)},ink,overlay});
        for (int i=0; i<module.maxPlayers && i<4; ++i) {
            auto& p = hud.players[size_t(i)];
            const auto player = "P"+std::to_string(i+1);
            const ImVec2 portraitSize = portrait ? ImVec2(float(portrait->width),float(portrait->height)) : ImVec2(16,16);
            items.push_back({player+" life","",&p.life,{},gaugeSize(hud.life),bodyColor,nullptr,nullptr,false,&hud.life});
            items.push_back({player+" MP","",&p.mp,{},gaugeSize(hud.mp),shotColor,nullptr,nullptr,false,&hud.mp});
            items.push_back({player+" opponent life","",&p.enemy,{},gaugeSize(hud.enemy),attackColor,
                             nullptr,nullptr,false,&hud.enemy});
            items.push_back({player+" portrait",player,&p.icon,{},portraitSize,accent,portrait});
            items.push_back({player+" name and score",model ? model->id+" - 0" : player+" - 0",
                             &p.score,{}, {80,10},ink});
        }
        items.push_back({"Timer","99",&hud.timer,{}, {float(hud.timerWidth),float(hud.timerHeight)},itemColor,
                         nullptr,nullptr,false,nullptr,true});
    }
    layoutSelection_ = clampIndex(layoutSelection_,items.size());
    const auto layout = inspectorLayout();
    child(select ? "Character select preview" : "HUD preview",{layout.preview,height},ImGuiWindowFlags_NoScrollbar);
    section(select ? "Character select preview" : "HUD preview");
    const auto available = ImGui::GetContentRegionAvail();
    const float zoom = std::max(.25f,std::min(available.x/320,available.y/240));
    const ImVec2 viewport{320*zoom,240*zoom};
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()+std::max(0.0f,(available.x-viewport.x)*.5f));
    const auto origin = ImGui::GetCursorScreenPos();
    const auto end = add(origin,viewport);
    ImGui::InvisibleButton("##Layout canvas",viewport);
    const auto mouse = ImGui::GetIO().MousePos;
    const auto logical = mul(sub(mouse,origin),1/zoom);
    const auto itemPoint = [&](const Element& item) {
        return add(ImVec2(float(item.position->x),float(item.position->y)),item.offset);
    };
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        layoutDragging_ = false;
        for (int i=int(items.size())-1; i>=0; --i) {
            const auto& item = items[size_t(i)];
            const auto p = itemPoint(item);
            const auto b = add(p,{std::max(5.0f,item.size.x),std::max(5.0f,item.size.y)});
            if (logical.x < p.x-2 || logical.x > b.x+2 || logical.y < p.y-2 || logical.y > b.y+2) continue;
            layoutSelection_ = i;
            layoutGrab_ = sub(logical,ImVec2(float(item.position->x),float(item.position->y)));
            layoutDragging_ = true;
            layoutChanged_ = false;
            break;
        }
    }
    if (layoutDragging_ && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !items.empty()) {
        auto& pos = *items[size_t(layoutSelection_)].position;
        const auto spot = sub(logical,layoutGrab_);
        const int x = int(std::lround(spot.x)), y = int(std::lround(spot.y));
        if (pos.x != x || pos.y != y) {
            if (!layoutChanged_) checkpoint();
            pos = {x,y};
            layoutChanged_ = true;
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) layoutDragging_ = false;
    if (const auto [dx,dy] = viewerNudge(!items.empty() && !layoutDragging_); dx || dy) {
        auto& pos = *items[size_t(layoutSelection_)].position;
        nudgePosition({pos.x,-2048,2048},{pos.y,-2048,2048},dx,dy,[&] { checkpoint(); });
    }
    auto* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(origin,end,true);
    draw->AddRectFilled(origin,end,IM_COL32(20,32,50,255));
    const fs::path background = select ? ccs.background :
        (project_.stages.empty() ? fs::path{} : project_.stages.front().background);
    if (const auto* art = images_.texture(background))
        image(draw,art,origin,add(origin,{art->width*zoom,art->height*zoom}),false,
              select ? IM_COL32_WHITE : IM_COL32(185,185,185,255));
    for (size_t i=0; i<items.size(); ++i) {
        const auto& item = items[i];
        const auto a = add(origin,mul(itemPoint(item),zoom));
        const auto b = add(a,mul(item.size,zoom));
        if (item.actor) {
            sprite(draw,images_,*item.actor,add(origin,{item.position->x*zoom,item.position->y*zoom}),zoom,item.flip);
        } else if (item.art) {
            image(draw,item.art,a,b);
        } else if (item.gauge) {
            draw->AddRectFilled(a,b,IM_COL32(11,18,27,255));
            if (item.gauge->vertical) draw->AddRectFilled({a.x,b.y-(b.y-a.y)*.7f},b,item.color);
            else draw->AddRectFilled(a,{a.x+(b.x-a.x)*.7f,b.y},item.color);
            if (!item.gauge->noBorder) draw->AddRect(a,b,ink);
        } else if (!item.sample.empty()) {
            if (item.timer && hud.timerBorder) draw->AddRect(a,b,ink);
            const auto text = item.timer ? add(a,{2*zoom,2*zoom}) : a;
            draw->AddText(ImGui::GetFont(),9*zoom,text,item.color,item.sample.c_str());
        }
        if (int(i) == layoutSelection_) draw->AddRect(sub(a,{2,2}),add(b,{2,2}),accent,0,0,2);
    }
    draw->PopClipRect();
    draw->AddRect(origin,end,guideColor);
    ImGui::EndChild();
    inspectorDivider(layout,height);
    child(select ? "Character select inspector" : "HUD inspector",{layout.width,height});
    section(select ? "Character Select" : "HUD");
    bool& enabled = select ? ccs.enabled : hud.enabled;
    if (ImGui::Checkbox("Include in export",&enabled)) dirty_ = true;
    ImGui::SetNextItemWidth(-98);
    if (numberFields<int>("Players",{{module.maxPlayers,1,4}})) dirty_ = true;
    if (select) {
        if (ImGui::Checkbox("Allow palette selection",&ccs.colorSelect)) dirty_ = true;
        chooseArt(ccs.background,"Choose select background");
        for (int i=0; i<std::clamp(module.maxPlayers,1,4); ++i) {
            ImGui::PushID(i);
            const auto title = "Player " + std::to_string(i+1) + " faces right";
            if (ImGui::Checkbox(title.c_str(),&ccs.slots[size_t(i)].right)) dirty_ = true;
            ImGui::PopID();
        }
    } else {
        chooseArt(hud.overlay,"Choose HUD overlay");
        const auto gauge = [&](const char* name, Gauge& g) {
            if (!ImGui::CollapsingHeader(name)) return;
            ImGui::PushID(name);
            ImGui::SetNextItemWidth(-99);
            if (numberFields<int>("Width / height",{{g.width,1,320},{g.height,1,240}})) dirty_ = true;
            if (ImGui::Checkbox("Ratio fill",&g.ratio)) dirty_ = true;
            if (ImGui::Checkbox("Vertical fill",&g.vertical)) dirty_ = true;
            if (ImGui::Checkbox("Hide border",&g.noBorder)) dirty_ = true;
            ImGui::TextUnformatted("Layers  Border / Shadow / Fill / Background");
            ImGui::SetNextItemWidth(-1);
            if (numberFields<int>("##Layers",{{g.border,-4096,4096},{g.shadow,-4096,4096},
                                              {g.fill,-4096,4096},{g.backfill,-4096,4096}})) dirty_ = true;
            ImGui::PopID();
        };
        gauge("Life gauge",hud.life);
        gauge("MP gauge",hud.mp);
        gauge("Opponent gauge",hud.enemy);
        if (ImGui::CollapsingHeader("Timer")) {
            ImGui::SetNextItemWidth(-100);
            if (numberFields<int>("Width / height",{{hud.timerWidth,1,320},{hud.timerHeight,1,240}})) dirty_ = true;
            ImGui::SetNextItemWidth(-100);
            if (numberFields<int>("Font",{{hud.timerFont,0,3}})) dirty_ = true;
            if (ImGui::Checkbox("Draw timer border",&hud.timerBorder)) dirty_ = true;
        }
    }
    if (!items.empty()) {
        auto& selected = items[size_t(layoutSelection_)];
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##Selected layout element",selected.name.c_str())) {
            for (size_t i=0; i<items.size(); ++i)
                if (ImGui::Selectable(items[i].name.c_str(),int(i)==layoutSelection_)) layoutSelection_ = int(i);
            ImGui::EndCombo();
        }
        auto& p = *items[size_t(layoutSelection_)].position;
        ImGui::SetNextItemWidth(-90);
        if (numberFields<int>("X / Y",{{p.x,-2048,2048},{p.y,-2048,2048}})) dirty_ = true;
    }
    ImGui::EndChild();
}

void Editor::characterWorkspace(float left, float height) {
    const float sidebarHeight = height-ImGui::GetStyle().ItemSpacing.y;
    const float modelsHeight = std::clamp(sidebarHeight*.60f,215.f,sidebarHeight-100.f);
    ImGui::BeginGroup();
    child("Models panel",{left,modelsHeight},ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    characterLibrary();
    ImGui::EndChild();
    child("Animations panel",{left,sidebarHeight-modelsHeight},ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    animationLibrary();
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::SameLine();
    const auto layout = inspectorLayout();
    Character* model = project_.characters.empty() ? nullptr : &project_.characters[size_t(model_)];
    Animation* animation = model && !model->animations.empty() ? &model->animations[size_t(animation_)] : nullptr;
    ImGui::BeginGroup();
    child("Animation viewport",{layout.preview,height-154},ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    characterCanvas(animation,height-154);
    ImGui::EndChild();
    child("Animation timeline",{layout.preview,146},ImGuiWindowFlags_NoScrollbar);
    timeline(animation,146);
    ImGui::EndChild();
    ImGui::EndGroup();
    inspectorDivider(layout,height);
    child("Character inspector",{layout.width,height});
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
    if (ImGui::Button("Import GLB model",{-1,29})) {
        try { importGlb(); } catch (const std::exception& error) { status_ = error.what(); }
    }
    ImGui::Spacing();
    if (ImGui::BeginChild("Model list",{0,0})) {
        ImGuiListClipper rows;
        rows.Begin(int(project_.characters.size()));
        if (revealModel_ && !project_.characters.empty()) rows.IncludeItemByIndex(model_);
        while (rows.Step()) {
            for (int i=rows.DisplayStart; i<rows.DisplayEnd; ++i) {
                const auto& model = project_.characters[size_t(i)];
                ImGui::PushID(i);
                const auto start = ImGui::GetCursorScreenPos();
                const bool selected = model_ == i;
                if (ImGui::Selectable("##Model",selected,0,{0,42})) setModel(i);
                if (selected && revealModel_ && !ImGui::IsItemVisible()) ImGui::SetScrollHereY();
                if (ImGui::IsItemVisible()) {
                    auto* draw = ImGui::GetWindowDrawList();
                    draw->AddRectFilled(add(start,{3,3}),add(start,{40,39}),IM_COL32(25,35,51,255),4);
                    if (const auto frame = previewFrame(&model))
                        thumbnail(draw,images_.texture(frame->image),add(start,{5,4}),{33,33});
                    if (selected) draw->AddRectFilled(start,add(start,{2,42}),accent,1);
                    draw->AddText(add(start,{48,3}),ink,model.title.c_str());
                    draw->AddText(add(start,{48,22}),dim,model.type.c_str());
                }
                ImGui::PopID();
            }
        }
        revealModel_ = false;
    }
    ImGui::EndChild();
}

void Editor::animationLibrary() {
    section("Animations");
    if (project_.characters.empty()) {
        textDim("Select or create a model.");
        return;
    }
    if (ImGui::Button("+ Animation",{-1,27})) addAnimation();
    const auto& animations = project_.characters[size_t(model_)].animations;
    if (ImGui::BeginChild("Animation list",{0,0})) {
        ImGuiListClipper rows;
        rows.Begin(int(animations.size()));
        if (revealAnimation_ && !animations.empty()) rows.IncludeItemByIndex(animation_);
        while (rows.Step()) {
            for (int i=rows.DisplayStart; i<rows.DisplayEnd; ++i) {
                const auto& animation = animations[size_t(i)];
                ImGui::PushID(i);
                const auto caption = animation.name + "  (" + std::to_string(animation.frames.size()) + ")";
                const bool selected = animation_ == i;
                if (ImGui::Selectable(caption.c_str(),selected,0,{0,25})) setAnimation(i);
                if (selected && revealAnimation_ && !ImGui::IsItemVisible()) ImGui::SetScrollHereY();
                ImGui::PopID();
            }
        }
        revealAnimation_ = false;
    }
    ImGui::EndChild();
}

void Editor::drawCue(SpawnCue& cue, const char* title, EditTool mode, Animation& animation) {
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
                    if (mode == EditTool::item ? model.type == "item" :
                        (model.type == "projectile" || model.type == "shot")) {
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
        if (numberFields<int>("Frame",{{cue.frame,0,int(animation.frames.size())-1}},1,nullptr,true)) dirty_ = true;
        if (ImGui::Button("Use current frame")) { checkpoint(); cue.frame = frame_; }
        ImGui::SameLine();
        if (ImGui::Button("Jump")) {
            frame_ = clampIndex(cue.frame,animation.frames.size());
            resetPlayback();
        }
        ImGui::TextUnformatted("Relative X / Z / altitude");
        ImGui::SetNextItemWidth(-1);
        if (numberFields<int>("##Position",{{cue.x,-2048,2048},{cue.z,-2048,2048},
                                             {cue.altitude,-2048,2048}})) dirty_ = true;
        if (toggleButton("Place in preview",tool_ == mode,{-1,25})) {
            tool_ = mode;
            frame_ = clampIndex(cue.frame,animation.frames.size());
            resetPlayback();
        }
    }
    ImGui::PopID();
}

void Editor::drawEffects(Frame& frame) {
    auto& effect = frame.effects;
    if (ImGui::Checkbox("Enable drawmethod",&effect.enabled)) dirty_ = true;
    if (effect.enabled) {
        ImGui::SetNextItemWidth(-108);
        if (numberFields<int>("Scale X / Y",{{effect.scaleX,1,4096},{effect.scaleY,1,4096}})) dirty_ = true;
        labelTip("256 is original size.");
        ImGui::SetNextItemWidth(-108);
        if (numberFields<int>("Rotation",{{effect.rotation,-360,360}})) dirty_ = true;
        if (ImGui::Checkbox("Flip X",&effect.flipX)) dirty_ = true;
        ImGui::SameLine();
        if (ImGui::Checkbox("Flip Y",&effect.flipY)) dirty_ = true;
        ImGui::TextUnformatted("Blend mode");
        constexpr const char* blends[]{"None","Alpha","Negative alpha","Overlay","Hard light","Dodge","Average"};
        ImGui::SetNextItemWidth(-1);
        ImGui::BeginDisabled(effect.embeddedAlpha || !effect.mask.empty());
        if (ImGui::Combo("##Blend",&effect.blend,blends,IM_ARRAYSIZE(blends))) dirty_ = true;
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    section("Alpha mask");
    bool fromAlpha = effect.embeddedAlpha;
    if (ImGui::Checkbox("Use sprite's alpha channel",&fromAlpha)) {
        checkpoint();
        effect.embeddedAlpha = fromAlpha;
        if (fromAlpha) effect.mask.clear();
    }
    if (ImGui::Button("Choose grayscale mask",{-1,27})) {
        try {
            const auto file = dialogPath(window_,false,false,true);
            if (!file.empty()) {
                auto candidate = frame;
                candidate.effects.mask = file;
                candidate.effects.embeddedAlpha = false;
                (void)images_.alphaMask(candidate,images_.read(frame.image));
                checkpoint();
                effect = std::move(candidate.effects);
            }
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    if (!effect.mask.empty()) {
        ImGui::TextWrapped("%s",utf8(effect.mask.filename()).c_str());
        if (ImGui::SmallButton("Remove mask")) { checkpoint(); effect.mask.clear(); }
    }
    ImGui::TextWrapped("The mask must match the frame size and transparent outline.");
}

void Editor::paletteEditor(Character& model) {
    if (model.colorSets.empty()) {
        ImGui::BeginDisabled(!hasFrames(model));
        if (filled("Build palette from frames",{-1,28})) {
            try {
                const auto colors = makeModelPalette(model,images_);
                checkpoint();
                model.sourcePalette.assign(colors.begin(),colors.end());
                model.colorSets.push_back({"Base",model.sourcePalette});
                paletteIndex_ = 0;
                paletteColor_ = 1;
            } catch (const std::exception& error) { status_ = error.what(); }
        }
        ImGui::EndDisabled();
        textDim("The palette is shared by every frame of the model.");
        return;
    }
    paletteIndex_ = clampIndex(paletteIndex_,model.colorSets.size());
    auto& set = model.colorSets[size_t(paletteIndex_)];
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Palette",set.name.c_str())) {
        for (size_t i=0; i<model.colorSets.size(); ++i) {
            ImGui::PushID(int(i));
            if (ImGui::Selectable(model.colorSets[i].name.c_str(),int(i)==paletteIndex_)) paletteIndex_ = int(i);
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(model.colorSets.size() >= 32);
    const bool addPalette = ImGui::Button("+ Alternate");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(paletteIndex_ == 0);
    const bool removePalette = ImGui::Button("Remove");
    ImGui::EndDisabled();
    if (addPalette) {
        const auto colors = model.colorSets[size_t(paletteIndex_)].colors;
        checkpoint();
        model.colorSets.push_back({"Alternate "+std::to_string(model.colorSets.size()),colors});
        paletteIndex_ = int(model.colorSets.size())-1;
    } else if (removePalette) {
        checkpoint();
        model.colorSets.erase(model.colorSets.begin()+paletteIndex_);
        for (auto& stage : project_.stages) {
            for (auto& spawn : stage.spawns) {
                if (spawn.model != model.id) continue;
                if (spawn.palette == paletteIndex_) spawn.palette = -1;
                else if (spawn.palette > paletteIndex_) --spawn.palette;
            }
        }
        paletteIndex_ = clampIndex(paletteIndex_,model.colorSets.size());
    }
    auto& selected = model.colorSets[size_t(paletteIndex_)];
    if (paletteIndex_ > 0) {
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Palette name",&selected.name)) dirty_ = true;
    }
    if (selected.colors.size() < 2) {
        textDim("This palette has no editable colors.");
        return;
    }
    paletteColor_ = std::clamp(paletteColor_,1,int(selected.colors.size())-1);
    const auto color = [](uint32_t v) {
        return ImVec4(float(v>>16&255)/255,float(v>>8&255)/255,float(v&255)/255,1);
    };
    auto value = color(selected.colors[size_t(paletteColor_)]);
    ImGui::Text("Index %d",paletteColor_);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::ColorEdit3("##Color",&value.x,ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Uint8)) {
        const auto r = uint32_t(std::lround(value.x*255));
        const auto g = uint32_t(std::lround(value.y*255));
        const auto b = uint32_t(std::lround(value.z*255));
        selected.colors[size_t(paletteColor_)] = 0xff000000u | r<<16 | g<<8 | b;
        dirty_ = true;
    }
    for (size_t i=0; i<selected.colors.size(); ++i) {
        if (i%8) ImGui::SameLine(0,5);
        ImGui::PushID(int(i));
        ImGui::BeginDisabled(!i);
        if (ImGui::ColorButton("##Swatch",i ? color(selected.colors[i]) : ImVec4(0,0,0,0),
                               ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip,{23,23}))
            paletteColor_ = int(i);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            if (i) ImGui::SetTooltip("Index %zu",i);
            else ImGui::SetTooltip("Index 0 is transparent.");
        }
        ImGui::PopID();
    }
}

void Editor::entityEditor(Character& model) {
    auto& entity = model.entity;
    chooseArt(model.icon,"Choose HUD portrait");
    pickMedia(model.deathSound,"Choose death sound");
    if (!ImGui::CollapsingHeader("Entity properties",ImGuiTreeNodeFlags_DefaultOpen)) return;
    textDim("Use -1 for engine defaults.");
    ImGui::SetNextItemWidth(-112);
    if (numberFields<int>("Max MP",{{entity.mp,-1,100000}})) dirty_ = true;
    ImGui::SetNextItemWidth(-112);
    if (numberFields<int>("Aggression",{{entity.aggression,-1,100}})) dirty_ = true;
    ImGui::SetNextItemWidth(-112);
    if (numberFields<int>("Score",{{entity.score,-1,1000000}})) dirty_ = true;
    ImGui::SetNextItemWidth(-112);
    if (numberFields<float>("Jump height",{{entity.jumpHeight,-1,100}},.1f,"%.2f")) dirty_ = true;
    ImGui::SetNextItemWidth(-112);
    if (numberFields<float>("Lifespan (s)",{{entity.lifespan,-1,10000}},.1f,"%.2f")) dirty_ = true;
    constexpr const char* options[]{"Engine default","Off","On"};
    int wall = std::clamp(entity.wall+1,0,2), gravity = std::clamp(entity.gravity+1,0,2);
    ImGui::SetNextItemWidth(-112);
    if (ImGui::Combo("Wall collision",&wall,options,IM_ARRAYSIZE(options))) {
        checkpoint(); entity.wall = wall-1;
    }
    ImGui::SetNextItemWidth(-112);
    if (ImGui::Combo("Gravity",&gravity,options,IM_ARRAYSIZE(options))) {
        checkpoint(); entity.gravity = gravity-1;
    }
    if (ImGui::CollapsingHeader("Model commands")) {
        if (ImGui::Button("Edit model commands",{-1,27})) openCommands(CommandContext::model);
    }
}

void Editor::frameInspector(Character& model, Animation* animation) {
    section("Inspector");
    if (ImGui::BeginTabBar("Model settings",inspectorTabs)) {
        if (ImGui::BeginTabItem("Entity")) {
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
            if (numberFields<int>("Health",{{model.health,1,100000}})) dirty_ = true;
            ImGui::SetNextItemWidth(-85);
            if (numberFields<float>("Speed",{{model.speed,0,50}},.05f,"%.2f")) dirty_ = true;
            entityEditor(model);
            if (animation) {
                ImGui::Separator();
                section("Selected animation");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##Animation ID",&animation->name)) dirty_ = true;
                if (ImGui::Checkbox("Loop",&animation->loop)) dirty_ = true;
                if (ImGui::Button("Edit animation commands",{-1,26})) openCommands(CommandContext::animation);
            }
            if (ImGui::Button("Edit / preview model TXT",{-1,30})) openCommands(CommandContext::model);
            if (animation && ImGui::Button("Remove animation",{-1,27})) {
                checkpoint();
                model.animations.erase(model.animations.begin()+animation_);
                setAnimation(clampIndex(animation_,model.animations.size()));
                animation = model.animations.empty() ? nullptr : &model.animations[size_t(animation_)];
            }
            if (ImGui::Button("Remove model",{-1,27})) {
                checkpoint();
                const auto id = model.id;
                project_.characters.erase(project_.characters.begin()+model_);
                for (auto& other : project_.characters) {
                    for (auto& clip : other.animations) {
                        if (clip.subEntity.model == id) clip.subEntity = {};
                        if (clip.item.model == id) clip.item = {};
                    }
                }
                for (auto& stage : project_.stages) {
                    std::erase_if(stage.spawns,[&](const StageSpawn& spawn){return spawn.model == id;});
                    for (auto& spawn : stage.spawns) if (spawn.item == id) spawn.item.clear();
                }
                setModel(clampIndex(model_,project_.characters.size()));
                ImGui::EndTabItem();
                ImGui::EndTabBar();
                return;
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Frame")) {
            if (animation && !animation->frames.empty()) {
                auto& frame = animation->frames[size_t(frame_)];
                ImGui::Text("%s  /  Frame %02d",animation->name.c_str(),frame_);
                textDim(frame.image.empty() ? "GLB timing frame" : utf8(frame.image.filename()).c_str());
                pickMedia(frame.sound,"Choose frame sound");
                if (ImGui::Button("Edit frame commands",{-1,26})) openCommands(CommandContext::frame);
                ImGui::SetNextItemWidth(-88);
                if (numberFields<int>("Delay (cs)",{{frame.ticks,1,1000}})) dirty_ = true;
                ImGui::SetNextItemWidth(-88);
                if (numberFields<int>("Origin",{{frame.originX,-4096,4096},{frame.originY,-4096,4096}})) dirty_ = true;
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
                        ImGui::SetNextItemWidth(-1);
                        int width = box.width, height = box.height;
                        if (numberFields<int>("##Box",{{box.x,-4096,4096},{box.y,-4096,4096},
                                                        {width,1,4096},{height,1,4096}})) {
                            box.width = std::max(1,width);
                            box.height = std::max(1,height);
                            dirty_ = true;
                        }
                        ImGui::SetNextItemWidth(-88);
                        if (numberFields<int>("Depth",{{box.depth,0,250}})) dirty_ = true;
                        if (attack) {
                            ImGui::SetNextItemWidth(-88);
                            if (numberFields<int>("Damage",{{box.damage,0,99999}})) dirty_ = true;
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
                drawCue(animation->subEntity,"Sub Entity cue",EditTool::subEntity,*animation);
                ImGui::Separator();
                drawCue(animation->item,"Item spawn cue",EditTool::item,*animation);
                ImGui::Spacing();
                ImGui::TextWrapped("Placement follows the parent and its facing. Set movement and attacks on the spawned model.");
            } else textDim("Add a frame to place a spawn cue.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Effects")) {
            if (animation && !animation->frames.empty() && !animation->frames[size_t(frame_)].image.empty())
                drawEffects(animation->frames[size_t(frame_)]);
            else textDim("Add a sprite frame to edit its draw effects.");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Palettes")) {
            paletteEditor(model);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GLB",nullptr,selectGlbTab_ ? ImGuiTabItemFlags_SetSelected : 0)) {
            meshInspector(model,animation);
            ImGui::EndTabItem();
        }
        selectGlbTab_ = false;
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
        {"Attack",EditTool::attack},{"Sub Entity",EditTool::subEntity},{"Item",EditTool::item}}};
    for (size_t i = 0; i < tools.size(); ++i) {
        if (i) ImGui::SameLine(0,5);
        const auto [name,mode] = tools[i];
        if (toggleButton(name,tool_==mode)) {
            tool_ = mode;
            characterSelection_ = EditTool::select;
        }
    }
    if (ImGui::GetContentRegionAvail().x > 92) {
        ImGui::SameLine();
        ImGui::Checkbox("Grid",&showGrid_);
    }
    if (!project_.characters.empty()) {
        if (ImGui::GetContentRegionAvail().x > ImGui::CalcTextSize("Jump simulator").x + ImGui::GetStyle().FramePadding.x*2)
            ImGui::SameLine(0,5);
        if (ImGui::Button("Jump simulator")) {
            resetPlayback();
            jumpTime_ = 0;
            jumpPlaying_ = false;
            jumpOpen_ = true;
            if (available && animationId(animation->name) == "runjump") jumpMode_ = JumpMode::running;
            if (available && animationId(animation->name) == "forwardjump") jumpMode_ = JumpMode::forward;
        }
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

    auto selected = tool_ == EditTool::select ? characterSelection_ : dragSprite ? EditTool::sprite : tool_;
    const auto& model = project_.characters[size_t(model_)];
    const bool mapped = animation->meshClip >= 0 && !model.mesh.file.empty();
    const int sign = facingLeft_ ? -1 : 1;
    const auto cuePoint = [&](const SpawnCue& cue) {
        return add(origin,{cue.x*sign*view.zoom,screenY(cue)*view.zoom});
    };
    auto& current = animation->frames[size_t(frame_)];
    const bool hasSprite = images_.texture(current.image) != nullptr || (mapped && current.image.empty());
    const bool nudgeSprite = selected == EditTool::select && tool_ == EditTool::select && hasSprite;
    const bool canNudge = nudgeSprite || (selected == EditTool::body && current.body.enabled && hasSprite) ||
                          (selected == EditTool::attack && current.attack.enabled && hasSprite) ||
                          (selected == EditTool::subEntity && animation->subEntity.enabled) ||
                          (selected == EditTool::item && animation->item.enabled) ||
                          ((selected == EditTool::origin || selected == EditTool::sprite) && hasSprite);
    if (const auto [dx,dy] = viewerNudge(canNudge && !spriteDrag_ && !dragging_); dx || dy) {
        const auto beforeMove = [&] { checkpoint(); resetPlayback(); };
        if (selected == EditTool::body || selected == EditTool::attack) {
            auto& box = selected == EditTool::body ? current.body : current.attack;
            nudgePosition({box.x,-4096,4096},{box.y,-4096,4096},sign*dx,dy,beforeMove);
        } else if (selected == EditTool::subEntity || selected == EditTool::item) {
            auto& cue = selected == EditTool::subEntity ? animation->subEntity : animation->item;
            nudgePosition({cue.x,-2048,2048},{cue.altitude,-2048,2048},sign*dx,-dy,beforeMove);
        } else {
            const auto direction = selected == EditTool::sprite || nudgeSprite ? -1 : 1;
            nudgePosition({current.originX,-4096,4096},{current.originY,-4096,4096},
                          sign*dx*direction,dy*direction,beforeMove);
        }
    }
    tick(*animation);
    auto& frame = animation->frames[size_t(frame_)];
    const auto tex = images_.texture(frame.image);
    const bool timingFrame = mapped && frame.image.empty();
    const int frameWidth = timingFrame ? meshFrameWidth : tex ? tex->width : 0;
    const int frameHeight = timingFrame ? meshFrameHeight : tex ? tex->height : 0;
    const bool frameBounds = frameWidth > 0 && frameHeight > 0;
    const auto topLeft = [&]() {
        const int pivot = facingLeft_ && frameBounds ? frameWidth-frame.originX : frame.originX;
        return sub(origin,{pivot*view.zoom,frame.originY*view.zoom});
    };
    const auto spriteCoord = [&](ImVec2 point) {
        auto p = mul(sub(point,topLeft()),1/view.zoom);
        if (facingLeft_ && frameBounds) p.x = frameWidth-p.x;
        return p;
    };
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && tool_ == EditTool::select) {
        const auto p = spriteCoord(io.MousePos);
        const auto inside = [&](const Box& box) {
            return frameBounds && box.enabled && p.x >= box.x && p.x <= float(box.x)+box.width &&
                   p.y >= box.y && p.y <= float(box.y)+box.height;
        };
        if (animation->item.enabled && nearMarker(io.MousePos,cuePoint(animation->item),11))
            selected = EditTool::item;
        else if (animation->subEntity.enabled && nearMarker(io.MousePos,cuePoint(animation->subEntity),11))
            selected = EditTool::subEntity;
        else if (frameBounds && nearMarker(io.MousePos,origin,8)) selected = EditTool::origin;
        else if (inside(frame.attack)) selected = EditTool::attack;
        else if (inside(frame.body)) selected = EditTool::body;
        else if (tex && !mapped && p.x >= 0 && p.x <= frameWidth && p.y >= 0 && p.y <= frameHeight)
            selected = EditTool::sprite;
        else selected = EditTool::select;
        characterSelection_ = selected;
        if (selected != EditTool::select) resetPlayback();
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && tool_ != EditTool::select) {
        resetPlayback();
        if (tool_ == EditTool::subEntity || tool_ == EditTool::item) {
            auto& cue = tool_ == EditTool::subEntity ? animation->subEntity : animation->item;
            checkpoint();
            const bool first = !cue.enabled;
            cue.enabled = true;
            if (first) {
                cue.frame = frame_;
                if (cue.model.empty()) {
                    const auto it = std::find_if(project_.characters.begin(),project_.characters.end(),
                        [&](const Character& c) {
                            return tool_ == EditTool::item ? c.type == "item" :
                                (c.type == "projectile" || c.type == "shot");
                        });
                    if (it != project_.characters.end()) cue.model = it->id;
                }
            }
            cue.x = int(std::lround((io.MousePos.x-origin.x)*sign/view.zoom));
            cue.altitude = cue.z-int(std::lround((io.MousePos.y-origin.y)/view.zoom));
        } else if (frameBounds) {
            const ImVec2 p = spriteCoord(io.MousePos);
            if (dragSprite) {
                if (p.x >= 0 && p.x <= frameWidth && p.y >= 0 && p.y <= frameHeight) {
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

    if (showOnionSkin_ && !mapped) {
        if (const auto* previous = previousFrame(*animation,size_t(frame_)))
            sprite(draw,images_,*previous,origin,view.zoom,facingLeft_,IM_COL32(183,171,255,115));
    }
    if (mapped) {
        try { drawMesh(draw,model.mesh,*animation,start,size,origin,view.zoom,size_t(frame_)); }
        catch (const std::exception& error) { label(draw,add(start,{18,20}),error.what(),attackColor); }
    } else if (tex) {
        const Texture* visible = tex;
        try {
            if (!model.colorSets.empty()) {
                paletteIndex_ = clampIndex(paletteIndex_,model.colorSets.size());
                visible = images_.preview(frame,model.sourcePalette,model.colorSets[size_t(paletteIndex_)].colors);
            } else if (frame.effects.embeddedAlpha || !frame.effects.mask.empty()) {
                visible = images_.preview(frame,{},{});
            }
        } catch (const std::exception& error) { status_ = error.what(); }
        if (visible) effectedSprite(draw,*visible,frame,origin,view.zoom,facingLeft_);
    }
    else label(draw,add(start,{18,20}),"Frame image unavailable",attackColor);
    if (tex && selected == EditTool::sprite) {
        const auto a = topLeft();
        draw->AddRect(a,add(a,{tex->width*view.zoom,tex->height*view.zoom}),accent,0,0,1.5f);
    }
    const auto rect = [&](const Box& box, ImU32 color, bool active) {
        if (!box.enabled || !frameBounds) return;
        const int x = facingLeft_ ? frameWidth-box.x-box.width : box.x;
        const auto a = add(topLeft(),{x*view.zoom,box.y*view.zoom});
        const auto b = add(a,{box.width*view.zoom,box.height*view.zoom});
        draw->AddRectFilled(a,b,(color & 0xffffff) | 0x2c000000);
        draw->AddRect(a,b,active ? accent : color,0.0f,0,active ? 2.5f : 1.5f);
    };
    rect(frame.body,bodyColor,selected == EditTool::body);
    rect(frame.attack,attackColor,selected == EditTool::attack);
    cross(draw,origin,accent,selected == EditTool::origin ? 8.0f : 5.0f);
    const auto drawSpawn = [&](const SpawnCue& cue, EditTool mode, float age) {
        if (!cue.enabled) return;
        const bool item = mode == EditTool::item;
        const auto color = item ? itemColor : shotColor;
        const bool current = cue.frame == frame_;
        const auto at = cuePoint(cue);
        const auto ground = add(origin,{cue.x*sign*view.zoom,cue.z*view.zoom});
        draw->AddLine(origin,ground,(color&0xffffff)|0x58000000,1);
        draw->AddLine(ground,at,(color&0xffffff)|0x90000000,1);
        if (showGhost_ && (current || age >= 0)) {
            const auto* child = previewFrame(findCharacter(project_,cue.model));
            if (child) {
                sprite(draw,images_,*child,at,view.zoom,facingLeft_,
                       current ? IM_COL32(255,255,255,175) : IM_COL32(255,255,255,140));
            }
        }
        cross(draw,at,current ? color : (color&0xffffff)|0x85000000);
        if (selected == mode)
            draw->AddCircle(at,12,accent,24,1.5f);
        const auto caption = std::string(item ? "Item" : "Sub Entity") + "  F" + std::to_string(cue.frame);
        label(draw,add(at,{11,item ? 10.0f : -24.0f}),caption.c_str(),color);
    };
    drawSpawn(animation->subEntity,EditTool::subEntity,cueAge_);
    drawSpawn(animation->item,EditTool::item,itemAge_);

    const auto scale = std::to_string(int(std::lround(view.zoom*100))) + "%";
    label(draw,{start.x+13,end.y-28},scale.c_str(),dim);
    const char* hint = dragSprite ? "Drag sprite  |  Arrow keys nudge" :
        tool_==EditTool::origin ? "Click to place pivot  |  Arrow keys nudge" :
        tool_==EditTool::body || tool_==EditTool::attack ? "Drag to draw box  |  Arrow keys nudge" :
        tool_==EditTool::subEntity || tool_==EditTool::item ? "Click to place spawn  |  Arrow keys nudge" :
        selected != EditTool::select ? "Arrow keys nudge selection" : "Arrow keys nudge sprite  |  Wheel zoom";
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
    if (ImGui::Button("<") && available) stepFrame(*animation,-1);
    labelTip("Previous frame (< or comma)");
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
    if (ImGui::Button(">") && available) stepFrame(*animation,1);
    labelTip("Next frame (> or period)");
    ImGui::SameLine(0,4);
    if (ImGui::Button("Delete") && available) {
        checkpoint();
        for (auto* cue : {&animation->subEntity,&animation->item}) {
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
        numberFields<float>("##Playback speed",{{playbackSpeed_,.1f,3.0f}},.025f,"%.2fx",false);
        playbackSpeed_ = std::clamp(playbackSpeed_,.1f,3.0f);
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
        if (revealFrame_ && i == frame_) ImGui::SetScrollHereX(.5f);
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
                    for (auto* cue : {&animation->subEntity,&animation->item}) {
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
        if (frame.image.empty()) draw->AddText(add(start,{22,21}),accent,"3D");
        else thumbnail(draw,images_.texture(frame.image),add(start,{6,4}),{54,49});
        const auto timing = std::to_string(i)+"  ·  "+std::to_string(frame.ticks)+" cs";
        draw->AddText(add(start,{6,55}),i==frame_ ? ink : dim,timing.c_str());
        if (animation->subEntity.enabled && animation->subEntity.frame == i)
            draw->AddCircleFilled(add(start,{56,9}),3.4f,shotColor);
        if (animation->item.enabled && animation->item.frame == i)
            draw->AddCircleFilled(add(start,{46,9}),3.4f,itemColor);
        ImGui::PopID();
    }
    ImGui::EndChild();
    revealFrame_ = false;
}

void Editor::stageWorkspace(float left, float height) {
    child("Stage library",{left,height});
    stageLibrary();
    ImGui::EndChild();
    ImGui::SameLine();
    const auto layout = inspectorLayout();
    Stage* stage = project_.stages.empty() ? nullptr : &project_.stages[size_t(stage_)];
    ImGui::BeginGroup();
    child("Stage viewport",{layout.preview,height-154},ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    stageCanvas(stage,height-154);
    ImGui::EndChild();
    child("Stage events",{layout.preview,146},ImGuiWindowFlags_NoScrollbar);
    stageEvents(stage,146);
    ImGui::EndChild();
    ImGui::EndGroup();
    inspectorDivider(layout,height);
    child("Stage inspector",{layout.width,height});
    stageInspector(stage);
    ImGui::EndChild();
}

void Editor::selectStageArtwork(StageSelection selection) {
    stageCameraPreview_ = false;
    stageSelection_ = selection;
    stageArtworkDrag_.reset();
    dragSpawn_ = -1;
    placeSpawn_ = false;
    terrainPlacement_.reset();
    terrainDrag_.reset();
    selectStageArtworkTab_ = true;
}

ScreenPoint* Editor::stageArtworkOffset(Stage& stage) {
    if (stageSelection_ == StageSelection::background && !stage.background.empty()) return &stage.backgroundOffset;
    if (stageSelection_ == StageSelection::panel && size_t(panel_) < stage.panels.size())
        return &stage.panels[size_t(panel_)].offset;
    return nullptr;
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
            stageSelection_ = StageSelection::spawn;
            stageArtworkDrag_.reset();
            terrainPlacement_.reset();
            terrainDrag_.reset();
            terrain_ = 0;
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
                checkpoint();
                stage.background = path;
                selectStageArtwork(StageSelection::background);
            }
        } catch (const std::exception& error) { status_ = error.what(); }
    }
    if (!stage.background.empty()) {
        const auto name = "Background  " + utf8(stage.background.filename()) + "##Stage background";
        if (ImGui::Selectable(name.c_str(),stageSelection_ == StageSelection::background))
            selectStageArtwork(StageSelection::background);
        if (ImGui::SmallButton("Clear background")) {
            checkpoint();
            stage.background.clear();
            stage.backgroundOffset = {};
            stage.repeatBackgroundX = stage.repeatBackgroundY = true;
            if (stageSelection_ == StageSelection::background) stageSelection_ = StageSelection::none;
            stageArtworkDrag_.reset();
        }
    }
    if (ImGui::Button("+ Add panels",{-1,29})) {
        try { addPanels(); } catch (const std::exception& error) { status_ = error.what(); }
    }
    for (size_t i = 0; i < stage.panels.size(); ++i) {
        ImGui::PushID(int(i));
        const auto label = std::string(1,char('A'+i))+"  "+utf8(stage.panels[i].image.filename());
        if (ImGui::Selectable(label.c_str(),stageSelection_ == StageSelection::panel && panel_==int(i),0,{0,25})) {
            panel_ = int(i);
            selectStageArtwork(StageSelection::panel);
        }
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
            stageArtworkDrag_.reset();
            if (stage.panels.empty() && stageSelection_ == StageSelection::panel) stageSelection_ = StageSelection::none;
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
    ImGui::SameLine();
    ImGui::BeginDisabled(!stage || (stage->background.empty() && stage->panels.empty()));
    const bool artwork = stage && stageArtworkOffset(*stage);
    if (toggleButton("Move artwork",artwork) && stage) {
        if (artwork) {
            stageSelection_ = StageSelection::none;
            stageArtworkDrag_.reset();
        } else selectStageArtwork(stage->panels.empty() ? StageSelection::background : StageSelection::panel);
    }
    ImGui::EndDisabled();
    if (stage) {
        if (ImGui::GetContentRegionAvail().x > 95) ImGui::SameLine();
        const bool terrainMode = stageSelection_ == StageSelection::terrain;
        if (toggleButton("Terrain",terrainMode)) {
            if (terrainMode) {
                terrainPlacement_.reset();
                terrainDrag_.reset();
                stageSelection_ = StageSelection::none;
            } else selectTerrain(terrain_);
        }
        if (ImGui::GetContentRegionAvail().x > 100) ImGui::SameLine();
        if (ImGui::Button(playtest_.active() ? "Test running" : "Test Play")) showPlaytest_ = true;
        if (ImGui::GetContentRegionAvail().x > 110) ImGui::SameLine();
        if (toggleButton("Camera",stageCameraPreview_)) {
            stageCameraPreview_ = !stageCameraPreview_;
            selectStageCameraTab_ = stageCameraPreview_;
            if (stageCameraPreview_) {
                stageSelection_ = StageSelection::none;
                placeSpawn_ = false;
                stageArtworkDrag_.reset();
                terrainDrag_.reset();
                terrainPlacement_.reset();
                dragSpawn_ = -1;
            }
        }
    }
    ImGui::Separator();

    std::vector<const Texture*> panels;
    std::vector<ImageSize> sizes;
    StageArtLayout layout;
    if (stage) {
        for (const auto& panel : stage->panels) {
            const auto* tex = images_.texture(panel.image);
            panels.push_back(tex);
            sizes.push_back(tex ? ImageSize{tex->width,tex->height} : ImageSize{});
        }
        layout = stageArtLayout(*stage,sizes);
    }
    const auto [screenWidth,screenHeight] = stageVideoSize(project_);
    const int maxCamera = std::max(0,layout.width()-screenWidth);
    camera_ = std::clamp(camera_,0,maxCamera);
    ImGui::SetNextItemWidth(-142);
    if (numberFields<int>("Preview X",{{camera_,0,maxCamera}},1,nullptr,true))
        camera_ = std::clamp(camera_,0,maxCamera);
    ImGui::SameLine();
    if (toggleButton("Place",placeSpawn_) && stage && !stage->spawns.empty()) {
        stageCameraPreview_ = false;
        placeSpawn_ = !placeSpawn_;
        stageArtworkDrag_.reset();
        terrainDrag_.reset();
        terrainPlacement_.reset();
        stageSelection_ = StageSelection::spawn;
    }
    if (stage && stage->spawns.empty()) placeSpawn_ = false;
    labelTip("Click the canvas to position the selected spawn.");

    const auto start = ImGui::GetCursorScreenPos();
    const auto size = ImGui::GetContentRegionAvail();
    const auto end = add(start,size);
    auto& view = stageView_;
    const auto canvasOrigin = [&]() {
        return add(start,{size.x*.5f - (camera_+screenWidth*.5f)*view.zoom + view.pan.x,
                          size.y*.5f - screenHeight*.5f*view.zoom + view.pan.y});
    };
    ImGui::InvisibleButton("##Stage canvas",size,ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    const auto& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    if (hovered && io.MouseWheel != 0 && !stageArtworkDrag_ && !terrainDrag_)
        zoomAt(view.zoom,view.pan,io.MouseWheel,canvasOrigin(),.25f,5.0f);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) view.pan = add(view.pan,io.MouseDelta);
    auto* artworkOffset = stage ? stageArtworkOffset(*stage) : nullptr;
    if (artworkOffset) {
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            stageArtworkDrag_ = SpriteDrag{io.MousePos,artworkOffset->x,artworkOffset->y};
        if (stageArtworkDrag_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsMousePosValid()) {
            auto& drag = *stageArtworkDrag_;
            const auto delta = mul(sub(io.MousePos,drag.mouseStart),1/view.zoom);
            constexpr int limit = stageArtworkOffsetLimit;
            const int x = std::clamp(drag.originX+int(std::lround(delta.x)),-limit,limit);
            const int y = std::clamp(drag.originY+int(std::lround(delta.y)),-limit,limit);
            if (x != artworkOffset->x || y != artworkOffset->y) {
                if (!drag.changed) checkpoint();
                drag.changed = true;
                *artworkOffset = {x,y};
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) stageArtworkDrag_.reset();
        if (const auto [dx,dy] = viewerNudge(!stageArtworkDrag_); dx || dy) {
            constexpr int limit = stageArtworkOffsetLimit;
            nudgePosition({artworkOffset->x,-limit,limit},{artworkOffset->y,-limit,limit},dx,dy,[&] { checkpoint(); });
        }
        if (stageSelection_ == StageSelection::panel) {
            layout = stageArtLayout(*stage,sizes);
            camera_ = std::clamp(camera_,0,std::max(0,layout.width()-screenWidth));
        }
    }
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
    if (layout.width() == 0) {
        label(draw,add(start,{18,20}),"Add a panel image to define the stage width",dim);
    }

    const int width = std::max(screenWidth,layout.width()), artHeight = std::max(screenHeight,layout.panelHeight);
    const int cameraZoom = std::clamp(stage->camera.zoom,cameraZoomMin,cameraZoomMax);
    const int shotWidth = ((screenWidth*100/cameraZoom+3)/4)*4;
    const int shotHeight = screenHeight*100/cameraZoom;
    const int baseX = std::clamp(camera_+std::clamp(stage->camera.offsetX,-cameraOffsetLimit,cameraOffsetLimit),0,maxCamera);
    const int baseY = std::clamp(stage->camera.offsetY,0,std::max(0,artHeight-screenHeight));
    const float shotX = float(baseX-(shotWidth-screenWidth)/2);
    const float shotY = float(baseY+screenHeight-shotHeight);
    const auto shotStart = point(shotX,shotY);
    const auto shotEnd = point(shotX+shotWidth,shotY+shotHeight);
    const auto artEnd = point(float(width),float(artHeight));
    draw->PushClipRect(origin,artEnd,true);
    std::optional<std::pair<ImVec2,ImVec2>> artOutline;
    if (const auto* back = images_.texture(stage->background,true)) {
        const int offsetX = std::clamp(stage->backgroundOffset.x,-stageArtworkOffsetLimit,stageArtworkOffsetLimit);
        const int offsetY = std::clamp(stage->backgroundOffset.y,-stageArtworkOffsetLimit,stageArtworkOffsetLimit);
        const int cameraShift = stage->repeatBackgroundX ? offsetX-baseX/2 : int(offsetX-baseX*.5);
        const float left = float(baseX+cameraShift), top = float(offsetY);
        const auto a = point(left,top);
        const auto b = point(left+back->width,top+back->height);
        const ImVec2 begin{stage->repeatBackgroundX ? std::max(origin.x,start.x) : std::max(a.x,start.x),
                           stage->repeatBackgroundY ? std::max(origin.y,start.y) : std::max(a.y,start.y)};
        const ImVec2 finish{stage->repeatBackgroundX ? std::min(artEnd.x,end.x) : std::min(b.x,end.x),
                            stage->repeatBackgroundY ? std::min(artEnd.y,end.y) : std::min(b.y,end.y)};
        if (begin.x < finish.x && begin.y < finish.y && stageBackgroundSampler_) {
            const auto uv = [&](ImVec2 pos) { return ImVec2{(pos.x-a.x)/(b.x-a.x),(pos.y-a.y)/(b.y-a.y)}; };
            draw->AddCallback([](const ImDrawList*,const ImDrawCmd* cmd) {
                auto* state = static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
                auto* sampler = static_cast<ID3D11SamplerState*>(cmd->UserCallbackData);
                state->DeviceContext->PSSetSamplers(0,1,&sampler);
            },stageBackgroundSampler_.Get());
            draw->AddImage(ImTextureID(reinterpret_cast<uintptr_t>(back->view.Get())),begin,finish,uv(begin),uv(finish));
            draw->AddCallback(ImDrawCallback_ResetRenderState,nullptr);
        }
        if (stageSelection_ == StageSelection::background) artOutline = std::pair{a,b};
    }
    for (size_t i = 0; i < stage->panels.size(); ++i) {
        const auto tex = panels[i];
        if (!tex) continue;
        const auto [x,y] = layout.positions[i];
        image(draw,tex,point(float(x),float(y)),point(float(x+tex->width),float(y+tex->height)));
    }
    draw->PopClipRect();
    for (size_t i = 0; i < panels.size(); ++i) {
        if (!panels[i]) continue;
        const auto [x,y] = layout.positions[i];
        const auto a = point(float(x),float(y)), b = point(float(x+sizes[i].width),float(y+sizes[i].height));
        draw->AddRect(a,b,guideColor);
        if (stageSelection_ == StageSelection::panel && int(i) == panel_) artOutline = std::pair{a,b};
        const auto id = std::string("Panel ")+char('A'+i);
        label(draw,add(a,{7,8}),id.c_str(),dim);
    }
    if (artOutline) draw->AddRect(artOutline->first,artOutline->second,accent,0,0,2);
    draw->AddRectFilled(point(0,float(stage->laneTop)),point(float(width),float(stage->laneBottom)),IM_COL32(85,195,151,29));
    for (const int lane : {stage->laneTop,stage->laneBottom}) {
        draw->AddLine(point(0,float(lane)),point(float(width),float(lane)),(bodyColor&0xffffff)|0x8a000000);
    }
    drawStageTerrain(*stage,draw,origin,view.zoom,hovered);
    const auto spawnPoint = [&](const StageSpawn& entry) {
        return point(float(entry.at)+entry.x,float(entry.z)-entry.altitude);
    };
    if (!stageCameraPreview_ && !artworkOffset && stageSelection_ != StageSelection::terrain && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        int target = -1;
        for (int i = int(stage->spawns.size())-1; i >= 0; --i) {
            if (nearMarker(io.MousePos,spawnPoint(stage->spawns[size_t(i)]),12)) { target = i; break; }
        }
        if (target >= 0) {
            spawn_ = target;
            stageSelection_ = StageSelection::spawn;
            dragSpawn_ = spawn_;
            spawnDragOffset_ = sub(spawnPoint(stage->spawns[size_t(spawn_)]),io.MousePos);
            spawnDragChanged_ = false;
        } else if (nearMarker(io.MousePos,point(float(stage->playerX),float(stage->playerZ)),10)) {
            stageSelection_ = StageSelection::player;
        } else if (placeSpawn_) {
            stageSelection_ = StageSelection::spawn;
            dragSpawn_ = spawn_;
            spawnDragOffset_ = {};
            checkpoint();
            spawnDragChanged_ = true;
        } else {
            stageSelection_ = StageSelection::none;
        }
    }
    if (dragSpawn_ >= 0 && dragSpawn_ < int(stage->spawns.size()) && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        (spawnDragChanged_ || ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
        auto& moving = stage->spawns[size_t(dragSpawn_)];
        const auto world = mul(sub(add(io.MousePos,spawnDragOffset_),origin),1/view.zoom);
        const auto x = int(std::lround(world.x)) - moving.at;
        const auto z = int(std::lround(world.y)) + moving.altitude;
        if (x != moving.x || z != moving.z) {
            if (!spawnDragChanged_) checkpoint();
            spawnDragChanged_ = true;
            moving.x = x;
            moving.z = z;
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragSpawn_ = -1;

    const bool spawnSelected = stageSelection_ == StageSelection::spawn && !stage->spawns.empty();
    const bool playerSelected = stageSelection_ == StageSelection::player;
    if (const auto [dx,dy] = viewerNudge(dragSpawn_ < 0 && (spawnSelected || playerSelected)); dx || dy) {
        const auto beforeMove = [&] { checkpoint(); };
        if (playerSelected) {
            nudgePosition({stage->playerX,0,319},{stage->playerZ,stage->laneTop,stage->laneBottom},dx,dy,beforeMove);
        } else {
            auto& spawn = stage->spawns[size_t(spawn_)];
            nudgePosition({spawn.x,-99999,99999},{spawn.z,-99999,99999},dx,dy,beforeMove);
        }
    }
    const auto player = point(float(stage->playerX),float(stage->playerZ));
    const auto playerColor = playerSelected ? accent : bodyColor;
    if (stageCameraPreview_) {
        const auto model = std::find_if(project_.characters.begin(),project_.characters.end(),[](const Character& entry) {
            return entry.type == "player" && hasFrames(entry);
        });
        if (model != project_.characters.end())
            sprite(draw,images_,*previewFrame(&*model),player,view.zoom,true);
    }
    cross(draw,player,playerColor,playerSelected ? 8.0f : 5.0f);
    label(draw,add(player,{8,9}),"Player start",playerColor);

    for (const auto& wave : stage->waves) {
        if (!wave.wait) continue;
        draw->AddLine(point(float(wave.at),0),point(float(wave.at),240),itemColor,1.3f);
        label(draw,add(point(float(wave.at),240),{6,26}),"Wait",itemColor);
    }
    for (size_t i = 0; i < stage->spawns.size(); ++i) {
        const auto& entry = stage->spawns[i];
        const auto base = point(float(entry.at)+entry.x,float(entry.z));
        const auto top = spawnPoint(entry);
        if (entry.altitude) draw->AddLine(base,top,guideColor,1.5f);
        const auto model = findCharacter(project_,entry.model);
        if (const auto frame = previewFrame(model)) sprite(draw,images_,*frame,top,view.zoom,!entry.flip);
        const bool selected = spawnSelected && i==size_t(spawn_);
        cross(draw,top,selected ? accent : shotColor,selected ? 8.0f : 5.0f);
        const std::string name = model ? model->title : entry.model.empty() ? "Unassigned" : entry.model;
        if (selected) {
            draw->AddLine(point(float(entry.at),0),point(float(entry.at),240),(accent&0xffffff)|0x85000000,1.4f);
        }
        label(draw,add(top,{8,8}),name.c_str(),selected ? accent : ink);
    }
    if (stageCameraPreview_) {
        constexpr auto shade = IM_COL32(4,8,15,178);
        const float top = std::clamp(shotStart.y,start.y,end.y);
        const float bottom = std::clamp(shotEnd.y,start.y,end.y);
        draw->AddRectFilled(start,{end.x,top},shade);
        draw->AddRectFilled({start.x,bottom},end,shade);
        draw->AddRectFilled({start.x,top},{std::clamp(shotStart.x,start.x,end.x),bottom},shade);
        draw->AddRectFilled({std::clamp(shotEnd.x,start.x,end.x),top},{end.x,bottom},shade);
    }
    draw->AddRect(shotStart,shotEnd,shotColor,0.0f,0,stageCameraPreview_ ? 2.0f : 1.3f);
    const auto cameraLabel = std::to_string(shotWidth)+" x "+std::to_string(shotHeight)+"  ·  "+std::to_string(cameraZoom)+"% camera";
    label(draw,add({std::max(start.x,shotStart.x),std::max(start.y,shotStart.y)},{6,8}),cameraLabel.c_str(),shotColor);
    const auto scale = std::to_string(layout.width())+" px  ·  "+std::to_string(int(std::lround(view.zoom*100)))+"%";
    label(draw,{start.x+13,end.y-28},scale.c_str(),dim);
    const char* hint = stageCameraPreview_ ? "Camera framing  |  Test Play to check gameplay"
        : stageSelection_ == StageSelection::terrain
        ? (terrainPlacement_ ? "Click to place  |  Esc to cancel" : "Drag terrain  |  Arrow keys nudge")
        : artworkOffset ? "Drag artwork  |  Arrow keys nudge" :
        spawnSelected || playerSelected ? "Arrow keys nudge selection" : "Click a marker to select";
    label(draw,{end.x-ImGui::CalcTextSize(hint).x-13,end.y-28},hint,dim);
    draw->PopClipRect();
}

void Editor::selectTerrain(int index) {
    stageCameraPreview_ = false;
    terrain_ = index;
    stageSelection_ = StageSelection::terrain;
    stageArtworkDrag_.reset();
    terrainDrag_.reset();
    terrainPlacement_.reset();
    dragSpawn_ = -1;
    placeSpawn_ = false;
    selectTerrainTab_ = true;
}

void Editor::drawStageTerrain(Stage& stage, ImDrawList* draw, ImVec2 origin, float zoom, bool hovered) {
    const auto screen = [&](float x, float y) { return add(origin,{x*zoom,y*zoom}); };
    const auto world = [&](ImVec2 position) {
        const auto p = mul(sub(position,origin),1/zoom);
        return ScreenPoint{int(std::lround(p.x)),int(std::lround(p.y))};
    };
    const bool sideView = stage.laneTop == stage.laneBottom;
    const auto plane = [&](const StageTerrain& shape) {
        return sideView && stage.laneTop >= shape.z-shape.depth && stage.laneTop <= shape.z ? stage.laneTop : shape.z;
    };
    if (stageSelection_ == StageSelection::terrain) {
        const auto mouse = ImGui::GetIO().MousePos;
        const bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (terrainPlacement_ && ImGui::IsKeyPressed(ImGuiKey_Escape,false)) terrainPlacement_.reset();
        if (terrainPlacement_ && clicked) {
            const auto at = world(mouse);
            StageTerrain shape;
            shape.pit = *terrainPlacement_;
            shape.x = std::clamp(at.x,0,stageTerrainLimit);
            const int lane = sideView ? stage.laneTop : std::clamp(at.y,stage.laneTop,stage.laneBottom);
            shape.z = lane+shape.depth/2;
            shape.altitude = shape.pit ? 0 : std::clamp(lane-at.y,1,stageTerrainLimit);
            checkpoint();
            stage.terrain.push_back(shape);
            selectTerrain(int(stage.terrain.size())-1);
        } else if (clicked) {
            for (int i = int(stage.terrain.size())-1; i >= 0; --i) {
                const auto& shape = stage.terrain[size_t(i)];
                const auto a = screen(float(shape.x),float(plane(shape)-shape.altitude));
                const auto b = screen(float(shape.x+shape.width),float(plane(shape)));
                if (mouse.x < a.x-6 || mouse.x > b.x+6 || mouse.y < a.y-7 || mouse.y > b.y+7) continue;
                selectTerrain(i);
                terrainDrag_ = TerrainDrag{world(mouse),shape};
                break;
            }
        }
        if (terrainDrag_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsMousePosValid()) {
            const auto at = world(mouse);
            auto& drag = *terrainDrag_;
            auto& shape = stage.terrain[size_t(terrain_)];
            auto next = drag.original;
            next.x = std::clamp(next.x+at.x-drag.start.x,-stageTerrainLimit,stageTerrainLimit);
            const int dy = at.y-drag.start.y;
            if (!next.pit) next.altitude = std::clamp(next.altitude-dy,1,stageTerrainLimit);
            else if (!sideView) next.z = std::clamp(next.z+dy,-stageTerrainLimit,stageTerrainLimit);
            if (next != shape) {
                if (!drag.changed) checkpoint();
                drag.changed = true;
                shape = next;
            }
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) terrainDrag_.reset();
        if (const auto [dx,dy] = viewerNudge(!terrainPlacement_ && !terrainDrag_ && !stage.terrain.empty()); dx || dy) {
            auto& shape = stage.terrain[size_t(terrain_)];
            nudgePosition({shape.x,-stageTerrainLimit,stageTerrainLimit},
                          shape.pit ? NumberField<int>{shape.z,-stageTerrainLimit,stageTerrainLimit} :
                                      NumberField<int>{shape.altitude,1,stageTerrainLimit},
                          dx,shape.pit ? (sideView ? 0 : dy) : -dy,[&] { checkpoint(); });
        }
    }
    for (size_t i=0; i<stage.terrain.size(); ++i) {
        const auto& shape = stage.terrain[i];
        const float z = float(plane(shape));
        const bool selected = stageSelection_ == StageSelection::terrain && int(i) == terrain_;
        const auto color = selected ? accent : shape.pit ? attackColor : bodyColor;
        const auto a = screen(float(shape.x),z-shape.altitude);
        const auto b = screen(float(shape.x+shape.width),z);
        if (shape.pit) {
            draw->AddRectFilled(add(a,{0,-3}),add({b.x,a.y},{0,8}),(color&0xffffff)|0x58000000);
            draw->AddLine(a,{b.x,a.y},color,3);
        } else {
            draw->AddRectFilled(a,b,(color&0xffffff)|0x32000000);
            draw->AddRect(a,b,color);
            draw->AddLine(a,{b.x,a.y},color,3);
        }
        if (!sideView && stageSelection_ == StageSelection::terrain) {
            draw->AddRect(screen(float(shape.x),float(shape.z-shape.depth)),
                          screen(float(shape.x+shape.width),float(shape.z)),(color&0xffffff)|0x65000000);
        }
        if (selected) {
            const auto name = std::string(shape.pit ? "Pit " : "Platform ")+std::to_string(i+1);
            label(draw,add(a,{5,-25}),name.c_str(),color);
        }
    }
}

void Editor::stageEvents(Stage* stage, float height) {
    (void)height;
    ImGui::BeginDisabled(!stage);
    if (filled("+ Spawn") && stage) {
        stageCameraPreview_ = false;
        checkpoint();
        auto& entry = stage->spawns.emplace_back();
        entry.at = camera_;
        entry.z = (stage->laneTop+stage->laneBottom)/2;
        for (const auto& model : project_.characters) {
            if (model.type == "enemy") { entry.model = model.id; break; }
        }
        if (entry.model.empty() && !project_.characters.empty()) entry.model = project_.characters.front().id;
        spawn_ = int(stage->spawns.size())-1;
        stageSelection_ = StageSelection::spawn;
        terrainDrag_.reset();
        terrainPlacement_.reset();
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
            stageCameraPreview_ = false;
            spawn_ = int(i);
            stageSelection_ = StageSelection::spawn;
            terrainDrag_.reset();
            terrainPlacement_.reset();
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

void Editor::stageArtworkInspector(Stage& stage) {
    auto* offset = stageArtworkOffset(stage);
    const auto panelName = [&] { return std::string("Panel ")+char('A'+panel_); };
    const auto selected = !offset ? std::string("Choose artwork") :
                          stageSelection_ == StageSelection::background ? std::string("Background") : panelName();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Artwork selection",selected.c_str())) {
        if (!stage.background.empty() && ImGui::Selectable("Background",stageSelection_ == StageSelection::background))
            selectStageArtwork(StageSelection::background);
        for (size_t i = 0; i < stage.panels.size(); ++i) {
            const auto title = std::string("Panel ")+char('A'+i)+"  "+utf8(stage.panels[i].image.filename());
            if (ImGui::Selectable(title.c_str(),stageSelection_ == StageSelection::panel && panel_ == int(i))) {
                panel_ = int(i);
                selectStageArtwork(StageSelection::panel);
            }
        }
        ImGui::EndCombo();
        offset = stageArtworkOffset(stage);
    }
    if (!offset) return;

    ImGui::TextUnformatted("X / Y offset (px)");
    constexpr int limit = stageArtworkOffsetLimit;
    auto value = *offset;
    ImGui::SetNextItemWidth(-1);
    if (numberFields<int>("##Artwork offset",{{value.x,-limit,limit},{value.y,-limit,limit}})) {
        checkpoint();
        *offset = {std::clamp(value.x,-limit,limit),std::clamp(value.y,-limit,limit)};
    }
    if (stageSelection_ == StageSelection::panel) {
        int seam = 0;
        for (int i = 0; i < panel_; ++i) {
            if (const auto* tex = images_.texture(stage.panels[size_t(i)].image)) seam += tex->width;
        }
        ImGui::Text("Stage position  %d / %d",seam+offset->x,offset->y);
        textDim("X offset starts at the panel's stitched position.");
    }
    if (ImGui::Button("Reset position",{-1,27}) && (offset->x || offset->y)) {
        checkpoint();
        *offset = {};
    }
    if (stageSelection_ == StageSelection::background) {
        bool repeatX = stage.repeatBackgroundX, repeatY = stage.repeatBackgroundY;
        const bool changedX = ImGui::Checkbox("Repeat horizontally",&repeatX);
        const bool changedY = ImGui::Checkbox("Repeat vertically",&repeatY);
        if (changedX || changedY) {
            checkpoint();
            stage.repeatBackgroundX = repeatX;
            stage.repeatBackgroundY = repeatY;
        }
    }
}

void Editor::stageTerrainInspector(Stage& stage) {
    if (project_.nativeEdits.contains(stage.nativeFile) || project_.nativeEdits.contains("data/levels.txt"))
        ImGui::TextWrapped("Native TXT overrides take precedence on export.");
    const bool sideView = stage.laneTop == stage.laneBottom;
    if (!sideView && ImGui::Button("Lock to side-view lane",{-1,28})) {
        checkpoint();
        if (stage.backgroundDepth < 0) stage.backgroundDepth = stage.laneBottom;
        stage.laneTop = stage.laneBottom = stage.playerZ = std::clamp(stage.playerZ,1,239);
    }
    if (stage.laneTop == stage.laneBottom) {
        int lane = stage.laneTop;
        ImGui::SetNextItemWidth(-102);
        if (numberFields<int>("Ground Z",{{lane,1,239}})) {
            checkpoint();
            const int shift = std::clamp(lane,1,239)-stage.laneTop;
            for (auto& shape : stage.terrain)
                if (stage.laneTop >= shape.z-shape.depth && stage.laneTop <= shape.z)
                    shape.z = std::clamp(shape.z+shift,-stageTerrainLimit,stageTerrainLimit);
            stage.laneTop = stage.laneBottom = stage.playerZ = std::clamp(lane,1,239);
        }
        labelTip("Up and down movement is locked to this lane. Set a Z range in the Stage tab to restore depth movement.");
    }

    const auto place = [&](const char* name, bool pit) {
        const bool active = terrainPlacement_ && *terrainPlacement_ == pit;
        if (toggleButton(name,active,{-1,27})) {
            selectTerrain(terrain_);
            if (!active) terrainPlacement_ = pit;
        }
    };
    place("+ Platform",false);
    place("+ Pit",true);
    if (terrainPlacement_)
        ImGui::TextWrapped("%s",*terrainPlacement_ ? "Click where the pit begins." : "Click the left end of the platform's top.");
    if (stage.terrain.empty()) return;

    const auto name = [&](size_t i) {
        return std::string(stage.terrain[i].pit ? "Pit " : "Platform ") + std::to_string(i+1);
    };
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##Terrain selection",name(size_t(terrain_)).c_str())) {
        for (size_t i=0; i<stage.terrain.size(); ++i)
            if (ImGui::Selectable(name(i).c_str(),int(i)==terrain_)) selectTerrain(int(i));
        ImGui::EndCombo();
    }
    auto next = stage.terrain[size_t(terrain_)];
    constexpr int limit = stageTerrainLimit;
    ImGui::TextUnformatted("X / width");
    ImGui::SetNextItemWidth(-1);
    bool edited = numberFields<int>("##Terrain width",{{next.x,-limit,limit},{next.width,1,limit}});
    ImGui::TextUnformatted("Z edge / depth");
    ImGui::SetNextItemWidth(-1);
    edited |= numberFields<int>("##Terrain depth",{{next.z,-limit,limit},{next.depth,1,limit}});
    ImGui::SetNextItemWidth(-102);
    edited |= numberFields<int>(next.pit ? "Pit rim altitude" : "Top altitude",{{next.altitude,next.pit ? 0 : 1,limit}});
    if (edited) {
        checkpoint();
        next.x = std::clamp(next.x,-limit,limit);
        next.z = std::clamp(next.z,-limit,limit);
        next.width = std::clamp(next.width,1,limit);
        next.depth = std::clamp(next.depth,1,limit);
        next.altitude = std::clamp(next.altitude,next.pit ? 0 : 1,limit);
        stage.terrain[size_t(terrain_)] = next;
        if (stageSelection_ != StageSelection::terrain) selectTerrain(terrain_);
    }
    if (stage.laneTop == stage.laneBottom &&
        (stage.laneTop > next.z || stage.laneTop < next.z-next.depth))
        ImGui::TextWrapped("This shape is outside the side-view lane.");
    if (ImGui::Button("Center camera on selection",{-1,26}))
        camera_ = std::max(0,next.x+next.width/2-stageVideoSize(project_).width/2);
    if (ImGui::Button(next.pit ? "Remove pit" : "Remove platform",{-1,26})) {
        checkpoint();
        stage.terrain.erase(stage.terrain.begin()+terrain_);
        selectTerrain(clampIndex(terrain_,stage.terrain.size()));
    }
}

void Editor::stageCameraInspector(Stage& stage) {
    auto camera = stage.camera;
    const bool nativeText = !stage.nativeFile.empty() && project_.nativeEdits.contains(stage.nativeFile);
    if (nativeText) textDim("This stage uses a Native TXT override.");
    ImGui::BeginDisabled(nativeText);
    ImGui::TextUnformatted("Game camera zoom");
    ImGui::SetNextItemWidth(-1);
    numberFields<int>("##Game camera zoom",{{camera.zoom,cameraZoomMin,cameraZoomMax}},1,"%d%%",true);
    labelTip("Lower values show more of the stage. Higher values move closer.");
    if (ImGui::Button("Wide")) camera.zoom = 75;
    ImGui::SameLine();
    if (ImGui::Button("Normal")) camera.zoom = 100;
    ImGui::SameLine();
    if (ImGui::Button("Close")) camera.zoom = 150;
    ImGui::Spacing();
    ImGui::TextUnformatted("Camera offset X / Y");
    ImGui::SetNextItemWidth(-1);
    numberFields<int>("##Camera offset",{{camera.offsetX,-cameraOffsetLimit,cameraOffsetLimit},
                                         {camera.offsetY,-cameraOffsetLimit,cameraOffsetLimit}});
    labelTip("Shifts the engine's scrolling focus within the stage limits.");
    ImGui::Checkbox("Follow jump height",&camera.followAltitude);
    ImGui::TextUnformatted("Zoom mode");
    ImGui::SetNextItemWidth(-1);
    int mode = camera.nativeZoom ? 1 : 0;
    if (ImGui::Combo("##Zoom mode",&mode,"Stock OpenBOR / 2D\0OpenBOR3D / GLB\0")) camera.nativeZoom = mode == 1;
    if (camera.nativeZoom) textDim("Link the updated OpenBOR3D build for GLB camera zoom.");
    else if (camera.zoom != 100) textDim("2D zoom uses a stage script. Wide views can show black edges.");
    if (ImGui::Button("Reset camera",{-1,26})) camera = {};
    ImGui::EndDisabled();
    if (camera != stage.camera) {
        camera.zoom = std::clamp(camera.zoom,cameraZoomMin,cameraZoomMax);
        camera.offsetX = std::clamp(camera.offsetX,-cameraOffsetLimit,cameraOffsetLimit);
        camera.offsetY = std::clamp(camera.offsetY,-cameraOffsetLimit,cameraOffsetLimit);
    }
    if (camera != stage.camera) {
        checkpoint();
        stage.camera = camera;
        stageCameraPreview_ = true;
        stageSelection_ = StageSelection::none;
        placeSpawn_ = false;
        dragSpawn_ = -1;
        stageArtworkDrag_.reset();
        terrainDrag_.reset();
        terrainPlacement_.reset();
    }
    ImGui::Checkbox("Show camera framing",&stageCameraPreview_);
}

void Editor::stageInspector(Stage* stage) {
    section("Inspector");
    if (!stage) { textDim("Select or create a stage."); return; }
    if (!ImGui::BeginTabBar("Stage settings",inspectorTabs)) return;
    if (ImGui::BeginTabItem("Spawn")) {
        if (!stage->spawns.empty()) {
            auto& spawn = stage->spawns[size_t(spawn_)];
            ImGui::Text("Spawn %d of %d",spawn_+1,int(stage->spawns.size()));
            chooseModel("##Stage spawn model",spawn.model,false);
            ImGui::SetNextItemWidth(-100);
            if (numberFields<int>("Trigger at",{{spawn.at,0,999999}})) dirty_ = true;
            ImGui::TextUnformatted("Screen X / Z / altitude");
            ImGui::SetNextItemWidth(-1);
            if (numberFields<int>("##Coordinates",{{spawn.x,-99999,99999},{spawn.z,-99999,99999},
                                                    {spawn.altitude,-99999,99999}})) dirty_ = true;
            ImGui::TextWrapped("Preview world X = %d",spawn.at+spawn.x);
            if (ImGui::Checkbox("Flip facing",&spawn.flip)) dirty_ = true;
            ImGui::SameLine();
            if (ImGui::Checkbox("Boss",&spawn.boss)) dirty_ = true;
            if (ImGui::CollapsingHeader("Entity overrides")) {
                textDim("0 health or MP uses the model's value.");
                ImGui::SetNextItemWidth(-100);
                if (numberFields<int>("Health",{{spawn.health,0,100000}})) dirty_ = true;
                ImGui::SetNextItemWidth(-100);
                if (numberFields<int>("MP",{{spawn.mp,0,100000}})) dirty_ = true;
                const auto* model = findCharacter(project_,spawn.model);
                const int maxPalette = model && !model->colorSets.empty() ? int(model->colorSets.size())-1 : 0;
                ImGui::SetNextItemWidth(-100);
                if (numberFields<int>("Palette",{{spawn.palette,-1,maxPalette}})) dirty_ = true;
                labelTip("-1 uses the engine default. 0 uses the base palette.");
            }
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
    if (ImGui::BeginTabItem("Artwork",nullptr,selectStageArtworkTab_ ? ImGuiTabItemFlags_SetSelected : 0)) {
        stageArtworkInspector(*stage);
        ImGui::EndTabItem();
    }
    selectStageArtworkTab_ = false;
    if (ImGui::BeginTabItem("Terrain",nullptr,selectTerrainTab_ ? ImGuiTabItemFlags_SetSelected : 0)) {
        stageTerrainInspector(*stage);
        ImGui::EndTabItem();
    }
    selectTerrainTab_ = false;
    if (ImGui::BeginTabItem("Camera",nullptr,selectStageCameraTab_ ? ImGuiTabItemFlags_SetSelected : 0)) {
        stageCameraInspector(*stage);
        ImGui::EndTabItem();
    }
    selectStageCameraTab_ = false;
    if (ImGui::BeginTabItem("Stage")) {
        ImGui::TextUnformatted("Stage title");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##Stage title",&stage->title)) dirty_ = true;
        int lane[] = {stage->laneTop,stage->laneBottom};
        ImGui::TextUnformatted("Walkable Z range");
        ImGui::SetNextItemWidth(-1);
        if (numberFields<int>("##Walkable lane",{{lane[0],1,239},{lane[1],std::max(1,stage->laneTop),239}})) {
            stage->laneTop=std::clamp(lane[0],1,239);
            stage->laneBottom=std::clamp(lane[1],stage->laneTop,239);
            stage->playerZ=std::clamp(stage->playerZ,stage->laneTop,stage->laneBottom);
            dirty_ = true;
        }
        int player[] = {stage->playerX,stage->playerZ};
        ImGui::TextUnformatted("Player start X / Z");
        ImGui::SetNextItemWidth(-1);
        if (numberFields<int>("##Player start",{{player[0],0,319},{player[1],stage->laneTop,stage->laneBottom}})) {
            stage->playerX=std::clamp(player[0],0,319);
            stage->playerZ=std::clamp(player[1],stage->laneTop,stage->laneBottom);
            dirty_ = true;
        }
        if (ImGui::CollapsingHeader("Stage commands")) {
            if (ImGui::Button("Edit stage commands",{-1,27})) openCommands(CommandContext::stage);
        }
        if (ImGui::Button("Edit / preview stage TXT",{-1,29})) openCommands(CommandContext::stage);
        if (ImGui::Button("Remove stage",{-1,27})) {
            checkpoint();
            project_.stages.erase(project_.stages.begin()+stage_);
            stage_ = clampIndex(stage_,project_.stages.size());
            camera_ = spawn_ = panel_ = 0;
            stageSelection_ = StageSelection::none;
            stageArtworkDrag_.reset();
            terrainDrag_.reset();
            terrainPlacement_.reset();
            terrain_ = 0;
        }
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Media")) {
        section("Stage music");
        pickMedia(stage->music,"Choose stage music");
        if (!stage->music.empty()) {
            auto offset = stage->musicOffset;
            ImGui::SetNextItemWidth(-105);
            if (numberFields<int>("Loop offset",{{offset,0,2147483647}})) {
                checkpoint();
                stage->musicOffset = offset;
            }
            labelTip("Loop start in sample frames. 0 restarts at the beginning.");
        }
        ImGui::Separator();
        section("Before stage");
        sceneInspector(stage->before,"Choose opening cutscene");
        ImGui::Separator();
        section("After stage");
        sceneInspector(stage->after,"Choose ending cutscene");
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}

} // namespace studio

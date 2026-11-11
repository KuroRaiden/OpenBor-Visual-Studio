#pragma once
#include "images.hpp"
#include "imgui.h"
#include <windows.h>
#include <optional>

namespace studio {

enum class EditTool { select, origin, body, attack, projectile, item };
enum class OriginMode { placePivot, dragSprite };

class Editor {
public:
    Editor(HWND window, ID3D11Device* device, const fs::path& sample);
    void draw();
    void setCaptureView(bool stage);
    void requestClose() { replace("Close"); }
private:
    struct View { float zoom; ImVec2 pan{}; };
    struct SpriteDrag {
        ImVec2 mouseStart;
        int originX, originY;
        bool changed = false;
    };
    HWND window_;
    Images images_;
    Project project_;
    fs::path file_, sample_;
    std::vector<Project> undo_, redo_;
    std::string status_, renameInput_;
    bool dirty_ = false, playing_ = false, showGhost_ = true, showGrid_ = true, facingLeft_ = false;
    bool showOnionSkin_ = false;
    bool stageMode_ = false, placeSpawn_ = false, importDialog_ = false, confirmReplace_ = false, previewOpen_ = false;
    int model_ = 0, animation_ = 0, frame_ = 0, stage_ = 0, spawn_ = 0, panel_ = 0, importFacing_ = 0;
    int camera_ = 0, dragSpawn_ = -1;
    float playbackSeconds_ = 0, cueAge_ = -1.0f, itemAge_ = -1.0f;
    float playbackSpeed_ = 1.0f;
    View characterView_{4.0f}, stageView_{1.0f};
    EditTool tool_ = EditTool::select;
    OriginMode originMode_ = OriginMode::placePivot;
    std::optional<SpriteDrag> spriteDrag_;
    ImVec2 dragFrom_{};
    bool dragging_ = false, selectSpawnTab_ = false;
    std::string pending_;
    void checkpoint();
    void history(bool forward);
    void normalizeSelection();
    void resetPlayback(bool play = false);
    void setModel(int index);
    void setAnimation(int index);
    void newModel();
    void addAnimation();
    void newStage();
    void load(const fs::path& path, bool asSample);
    void save(bool as);
    void replace(std::string action);
    void executeReplace(const std::string& action);
    void addFrames();
    void addPanels();
    void insertModel(Character character);
    void importModel();
    void tick(Animation& animation);
    void drawHeader();
    void drawStatus();
    void drawModals();
    void characterWorkspace(float left, float right, float height);
    void stageWorkspace(float left, float right, float height);
    void characterLibrary();
    void frameInspector(Character& character, Animation* animation);
    void characterCanvas(Animation* animation, float height);
    void timeline(Animation* animation, float height);
    void stageLibrary();
    void stageCanvas(Stage* stage, float height);
    void stageEvents(Stage* stage, float height);
    void stageInspector(Stage* stage);
    void drawCue(SpawnCue& cue, const char* label, bool projectile, Animation& animation);
    bool chooseModel(const char* label, std::string& id, bool emptyAllowed);
    void previewText(const std::string& text);
    std::string textPreview_;
};

void configureStyle();
}


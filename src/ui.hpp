#pragma once
#include "mesh.hpp"
#include "commands.hpp"
#include "jump.hpp"
#include "recovery.hpp"
#include "playtest.hpp"
#include "imgui.h"
#include <windows.h>
#include <optional>

namespace studio {

enum class EditTool { select, origin, body, attack, subEntity, item, sprite };
enum class OriginMode { placePivot, dragSprite };

class Editor {
public:
    Editor(HWND window, ID3D11Device* device, const fs::path& sample);
    void draw();
    void setCaptureView(bool stage);
    void updateBackgroundTasks();
    void initializeSession();
    void requestClose() { replace("Close"); }
private:
    enum class StageSelection { none, spawn, player, background, panel, terrain };
    struct TerrainDrag {
        ScreenPoint start;
        StageTerrain original;
        bool changed = false;
    };
    struct View { float zoom; ImVec2 pan{}; };
    struct InspectorLayout { float preview, width, minimum, maximum; };
    struct SpriteDrag {
        ImVec2 mouseStart;
        int originX, originY;
        bool changed = false;
    };
    struct ScreenFontPreview {
        ScreenFont settings;
        Texture texture;
        std::array<int,256> advances{};
    };
    struct MeshPreview {
        MeshSettings settings;
        MeshView view;
        int clip;
        float phase;
        Texture texture;
    };
    HWND window_;
    Images images_;
    ComPtr<ID3D11SamplerState> stageBackgroundSampler_;
    Project project_;
    Recovery recovery_;
    Playtest playtest_;
    std::string testPlayer_, testError_;
    bool showPlaytest_ = false;
    std::chrono::steady_clock::time_point nextAutosave_ = std::chrono::steady_clock::now()+Recovery::interval;
    fs::path recoveryOffer_;
    std::string recoveryError_;
    bool showRecovery_ = false;
    fs::path file_, sample_;
    std::vector<Project> undo_, redo_;
    std::string status_, renameInput_;
    bool dirty_ = false, playing_ = false, showGhost_ = true, showGrid_ = true, facingLeft_ = false;
    bool showOnionSkin_ = false;
    bool stageMode_ = false, placeSpawn_ = false, importDialog_ = false, confirmReplace_ = false;
    bool moduleMode_ = false, showExport_ = false, playableExport_ = false;
    fs::path exportedTo_;
    fs::path exportTarget_;
    ExportDestination exportDestination_ = ExportDestination::newFolder;
    std::string exportError_;
    int paletteIndex_ = 0, paletteColor_ = 1, layoutSelection_ = 0;
    bool layoutSelectMode_ = false, layoutDragging_ = false, layoutChanged_ = false;
    ImVec2 layoutGrab_{};
    std::array<std::optional<ScreenFontPreview>,screenFontNames.size()> screenFontPreviews_;
    std::array<std::optional<Texture>,2> screenBackgroundPreviews_;
    int screenPreview_ = 1, screenRow_ = 0;
    float sceneTime_ = 0;
    bool scenePlaying_ = false;
    std::optional<SpriteDrag> sceneDrag_;
    fs::path glbPath_;
    std::unique_ptr<GlbModel> glb_;
    std::optional<MeshPreview> meshPreview_;
    bool selectGlbTab_ = false;
    bool revealModel_ = false, revealAnimation_ = false;
    bool revealFrame_ = false;
    CommandContext commandContext_ = CommandContext::model;
    std::string commandDraft_, commandFilter_, commandReference_, commandGenerated_;
    std::string nativeSelection_, nativeDraft_, nativeSaved_, nativeFilter_, nativeError_;
    int commandCursor_ = 0;
    bool commandOpen_ = false, commandShowGenerated_ = false;
    bool nativeOpen_ = false;
    int model_ = 0, animation_ = 0, frame_ = 0, stage_ = 0, spawn_ = 0, panel_ = 0, importFacing_ = 0;
    int camera_ = 0, dragSpawn_ = -1;
    float playbackSeconds_ = 0, cueAge_ = -1.0f, itemAge_ = -1.0f;
    float playbackSpeed_ = 1.0f;
    JumpMode jumpMode_ = JumpMode::forward;
    int jumpStage_ = -1;
    float jumpTime_ = 0;
    std::optional<float> jumpGravity_;
    bool jumpOpen_ = false, jumpPlaying_ = false;
    std::optional<float> inspectorWidth_;
    float inspectorDragWidth_ = 0;
    View characterView_{4.0f}, stageView_{1.0f};
    EditTool tool_ = EditTool::select;
    EditTool characterSelection_ = EditTool::select;
    StageSelection stageSelection_ = StageSelection::spawn;
    std::optional<SpriteDrag> stageArtworkDrag_;
    std::optional<TerrainDrag> terrainDrag_;
    std::optional<bool> terrainPlacement_;
    int terrain_ = 0;
    bool selectTerrainTab_ = false;
    bool selectStageArtworkTab_ = false;
    bool selectStageCameraTab_ = false;
    bool stageCameraPreview_ = false;
    OriginMode originMode_ = OriginMode::placePivot;
    std::optional<SpriteDrag> spriteDrag_;
    ImVec2 dragFrom_{}, spawnDragOffset_{};
    bool spawnDragChanged_ = false;
    bool dragging_ = false, selectSpawnTab_ = false;
    std::string pending_;
    void checkpoint();
    void history(bool forward);
    void normalizeSelection();
    void resetPlayback(bool play = false);
    void stepFrame(const Animation& animation, int direction);
    void setModel(int index);
    void setAnimation(int index);
    void newModel();
    void addAnimation();
    void newStage();
    void load(const fs::path& path, bool asSample);
    void adoptProject(Project project, const fs::path& file, bool asSample);
    void save(bool as);
    void replace(std::string action);
    void executeReplace(const std::string& action);
    void addFrames();
    void addPanels();
    void insertModel(Character character);
    void importModel();
    void importGlb();
    GlbModel* meshAsset(const fs::path& path);
    void meshInspector(Character& model, Animation* animation);
    void drawMesh(ImDrawList* draw, const MeshSettings& settings, const Animation& animation,
                  ImVec2 start, ImVec2 size, ImVec2 origin, float zoom, size_t frameIndex);
    void tick(Animation& animation);
    void drawHeader();
    void drawStatus();
    void drawModals();
    void recoveryDialog();
    void playtestDialog();
    void openCommands(CommandContext context);
    void commandsDialog();
    void nativeDialog();
    void jumpSimulator();
    void jumpCanvas(const Character& model, const Animation* animation, size_t frameIndex,
                    const JumpArc& arc, const JumpPoint& point);
    void selectNativeFile(const std::string& name);
    std::string* commandText(Project& project, CommandContext context) const;
    void exportDialog();
    void moduleWorkspace(float height);
    void screensEditor(float height);
    void layoutEditor(bool select, float height);
    void drawEffects(Frame& frame);
    void paletteEditor(Character& model);
    void entityEditor(Character& model);
    void chooseArt(fs::path& path, const char* label);
    bool pickMedia(fs::path& path, const char* label, bool scene = false);
    bool sceneInspector(ScreenScene& scene, const char* label);
    InspectorLayout inspectorLayout() const;
    void inspectorDivider(const InspectorLayout& layout, float height);
    void characterWorkspace(float left, float height);
    void stageWorkspace(float left, float height);
    void characterLibrary();
    void animationLibrary();
    void frameInspector(Character& character, Animation* animation);
    void characterCanvas(Animation* animation, float height);
    void timeline(Animation* animation, float height);
    void stageLibrary();
    void stageCanvas(Stage* stage, float height);
    void stageEvents(Stage* stage, float height);
    void stageInspector(Stage* stage);
    void selectStageArtwork(StageSelection selection);
    ScreenPoint* stageArtworkOffset(Stage& stage);
    void stageArtworkInspector(Stage& stage);
    void stageTerrainInspector(Stage& stage);
    void stageCameraInspector(Stage& stage);
    void drawStageTerrain(Stage& stage, ImDrawList* draw, ImVec2 origin, float zoom, bool hovered);
    void selectTerrain(int index);
    void drawCue(SpawnCue& cue, const char* label, EditTool mode, Animation& animation);
    bool chooseModel(const char* label, std::string& id, bool emptyAllowed);
};

void configureStyle();
}


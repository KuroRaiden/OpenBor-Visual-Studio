#pragma once

#include <span>
#include <string_view>

namespace studio {

enum class CommandContext { model, animation, frame, stage, models, levels, levelSet, video };

struct CommandHelp {
    std::string_view name, syntax, description;
};

inline std::span<const CommandHelp> commandHelp(CommandContext context) {
    static constexpr CommandHelp model[]{
        {"name","name <model_id>","ID used by model registrations and spawn commands."},
        {"type","type <player|enemy|npc|item|obstacle|shot|none>","Sets the entity's gameplay type."},
        {"health","health <points>","Maximum health or item health value."},
        {"mp","mp <points>","Maximum magic points."},
        {"speed","speed <value>","Ground movement speed."},
        {"jumpheight","jumpheight <value>","Vertical launch velocity for a standing or forward jump."},
        {"jumpspeedf","jumpspeedf <value>","Horizontal launch velocity for a forward jump."},
        {"run.jump.x","run.jump.x <multiplier>","Multiplier applied to the forward jump speed during a running jump."},
        {"run.jump.y","run.jump.y <value>","Vertical launch velocity for a running jump."},
        {"subject_to_wall","subject_to_wall <0|1>","Controls whether the entity collides with walls."},
        {"subject_to_gravity","subject_to_gravity <0|1>","Controls whether gravity affects the entity."},
        {"atchain","atchain <attack_number> [attack_number ...]","Order of basic attack animations in a chain."},
        {"icon","icon <path> [palette]","Sets the HUD portrait sprite."},
        {"diesound","diesound <path>","Plays a sample when this model dies."},
        {"alternatepal","alternatepal <path>","Registers a selectable alternate palette."},
        {"meshmodel","meshmodel <glb_path> <scale> [yaw [planar]]","OpenBOR3D character mesh. Studio writes this from the GLB inspector."},
        {"anim","anim <animation_id>","Starts an OpenBOR animation block. Add editable animations in the character workspace."}
    };
    static constexpr CommandHelp animation[]{
        {"loop","loop <0|1>","Controls whether the animation repeats."},
        {"meshclip","meshclip <exact_clip_name>","OpenBOR3D GLB clip for this animation. Studio writes the mapping from the GLB inspector."},
        {"range","range <minimum_x> <maximum_x>","Horizontal range used to select this attack."},
        {"rangez","rangez <minimum_z> <maximum_z>","Depth range used to select this attack."},
        {"subentity","subentity <model_id>","Chooses the entity created by spawnframe. Its model defines movement and attacks."},
        {"spawnframe","spawnframe <frame_index> <x> <z> <altitude> [placement]","Spawns the chosen entity on a zero-based frame. Placement 0 offsets from the parent and inherits its facing."},
        {"sound","sound <path>","Plays a sample when the animation reaches the following frame."}
    };
    static constexpr CommandHelp frame[]{
        {"delay","delay <centiseconds>","Time before advancing to the next frame."},
        {"offset","offset <x> <y>","Origin of the sprite in image pixels."},
        {"bbox","bbox <x> <y> <width> <height> [depth]","Defines the body collision box."},
        {"attack","attack <x> <y> <width> <height> <damage> [drop] [block] [flash] [pause] [depth]","Defines an attack box. Use attack 0 to clear it."},
        {"move","move <x>","Moves the entity along its facing direction on this frame."},
        {"movez","movez <z>","Moves the entity along stage depth on this frame."},
        {"movea","movea <altitude>","Changes the entity's altitude on this frame."},
        {"sound","sound <path>","Plays a sound when this frame starts."},
        {"alphamask","alphamask <path>","Supplies a grayscale alpha mask for the following image."},
        {"drawmethod","drawmethod <scale_x> <scale_y> <flip_x> <flip_y> <shift_x> <alpha> <remap> <fill> <rotate> <fliprotate>","Sets sprite drawing properties."},
        {"nodrawmethod","nodrawmethod","Resets sprite drawing properties."},
        {"frame","frame <path>","Commits the frame with the preceding properties. Studio generates this line."}
    };
    static constexpr CommandHelp stage[]{
        {"background","background <path> [x_ratio] [z_ratio] [x] [y] [x_spacing] [y_spacing] [x_repeat] [y_repeat]","Loads and positions the stage background image. Use -1 to repeat along an axis or 1 for one copy."},
        {"panel","panel <normal> <neon|none> <screen|none>","Declares a stage panel and optional overlay images."},
        {"order","order <panel_sequence>","Sets the order of the declared stage panels."},
        {"music","music <path> [loop_offset]","Sets the music that loops during the stage."},
        {"gravity","gravity <value>","Sets stage gravity. -10 gives the engine default of -0.1."},
        {"cameratype","cameratype <0|1>","Use 1 to let the camera follow player altitude."},
        {"cameraoffset","cameraoffset <x> <y>","Offsets the camera's scrolling focus within stage boundaries."},
        {"camerazoom","camerazoom <50-200>","Native stage zoom in the updated OpenBOR3D fork. 100 uses the original scale."},
        {"wall","wall <x> <z> <upper_left> <lower_left> <upper_right> <lower_right> <depth> <altitude> [type]","Defines a solid platform rising from the floor."},
        {"hole","hole <x> <z> <upper_left> <lower_left> <upper_right> <lower_right> <depth> [altitude] [type]","Defines a pit in the floor."},
        {"spawn","spawn <model_id>","Begins a spawn event for a registered model."},
        {"coords","coords <x> <z> [altitude]","Position of the pending spawn."},
        {"wait","wait","Begins a camera lock event."},
        {"at","at <scroll_x>","Commits a pending stage event at this scroll position."},
        {"mesh3d","mesh3d <glb_path> <x> <altitude> <z> <scale> [yaw]","Places a static mesh in OpenBOR3D. The path must exist in the module."}
    };
    static constexpr CommandHelp models[]{
        {"load","load <model_id> <path>","Registers and loads a model at startup."},
        {"know","know <model_id> <path>","Registers a model to load when needed."},
        {"ajspecial","ajspecial <special|double|attack2|attack3|attack4>","Maps the native Special animation to an input. Attack 2 gives Special its own input when the model also has Block."},
        {"maxfreespecials","maxfreespecials <count>","Number of Free Special slots. Studio computes this from animation IDs."},
        {"colourselect","colourselect <0|1>","Enables palette selection on the character select screen."},
        {"spdirection","spdirection <p1> [p2] [p3] [p4]","Sets each player slot's facing on the character select screen."}
    };
    static constexpr CommandHelp levels[]{
        {"skiptoset","skiptoset <set_index>","Starts the specified set directly. Index 0 is the first set."},
        {"timeloc","timeloc <x> <y> <width> <height> <hide_border> <font>","Sets the HUD timer position and appearance."},
        {"p1smenu","p1smenu <sprite_x> <sprite_y> <text_x> <text_y>","Sets player one's sprite and Ready text positions on Character Select."}
    };
    static constexpr CommandHelp levelSet[]{
        {"maxplayers","maxplayers <1-4>","Maximum players for this set."},
        {"skipselect","skipselect <model_id> [model_id ...]","Selects the listed characters without opening the character select screen."},
        {"z","z <top> <bottom> [background_depth]","Sets the playable depth range and background depth reference."},
        {"file","file <level_path>","Adds a stage to the current set."},
        {"scene","scene <scene_script_path>","Plays the scene at this point in the current set."},
        {"next","next","Moves to the following stage in the set."}
    };
    static constexpr CommandHelp video[]{
        {"video","video <mode>","Video mode 0 uses 320 × 240. Studio writes mode 0 before custom video commands."}
    };
    switch (context) {
    case CommandContext::model: return model;
    case CommandContext::animation: return animation;
    case CommandContext::frame: return frame;
    case CommandContext::stage: return stage;
    case CommandContext::models: return models;
    case CommandContext::levels: return levels;
    case CommandContext::levelSet: return levelSet;
    case CommandContext::video: return video;
    }
    return {};
}

}

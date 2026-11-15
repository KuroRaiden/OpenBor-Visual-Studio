#include "jump.hpp"
#include "native.hpp"

#include <algorithm>
#include <cmath>

namespace studio {
namespace {

template<class Read>
void readCommands(const std::string& source, Read read, bool headerOnly = false) {
    std::istringstream input(source), tokens;
    std::string line;
    while (std::getline(input,line)) {
        const auto command = nativeCommand(line,tokens);
        if (headerOnly && command == "anim") break;
        if (!command.empty()) read(command,tokens);
    }
}

std::string nativeText(const Project& project, const std::string& name) {
    return project.nativeFiles.contains(name) ? nativeFileText(project,name) : std::string{};
}

float frameSeconds(const Frame& frame) { return std::max(1,frame.ticks)/100.f; }

}

JumpSettings jumpSettings(const Project& project, const Character& model, const Stage* stage) {
    JumpSettings settings;
    settings.speed = -1;
    const auto registry = [&](const std::string& command, std::istringstream& tokens) {
        float value;
        if (!(tokens >> value) || !std::isfinite(value)) return;
        if (command == "jumpheight") settings.height = settings.runningHeight = value;
        if (command == "jumpspeed") settings.speed = value;
        if (command == "global_config_game_speed") settings.ticksPerSecond = int(std::clamp(value,1.f,2000.f));
    };
    if (!project.nativeEdits.contains("data/models.txt")) readCommands(project.module.modelsHeader,registry);
    readCommands(nativeText(project,"data/models.txt"),registry);
    const float defaultHeight = settings.height, defaultSpeed = settings.speed;
    float groundSpeed = std::clamp(model.speed/10,.5f,30.f);
    const auto applyModel = [&](const std::string& command, std::istringstream& tokens) {
        if (command == "move_constraints" || command == "move_config") {
            settings.subjectToGravity = false;
            for (std::string flag; tokens >> flag;)
                if (lower(flag) == "subject_to_gravity") settings.subjectToGravity = true;
            return;
        }
        float value;
        if (!(tokens >> value) || !std::isfinite(value)) return;
        if (command == "speed") groundSpeed = std::clamp(value/10,.5f,30.f);
        if (command == "speedf" || command == "speed_x") groundSpeed = value;
        if (command == "jumpheight" || command == "jump_height") settings.height = value;
        if (command == "jumpspeed") settings.speed = value/10;
        if (command == "jumpspeedf" || command == "jump_speed") settings.speed = value;
        if (command == "run.jump.x") settings.runningDistance = value;
        if (command == "run.jump.y") settings.runningHeight = value;
        if (command == "antigravity") settings.antigravity = value/100;
        if (command == "subject_to_gravity") settings.subjectToGravity = value != 0;
        if (command == "running") {
            if (tokens >> value && std::isfinite(value)) settings.runningHeight = value;
            if (tokens >> value && std::isfinite(value)) settings.runningDistance = value;
        }
    };
    readCommands(nativeText(project,model.nativeFile),applyModel,true);
    if (!project.nativeEdits.contains(model.nativeFile)) {
        settings.height = model.entity.jumpHeight >= 0 ? model.entity.jumpHeight : defaultHeight;
        settings.speed = model.entity.jumpSpeed >= 0 ? model.entity.jumpSpeed : defaultSpeed;
        if (model.entity.gravity >= 0) settings.subjectToGravity = model.entity.gravity != 0;
        readCommands(model.extraHeader,applyModel,true);
        if (model.entity.runningJumpHeight >= 0) settings.runningHeight = model.entity.runningJumpHeight;
        if (model.entity.runningJumpDistance >= 0) settings.runningDistance = model.entity.runningJumpDistance;
    }
    if (settings.speed < 0) settings.speed = std::max(1.f,groundSpeed);
    const auto levelOrder = [&](const std::string& command, std::istringstream& tokens) {
        float value;
        if (command == "gravity" && tokens >> value && std::isfinite(value)) settings.gravity = value;
    };
    if (!project.nativeEdits.contains("data/levels.txt")) {
        readCommands(project.module.levelsHeader,levelOrder);
        if (project.nativeFiles.empty()) readCommands(project.module.levelSetCommands,levelOrder);
    }
    readCommands(nativeText(project,"data/levels.txt"),levelOrder);
    if (stage) {
        const auto level = [&](const std::string& command, std::istringstream& tokens) {
            float value;
            if (!(tokens >> value) || !std::isfinite(value)) return;
            if (command == "gravity") settings.gravity = value/100;
            if (command == "maxfallspeed") settings.fallSpeed = value/10;
            if (command == "maxtossspeed") settings.tossSpeed = value/10;
        };
        readCommands(nativeText(project,stage->nativeFile),level);
        if (!project.nativeEdits.contains(stage->nativeFile)) readCommands(stage->extraHeader,level);
    }
    return settings;
}

JumpArc simulateJump(const JumpSettings& settings, JumpMode mode) {
    JumpArc arc;
    arc.points.push_back({});
    const bool running = mode == JumpMode::running;
    const float takeoff = running ? settings.runningHeight : settings.height;
    if (!(takeoff > 0)) return arc;
    const int rate = std::clamp(settings.ticksPerSecond,1,2000);
    const float units = 100.f/rate;
    const float speed = mode == JumpMode::standing ? 0.f : settings.speed;
    const float distance = std::clamp(speed*(running ? settings.runningDistance : 1.f),-1000.f,1000.f)*units;
    const float gravity = settings.subjectToGravity ? std::clamp(settings.gravity*(1-settings.antigravity),-100.f,100.f) : 0;
    const float minimum = std::clamp(std::min(settings.fallSpeed,settings.tossSpeed),-1000.f,1000.f);
    const float maximum = std::clamp(std::max(settings.fallSpeed,settings.tossSpeed),-1000.f,1000.f);
    auto point = JumpPoint{distance,.5f};
    arc.points.push_back(point);
    arc.apex = 1;
    float velocity = std::min(takeoff,1000.f);
    // OpenBOR advances position before applying gravity and its speed limits.
    for (int tick = 2; tick <= 12*rate; ++tick) {
        point.x += distance;
        point.altitude += velocity*units;
        velocity = std::clamp(velocity+gravity*units,minimum,maximum);
        arc.landed = point.altitude <= 0 && velocity <= 0;
        if (arc.landed) point.altitude = 0;
        arc.points.push_back(point);
        if (point.altitude > arc.points[arc.apex].altitude) arc.apex = arc.points.size()-1;
        if (arc.landed) break;
    }
    arc.seconds = float(arc.points.size()-1)/rate;
    return arc;
}

const Animation* jumpClip(const Character& model, std::string_view id) {
    const auto it = std::find_if(model.animations.begin(),model.animations.end(),[&](const Animation& clip) {
        return !clip.frames.empty() && animationId(clip.name) == id;
    });
    return it == model.animations.end() ? nullptr : &*it;
}

const Animation* jumpAnimation(const Character& model, JumpMode mode) {
    if (mode == JumpMode::running)
        if (const auto* clip = jumpClip(model,"runjump")) return clip;
    if (mode != JumpMode::standing)
        if (const auto* clip = jumpClip(model,"forwardjump")) return clip;
    return jumpClip(model,"jump");
}

float jumpClipSeconds(const Animation* animation) {
    float duration = 0;
    if (animation)
        for (const auto& frame : animation->frames) duration += frameSeconds(frame);
    return duration;
}

size_t jumpClipFrame(const Animation& animation, float seconds) {
    const float duration = jumpClipSeconds(&animation);
    if (animation.loop && duration > 0) seconds = std::fmod(seconds,duration);
    for (size_t i=0; i<animation.frames.size(); ++i) {
        seconds -= frameSeconds(animation.frames[i]);
        if (seconds < 0) return i;
    }
    return animation.frames.empty() ? 0 : animation.frames.size()-1;
}
}

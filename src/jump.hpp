#pragma once

#include "project.hpp"

namespace studio {

enum class JumpMode { standing, forward, running };

struct JumpSettings {
    float height = 4, speed = 1, runningHeight = 4, runningDistance = 1;
    float gravity = -.1f, fallSpeed = -6, tossSpeed = 100;
    float antigravity = 0;
    int ticksPerSecond = 200;
    bool subjectToGravity = true;
};

struct JumpPoint { float x = 0, altitude = 0; };

struct JumpArc {
    std::vector<JumpPoint> points;
    float seconds = 0;
    size_t apex = 0;
    bool landed = false;
};

JumpSettings jumpSettings(const Project& project, const Character& model, const Stage* stage);
JumpArc simulateJump(const JumpSettings& settings, JumpMode mode);
const Animation* jumpAnimation(const Character& model, JumpMode mode);
const Animation* jumpClip(const Character& model, std::string_view id);
float jumpClipSeconds(const Animation* animation);
size_t jumpClipFrame(const Animation& animation, float seconds);
}

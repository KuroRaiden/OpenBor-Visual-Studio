#pragma once

#include "project.hpp"
#include <future>
#include <windows.h>

namespace studio {

class Playtest {
public:
    ~Playtest();
    void loadEngine();
    void linkEngine(const fs::path& executable);
    const fs::path& engine() const { return engine_; }
    void start(const Project& project, StagePlaytest test);
    std::string update();
    void stop();
    bool active() const { return build_.valid() || process_ != nullptr; }
    bool building() const { return build_.valid(); }
    fs::path logs() const;
private:
    fs::path engine_, session_, previous_;
    HANDLE process_ = nullptr;
    std::future<fs::path> build_;
};
}

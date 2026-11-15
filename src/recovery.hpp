#pragma once

#include "project.hpp"
#include <chrono>
#include <future>
#include <windows.h>

namespace studio {

class Recovery {
public:
    ~Recovery();
    void initialize();
    fs::path latest() const;
    Project restore(const fs::path& directory);
    void autosave(const Project& project);
    std::string finish();
    void clear();
    static constexpr auto interval = std::chrono::minutes(8);
private:
    fs::path root_, active_, restored_;
    HANDLE lease_ = INVALID_HANDLE_VALUE, restoredLease_ = INVALID_HANDLE_VALUE;
    std::future<void> job_;
};
}

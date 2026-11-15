#include "recovery.hpp"

#include <shlobj.h>
#include <algorithm>
#include <stdexcept>

namespace studio {
namespace {
constexpr auto filename = L"Recovered.borstudio";
constexpr auto first = L"Latest";
constexpr auto second = L"Previous";

HANDLE lock(const fs::path& directory) {
    return CreateFileW((directory/L"session.lock").c_str(),GENERIC_READ | GENERIC_WRITE,0,
                       nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
}

bool ordinaryDirectory(const fs::path& path) {
    const auto flags = GetFileAttributesW(path.c_str());
    return flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_DIRECTORY) &&
           !(flags & FILE_ATTRIBUTE_REPARSE_POINT);
}

std::vector<fs::path> snapshots(const fs::path& directory) {
    std::vector<fs::path> paths;
    for (const auto slot : {first,second}) {
        const auto folder = directory/slot;
        if (ordinaryDirectory(folder) && fs::is_regular_file(folder/filename)) paths.push_back(folder/filename);
    }
    std::sort(paths.begin(),paths.end(),[](const auto& a,const auto& b) {
        return fs::last_write_time(a) > fs::last_write_time(b);
    });
    return paths;
}

bool generationName(const fs::path& path) {
    const auto name = path.filename().wstring();
    return name.size() == 38 && name.front() == L'{' && name.back() == L'}' &&
        std::all_of(name.begin()+1,name.end()-1,[](wchar_t c) {
            return c == L'-' || (c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f');
        });
}

void release(HANDLE& handle) {
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
}
}

Recovery::~Recovery() {
    if (job_.valid()) job_.wait();
    release(lease_);
    release(restoredLease_);
}

void Recovery::initialize() {
    PWSTR location = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&location)))
        throw std::runtime_error("Could not locate the Windows recovery folder.");
    const fs::path local(location);
    CoTaskMemFree(location);
    root_ = local/L"OpenBOR Studio"/L"Recovery";
    fs::create_directories(root_);
}

fs::path Recovery::latest() const {
    if (root_.empty()) return {};
    fs::path latest;
    auto time = fs::file_time_type::min();
    for (const auto& item : fs::directory_iterator(root_)) {
        const auto& directory = item.path();
        if (directory == active_ || directory == restored_ ||
            !generationName(directory) || !ordinaryDirectory(directory)) continue;
        const HANDLE probe = lock(directory);
        if (probe == INVALID_HANDLE_VALUE) continue;
        CloseHandle(probe);
        try {
            const auto paths = snapshots(directory);
            if (paths.empty()) continue;
            const auto modified = fs::last_write_time(paths.front());
            if (modified > time) {
                latest = directory;
                time = modified;
            }
        } catch (const fs::filesystem_error&) {
            // A second Studio instance can retire a session while we scan.
        }
    }
    return latest;
}

Project Recovery::restore(const fs::path& directory) {
    if (directory.parent_path() != root_ || !generationName(directory) || !ordinaryDirectory(directory))
        throw std::runtime_error("The autosave is unavailable.");
    const HANDLE claimed = lock(directory);
    if (claimed == INVALID_HANDLE_VALUE) throw std::runtime_error("Another Studio window is using this autosave.");
    try {
        for (const auto& file : snapshots(directory)) {
            try {
                auto project = openProject(file,true);
                clear();
                restored_ = directory;
                restoredLease_ = claimed;
                return project;
            } catch (const std::exception&) {}
        }
    } catch (...) {
        CloseHandle(claimed);
        throw;
    }
    CloseHandle(claimed);
    throw std::runtime_error("Neither saved recovery copy could be opened.");
}

void Recovery::autosave(const Project& project) {
    if (job_.valid()) return;
    if (root_.empty()) initialize();
    if (active_.empty()) {
        GUID id{};
        wchar_t name[39]{};
        if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id,name,39))
            throw std::runtime_error("Could not create a recovery session.");
        const auto directory = root_/name;
        if (!fs::create_directory(directory)) throw std::runtime_error("Could not create a recovery session folder.");
        lease_ = lock(directory);
        if (lease_ == INVALID_HANDLE_VALUE) throw std::runtime_error("Could not lock the recovery session.");
        active_ = directory;
    }
    job_ = std::async(std::launch::async,[project,directory=active_] {
        const auto paths = snapshots(directory);
        const auto slot = directory/(paths.empty() || paths.front().parent_path().filename() == second ? first : second);
        if (fs::exists(slot) && !ordinaryDirectory(slot)) throw std::runtime_error("Invalid recovery snapshot folder.");
        fs::remove_all(slot);
        saveProject(project,slot/filename);
    });
}

std::string Recovery::finish() {
    if (!job_.valid() || job_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
    try {
        job_.get();
        return "Recovery snapshot saved.";
    } catch (const std::exception& error) {
        return "Autosave failed. "+std::string(error.what());
    }
}

void Recovery::clear() {
    if (job_.valid()) {
        job_.wait();
        (void)finish();
    }
    release(lease_);
    release(restoredLease_);
    for (auto* directory : {&active_,&restored_}) {
        if (directory->empty()) continue;
        std::error_code error;
        fs::remove_all(*directory,error);
        directory->clear();
    }
}
}

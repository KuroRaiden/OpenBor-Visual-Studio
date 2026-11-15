#include "playtest.hpp"
#include "images.hpp"
#include "media.hpp"

#include <shlobj.h>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace studio {
namespace {

fs::path settingsFolder() {
    PWSTR value = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&value)))
        throw std::runtime_error("Could not find Studio's settings folder.");
    const fs::path path(value);
    CoTaskMemFree(value);
    return path/L"OpenBOR Studio";
}

void clean(const fs::path& session) {
    if (session.empty()) return;
    std::error_code error;
    fs::remove_all(session,error);
}

fs::path createSession(const fs::path& parent) {
    GUID id{};
    wchar_t name[39]{};
    if (FAILED(CoCreateGuid(&id)) || !StringFromGUID2(id,name,39))
        throw std::runtime_error("Could not create a test session.");
    fs::create_directories(parent);
    const auto session = parent/name;
    if (!fs::create_directory(session)) throw std::runtime_error("Could not create a temporary test folder.");
    return session;
}

struct ComApartment {
    ComApartment() {
        if (FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))
            throw std::runtime_error("Could not initialize the test build.");
    }
    ~ComApartment() { CoUninitialize(); }
};
}

Playtest::~Playtest() {
    if (build_.valid()) build_.wait();
    if (process_) {
        stop();
        WaitForSingleObject(process_,2000);
        CloseHandle(process_);
    }
    clean(session_);
    clean(previous_);
}

void Playtest::loadEngine() {
    std::ifstream in(settingsFolder()/"test-engine.txt",std::ios::binary);
    if (!in) return;
    std::string path{std::istreambuf_iterator<char>(in),{}};
    engine_ = pathFromUtf8(path);
}

void Playtest::linkEngine(const fs::path& executable) {
    if (active()) throw std::runtime_error("Close the current test before changing its engine.");
    const auto file = fs::absolute(executable);
    if (mediaExtension(file) != ".exe" || !fs::is_regular_file(file))
        throw std::runtime_error("Choose a Windows OpenBOR executable.");
    const auto folder = settingsFolder();
    fs::create_directories(folder);
    std::ofstream out(folder/"test-engine.txt",std::ios::binary | std::ios::trunc);
    out << utf8(file);
    out.close();
    if (!out) throw std::runtime_error("Could not save the engine path.");
    engine_ = file;
}

void Playtest::start(const Project& project, StagePlaytest test) {
    if (active()) throw std::runtime_error("A test is already running.");
    if (engine_.empty() || !fs::is_regular_file(engine_)) throw std::runtime_error("Link an OpenBOR executable first.");
    const auto session = createSession(settingsFolder()/"Playtest");
    try {
        build_ = std::async(std::launch::async,[project,engine=engine_,session,test=std::move(test)] {
            const ComApartment com;
            Images images;
            const auto output = exportProject(project,session,images,true,ExportDestination::newFolder,&test);
            // OpenBOR resolves the game directory from the executable or its working directory.
            fs::copy_file(engine,output/engine.filename());
            for (const auto& file : fs::directory_iterator(engine.parent_path())) {
                if (file.is_regular_file() && mediaExtension(file.path()) == ".dll")
                    fs::copy_file(file.path(),output/file.path().filename());
            }
            return output;
        });
        clean(previous_);
        previous_.clear();
        session_ = session;
    } catch (...) { clean(session); throw; }
}

std::string Playtest::update() {
    if (process_ && WaitForSingleObject(process_,0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess(process_,&code);
        CloseHandle(process_);
        process_ = nullptr;
        previous_ = std::exchange(session_,{});
        return code == 0 ? "Test play ended." : "OpenBOR test exited with code "+std::to_string(code)+".";
    }
    if (!build_.valid() || build_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
    try {
        const auto output = build_.get();
        const auto executable = output/engine_.filename();
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESHOWWINDOW;
        startup.wShowWindow = SW_SHOWNORMAL;
        PROCESS_INFORMATION info{};
        if (!CreateProcessW(executable.c_str(),nullptr,nullptr,nullptr,FALSE,0,nullptr,output.c_str(),&startup,&info))
            throw std::runtime_error("Windows could not run the selected engine (error "+std::to_string(GetLastError())+").");
        CloseHandle(info.hThread);
        process_ = info.hProcess;
        return "OpenBOR is running the stage test.";
    } catch (const std::exception& error) {
        previous_ = std::exchange(session_,{});
        return "Test play failed. "+std::string(error.what());
    }
}

void Playtest::stop() {
    if (process_) TerminateProcess(process_,0);
}

fs::path Playtest::logs() const {
    const auto& session = session_.empty() ? previous_ : session_;
    return session.empty() ? fs::path{} : session/"OpenBOR_Export"/"Logs";
}
}

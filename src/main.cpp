#include "ui.hpp"

#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <shellapi.h>
#include <dxgi.h>
#include <fstream>
#include <stdexcept>

using studio::ComPtr;
namespace fs = std::filesystem;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

class Graphics {
public:
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11RenderTargetView> target;

    void initialize(HWND window) {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 2;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        auto result = D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,
                nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context);
        if (FAILED(result)) {
            result = D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,
                    nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&device,nullptr,&context);
        }
        if (FAILED(result)) throw std::runtime_error("Unable to start Direct3D 11.");
        createTarget();
    }

    void createTarget() {
        ComPtr<ID3D11Texture2D> buffer;
        if (FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer))) ||
            FAILED(device->CreateRenderTargetView(buffer.Get(),nullptr,&target))) {
            throw std::runtime_error("Unable to create the editor render target.");
        }
    }

    void resize(UINT width, UINT height) {
        if (!width || !height) return;
        context->OMSetRenderTargets(0,nullptr,nullptr);
        target.Reset();
        if (FAILED(swap->ResizeBuffers(0,width,height,DXGI_FORMAT_UNKNOWN,0))) {
            throw std::runtime_error("Unable to resize the editor window.");
        }
        createTarget();
    }

    void render() {
        constexpr float color[]{.055f,.077f,.116f,1};
        auto* view = target.Get();
        context->OMSetRenderTargets(1,&view,nullptr);
        context->ClearRenderTargetView(view,color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void capture(const fs::path& file) {
        ComPtr<ID3D11Texture2D> buffer, staging;
        if (FAILED(swap->GetBuffer(0,IID_PPV_ARGS(&buffer)))) throw std::runtime_error("Capture failed.");
        D3D11_TEXTURE2D_DESC desc{};
        buffer->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        if (FAILED(device->CreateTexture2D(&desc,nullptr,&staging))) throw std::runtime_error("Capture failed.");
        context->CopyResource(staging.Get(),buffer.Get());
        D3D11_MAPPED_SUBRESOURCE map{};
        if (FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map))) throw std::runtime_error("Capture failed.");
        std::ofstream output(file,std::ios::binary);
        BITMAPFILEHEADER bmp{};
        BITMAPINFOHEADER info{};
        info.biSize = sizeof(info);
        info.biWidth = LONG(desc.Width);
        info.biHeight = -LONG(desc.Height);
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biSizeImage = desc.Width*desc.Height*4;
        bmp.bfType = 0x4d42;
        bmp.bfOffBits = sizeof(bmp)+sizeof(info);
        bmp.bfSize = bmp.bfOffBits+info.biSizeImage;
        output.write(reinterpret_cast<const char*>(&bmp),sizeof(bmp));
        output.write(reinterpret_cast<const char*>(&info),sizeof(info));
        std::vector<unsigned char> row(size_t(desc.Width)*4);
        for (UINT y = 0; y < desc.Height; ++y) {
            const auto* data = static_cast<const unsigned char*>(map.pData)+size_t(y)*map.RowPitch;
            for (UINT x = 0; x < desc.Width; ++x) {
                row[x*4] = data[x*4+2]; row[x*4+1] = data[x*4+1];
                row[x*4+2] = data[x*4]; row[x*4+3] = 255;
            }
            output.write(reinterpret_cast<const char*>(row.data()),std::streamsize(row.size()));
        }
        context->Unmap(staging.Get(),0);
        if (!output) throw std::runtime_error("Could not write capture.");
    }
};

studio::Editor* editor = nullptr;
UINT pendingWidth = 0, pendingHeight = 0;
bool minimized = false;
bool closeRequested = false;

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd,message,wparam,lparam)) return 1;
    switch (message) {
    case WM_SIZE:
        minimized = wparam == SIZE_MINIMIZED;
        if (!minimized) {
            pendingWidth = LOWORD(lparam);
            pendingHeight = HIWORD(lparam);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        auto* sizes = reinterpret_cast<MINMAXINFO*>(lparam);
        sizes->ptMinTrackSize = {1040,670};
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_CLOSE:
        if (editor) closeRequested = true;
        else DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd,message,wparam,lparam);
}

fs::path executableDirectory() {
    std::wstring path(32768,L'\0');
    const DWORD length = GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if (!length || length >= path.size()) throw std::runtime_error("Cannot locate the editor executable.");
    path.resize(length);
    return fs::path(path).parent_path();
}

int validate(const fs::path& file, const fs::path& output) {
    const auto report = output / "verification.txt";
    fs::create_directories(output);
    try {
        studio::Images images;
        studio::Project original;
        if (fs::is_directory(file)) original.characters.push_back(studio::importSpriteFolder(file,"right",images));
        else original = studio::openProject(file);
        const auto portable = output/"Roundtrip.borstudio";
        studio::saveProject(original,portable);
        const auto roundtrip = studio::openProject(portable);
        const auto exported = studio::exportProject(roundtrip,output,images);
        if (roundtrip.characters.size() != original.characters.size() ||
            roundtrip.stages.size() != original.stages.size()) {
            throw std::runtime_error("Project roundtrip lost models or stages.");
        }
        std::ofstream log(report);
        log << "Verified save, reopen, and OpenBOR export for " << roundtrip.characters.size()
            << " models and " << roundtrip.stages.size() << " stages.\n"
            << studio::utf8(exported) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::ofstream log(report);
        log << "Verification failed: " << error.what() << '\n';
        return 1;
    }
}

int run(HINSTANCE instance, int show, const std::vector<std::wstring>& args) {
    if (args.size() == 4 && args[1] == L"--verify") return validate(args[2],args[3]);
    const bool capturing = args.size() == 4 && args[1] == L"--capture";
    ImGui_ImplWin32_EnableDpiAwareness();
    const WNDCLASSEXW wc{sizeof(wc),CS_CLASSDC,windowProc,0,0,instance,
                         nullptr,LoadCursor(nullptr,IDC_ARROW),nullptr,nullptr,L"OpenBORStudio",nullptr};
    if (!RegisterClassExW(&wc)) throw std::runtime_error("Could not register the studio window.");
    RECT size{0,0,1430,890};
    AdjustWindowRect(&size,WS_OVERLAPPEDWINDOW,FALSE);
    HWND hwnd = CreateWindowW(wc.lpszClassName,L"OpenBOR Studio",WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,CW_USEDEFAULT,size.right-size.left,size.bottom-size.top,
                             nullptr,nullptr,instance,nullptr);
    if (!hwnd) throw std::runtime_error("Could not create the studio window.");
    Graphics graphics;
    graphics.initialize(hwnd);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    std::wstring fonts(MAX_PATH,L'\0');
    const auto n = GetWindowsDirectoryW(fonts.data(),UINT(fonts.size()));
    fonts.resize(n);
    const auto font = fs::path(fonts)/"Fonts"/"segoeui.ttf";
    if (fs::is_regular_file(font)) io.Fonts->AddFontFromFileTTF(studio::utf8(font).c_str(),16.0f);
    studio::configureStyle();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(graphics.device.Get(),graphics.context.Get());
    const auto sample = executableDirectory()/"Demo"/"Training Ground.borstudio";
    studio::Editor studioEditor(hwnd,graphics.device.Get(),sample);
    editor = &studioEditor;
    if (capturing && args[2] != L"blank") editor->setCaptureView(args[2] == L"stage");
    ShowWindow(hwnd,capturing ? SW_SHOWNOACTIVATE : show);
    UpdateWindow(hwnd);
    bool running = true;
    int frames = 0;
    while (running) {
        MSG message{};
        while (PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (minimized) { Sleep(25); continue; }
        if (pendingWidth && pendingHeight) {
            graphics.resize(pendingWidth,pendingHeight);
            pendingWidth = pendingHeight = 0;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (closeRequested) {
            closeRequested = false;
            editor->requestClose();
        }
        editor->draw();
        ImGui::Render();
        if (!IsWindow(hwnd)) break;
        graphics.render();
        if (capturing && ++frames >= 4) {
            graphics.capture(args[3]);
            break;
        }
        graphics.swap->Present(1,0);
    }
    editor = nullptr;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName,instance);
    return 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    const HRESULT com = CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    int count = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(),&count);
    std::vector<std::wstring> args;
    if (argv) {
        for (int i=0; i<count; ++i) args.emplace_back(argv[i]);
        LocalFree(argv);
    }
    int result = 1;
    try {
        if (FAILED(com)) throw std::runtime_error("Cannot initialize Windows COM.");
        result = run(instance,show,args);
    } catch (const std::exception& error) {
        MessageBoxA(nullptr,error.what(),"OpenBOR Studio",MB_OK|MB_ICONERROR);
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return result;
}


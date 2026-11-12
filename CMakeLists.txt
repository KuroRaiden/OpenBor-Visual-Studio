cmake_minimum_required(VERSION 3.21)
project(OpenBORStudio VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "OpenBOR Studio uses Win32, Windows Imaging Component and Direct3D 11.")
endif()

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui")
add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
)
target_include_directories(imgui PUBLIC "${IMGUI_DIR}" "${IMGUI_DIR}/backends" "${IMGUI_DIR}/misc/cpp")
target_compile_features(imgui PUBLIC cxx_std_20)
target_compile_definitions(imgui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

add_executable(OpenBORStudio WIN32 src/main.cpp src/ui.cpp src/project.cpp src/images.cpp src/module.cpp src/mesh.cpp src/media.cpp src/native.cpp)
target_compile_definitions(OpenBORStudio PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(OpenBORStudio PRIVATE vendor)
target_link_libraries(OpenBORStudio PRIVATE imgui d3d11 dxgi windowscodecs ole32 shell32 shlwapi uuid gdi32 gdiplus winmm)
if(MSVC)
    target_compile_options(OpenBORStudio PRIVATE /W4 /utf-8 /permissive-)
    set_property(TARGET OpenBORStudio imgui PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set_target_properties(OpenBORStudio PROPERTIES OUTPUT_NAME "OpenBOR Studio")

add_custom_command(TARGET OpenBORStudio POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/Demo" "$<TARGET_FILE_DIR:OpenBORStudio>/Demo"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/ScreenTemplate" "$<TARGET_FILE_DIR:OpenBORStudio>/ScreenTemplate"
    VERBATIM
)

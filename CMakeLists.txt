cmake_minimum_required(VERSION 3.24)

project(PinTop
    VERSION 0.1.0
    DESCRIPTION "轻量级窗口置顶工具"
    LANGUAGES CXX
)

# 仅支持 Windows 10 / 11，x64
if(NOT WIN32)
    message(FATAL_ERROR "PinTop 仅支持 Windows 10/11 x64")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PinTop WIN32
    src/main.cpp
    src/app.cpp
    src/pin.cpp
    src/layer.cpp
    app.rc
)

target_include_directories(PinTop PRIVATE src)

target_compile_definitions(PinTop PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

if(MSVC)
    # 严格警告 + 现代 C++ + UTF-8 源码
    target_compile_options(PinTop PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    # 静态链接运行库 (/MT)，目标机器无需安装 VC++ Redistributable
    set_target_properties(PinTop PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    # Release 尺寸优先
    target_compile_options(PinTop PRIVATE $<$<CONFIG:Release>:/O2>)
endif()

# Win32 系统库（dwmapi: 标题栏按钮定位）
target_link_libraries(PinTop PRIVATE user32 shell32 gdi32 comctl32 dwmapi)

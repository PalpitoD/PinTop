#pragma once

#include <windows.h>

// 全局热键：快速置顶 / 取消置顶当前前台窗口。
// 默认 Ctrl+Shift+P；后续可扩展为可配置（选项对话框）。
class HotKey {
public:
    // 注册默认热键；返回是否成功（可能被其他程序占用）。
    static bool registerDefault(HWND owner);
    static void unregister(HWND owner);

    static constexpr UINT ID   = 1;
    static constexpr UINT VK   = 'P';
    // MOD_NOREPEAT：按住不重复触发（Win7+），避免置顶/取消快速闪烁
    static constexpr UINT MODS = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
};

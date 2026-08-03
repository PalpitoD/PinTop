#pragma once

#include <windows.h>

// 置顶放置模式的捕获图层：一个 1×1 透明顶层小窗口。
// 进入模式后捕获鼠标，光标变为准星，用户点击任意窗口即发出 WM_PINREQ。
// 与旧版 DeskPins 不同：不注入任何系统级鼠标事件（见 app.cpp 的 startPinPlacement）。
class LayerWnd {
public:
    static bool start(HWND owner);
    static void cancel();
    static bool active();

    static HWND hwnd() { return hwnd_; }

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static HWND hwnd_;
    // 第一次"按下"由 start() 合成（仅置捕获），第二次才是用户的真实点击。
    static bool gotInitLButtonDown;
};

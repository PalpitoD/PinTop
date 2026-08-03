#pragma once

#include <windows.h>
#include <unordered_map>

// 图钉窗口：一个带 alpha 的顶层异形小窗口，挂在目标窗口标题栏上。
// 点击图钉 = 取消该窗口的置顶。
// 所有图钉由本类静态管理（创建/移除/遍历），App 退出时统一清理。
class PinWnd {
public:
    // 惰性注册窗口类；首次调用前必须设置实例句柄（见 setHInstance）。
    static void setHInstance(HINSTANCE hInst);
    static bool create(HWND target);
    static void remove(HWND target);
    static void removeAll();

    // 目标窗口位置变化时由全局事件钩子调用，重定位图钉。
    static void repositionFor(HWND target);

    // hwnd 是否为某个图钉窗口（放置模式点击过滤用）
    static bool isPinWindow(HWND hwnd);

    static bool isPinned(HWND target);

private:
    PinWnd(HWND target);
    ~PinWnd();

    bool init(); // 加载位图、创建窗口、应用分层绘制

    static PinWnd* find(HWND target);

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND target_{};
    HWND hwnd_{};
    HBITMAP bmp_{};   // 32bpp premultiplied-alpha 位图
    SIZE size_{};

    static HINSTANCE hInst_;
    static std::unordered_map<HWND, PinWnd*> all_;
};

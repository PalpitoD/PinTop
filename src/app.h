#pragma once

#include <windows.h>

// 应用单例：持有实例句柄、主隐藏窗口与托盘图标。
// 无主窗口常驻 —— 仅一个隐藏消息窗口承载托盘回调与消息循环。
class App {
public:
    static App& instance();

    HINSTANCE hInstance() const { return hInst_; }
    HWND mainWnd() const { return mainWnd_; }

    // 进入消息循环，退出时返回进程退出码。
    int run(HINSTANCE hInst);

    // 请求退出（销毁主窗口，触发 WM_DESTROY 结束循环）。
    void exitApp();

    // 自定义消息
    static constexpr UINT WM_PINREQ = WM_APP + 2; // wParam/lParam = 目标屏幕坐标 x/y

private:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool createMainWindow();
    void addTrayIcon();
    void removeTrayIcon();
    void showTrayMenu();
    void onTrayIcon(UINT msg, LPARAM lParam);
    void onCommand(int id);

    // 置顶功能
    void startPinPlacement();  // 进入"放置图钉"模式（鼠标钩子捕获）
    void placePinAt(POINT pt); // 在屏幕坐标处放置图钉
    void installWinEventHook();
    void uninstallWinEventHook();

    // 全局事件钩子回调（目标窗口移动/销毁时重定位/清理图钉）
    static void CALLBACK winEventProc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                                      LONG idObject, LONG idChild, DWORD thread, DWORD time);

    // 托盘回调消息
    static constexpr UINT WM_TRAYICON = WM_APP + 1;
    // 托盘图标 ID（NOTIFYICONDATA.uID）
    static constexpr UINT TRAY_ID = 1;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInst_{};
    HWND mainWnd_{};
    HWINEVENTHOOK winEventHook_{};
};

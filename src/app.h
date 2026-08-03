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

    // 托盘回调消息
    static constexpr UINT WM_TRAYICON = WM_APP + 1;
    // 托盘图标 ID（NOTIFYICONDATA.uID）
    static constexpr UINT TRAY_ID = 1;

    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HINSTANCE hInst_{};
    HWND mainWnd_{};
};

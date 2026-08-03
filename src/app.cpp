#include "app.h"
#include "resource.h"

#include <shellapi.h>
#include <cwchar>

namespace {

// 托盘工具提示文本
constexpr wchar_t kTrayTip[] = L"PinTop - 窗口置顶";

} // namespace

App& App::instance()
{
    static App app;
    return app;
}

int App::run(HINSTANCE hInst)
{
    hInst_ = hInst;

    // Per-Monitor V2 DPI 感知（Windows 10 1703+）：
    // 避免系统按位图缩放导致图标/坐标模糊错位。
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (!createMainWindow()) {
        return 1;
    }
    addTrayIcon();

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void App::exitApp()
{
    if (mainWnd_) {
        DestroyWindow(mainWnd_);
    }
}

bool App::createMainWindow()
{
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = hInst_;
    wc.hIcon         = LoadIconW(hInst_, MAKEINTRESOURCEW(IDI_APP));
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"PinTopMainWnd";
    if (!RegisterClassExW(&wc)) {
        return false;
    }

    // 主窗口仅作为消息宿主，永不显示（无 WS_VISIBLE）
    mainWnd_ = CreateWindowExW(
        0, wc.lpszClassName, L"PinTop", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr, nullptr, hInst_, this);
    return mainWnd_ != nullptr;
}

void App::addTrayIcon()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = mainWnd_;
    nid.uID              = TRAY_ID;
    nid.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon            = LoadIconW(hInst_, MAKEINTRESOURCEW(IDI_APP));
    if (nid.hIcon == nullptr) {
        nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION); // 兜底
    }
    wcscpy_s(nid.szTip, kTrayTip);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void App::removeTrayIcon()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = mainWnd_;
    nid.uID    = TRAY_ID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void App::showTrayMenu()
{
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CM_NEWPIN, L"置顶窗口(&P)...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CM_EXIT, L"退出(&X)");

    // 菜单弹出前先置前，否则可能立即失焦收起
    SetForegroundWindow(mainWnd_);

    POINT pt{};
    GetCursorPos(&pt);
    // TPM_RETURNCMD：TrackPopupMenu 直接返回选中的命令 ID
    const int cmd = static_cast<int>(
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, mainWnd_, nullptr));
    DestroyMenu(menu);

    if (cmd > 0) {
        onCommand(cmd);
    }
}

void App::onTrayIcon(UINT msg, LPARAM lParam)
{
    if (msg != WM_TRAYICON) {
        return;
    }
    switch (LOWORD(lParam)) {
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        showTrayMenu();
        break;
    case WM_LBUTTONDBLCLK:
        // TODO: 双击托盘 → 进入置顶模式（与菜单"置顶窗口"一致）
        break;
    default:
        break;
    }
}

void App::onCommand(int id)
{
    switch (id) {
    case CM_NEWPIN:
        // TODO: 下一阶段实现 —— 进入"放置图钉"模式：
        //       创建透明图层窗口捕获鼠标 → 点击目标窗口 → 在其最小化按钮上挂图钉。
        break;
    case CM_EXIT:
        exitApp();
        break;
    default:
        break;
    }
}

LRESULT CALLBACK App::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    App* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        const auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self           = static_cast<App*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) {
        return self->handleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT App::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_TRAYICON:
        onTrayIcon(msg, lParam);
        return 0;
    case WM_COMMAND:
        onCommand(LOWORD(wParam));
        return 0;
    case WM_DESTROY:
        removeTrayIcon();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

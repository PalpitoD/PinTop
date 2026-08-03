#include "layer.h"
#include "app.h"
#include "resource.h"

#include <windowsx.h> // GET_X/Y_LPARAM

namespace {

// 放置模式光标：SVG 图钉形状（与图钉/图标同风格）
HCURSOR placePinCursor()
{
    static HCURSOR cur =
        LoadCursorW(App::instance().hInstance(), MAKEINTRESOURCEW(IDC_PLACEPIN));
    if (cur == nullptr) {
        cur = LoadCursorW(nullptr, IDC_CROSS); // 资源损坏兜底
    }
    return cur;
}

} // namespace

HWND LayerWnd::hwnd_{};
bool LayerWnd::gotInitLButtonDown = false;

bool LayerWnd::start(HWND owner)
{
    if (hwnd_) return true; // 已在放置模式

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = proc;
        wc.hInstance     = App::instance().hInstance();
        wc.hCursor       = placePinCursor();
        wc.lpszClassName = L"PinTopLayerWnd";
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }

    // 1×1 顶层透明小窗口（与旧版一致：Win10+ 上避开屏幕左上角，防被系统识别为热区）。
    // WS_EX_NOACTIVATE：绝不抢焦点，否则用户点击目标窗口时 WM_KILLFOCUS 会先于
    // 被捕获的 WM_LBUTTONDOWN 到达，导致放置模式被意外取消。
    const POINT pos = { 100, 100 };
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"PinTopLayerWnd", L"PinTopLayer", WS_POPUP | WS_VISIBLE,
        pos.x, pos.y, 1, 1, owner, nullptr, App::instance().hInstance(), nullptr);
    if (!hwnd_) return false;

    // 让图层窗口捕获鼠标，但不注入系统级输入：
    // 直接投递 WM_LBUTTONDOWN 触发 proc 的初始化分支（SetCapture + 状态标记）。
    // 旧版用 SetCursorPos + mouse_event(LEFTDOWN) 合成真实点击，会让左键在全局保持按下，拖拽其他窗口。
    SendMessageW(hwnd_, WM_LBUTTONDOWN, MK_LBUTTON, 0);
    // 捕获期间 WM_SETCURSOR 不会发送（鼠标不在本窗口上），必须立即设置光标
    // 并在 WM_MOUSEMOVE 中持续保持。
    SetCursor(placePinCursor());
    return true;
}

void LayerWnd::cancel()
{
    if (hwnd_) {
        HWND w = hwnd_;
        hwnd_   = nullptr;
        DestroyWindow(w);
    }
}

bool LayerWnd::active()
{
    return hwnd_ != nullptr;
}

LRESULT CALLBACK LayerWnd::proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        gotInitLButtonDown = false;
        return 0;
    case WM_SETCURSOR:
        // 鼠标恰好在本窗口上时设置图钉光标
        SetCursor(placePinCursor());
        return TRUE;
    case WM_MOUSEMOVE:
        // 捕获期间鼠标消息都发给本窗口：持续保持图钉光标
        SetCursor(placePinCursor());
        return 0;
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        SetCapture(hwnd);
        if (!gotInitLButtonDown) {
            // start() 合成的一次：只用于获得捕获
            gotInitLButtonDown = true;
        } else {
            // 用户真实点击：把屏幕坐标交给 App 放置图钉
            ReleaseCapture();
            POINT pt{ x, y };
            if (ClientToScreen(hwnd, &pt)) {
                PostMessageW(GetParent(hwnd), App::WM_PINREQ, pt.x, pt.y);
            }
            hwnd_ = nullptr; // 即将销毁，先清除全局引用
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KILLFOCUS:
    case WM_CANCELMODE:
        // 取消放置模式。WM_CANCELMODE：按 Alt/系统菜单等模态循环会由系统
        // 自动释放捕获，若不销毁窗口，active() 仍为 true 会导致模式卡死。
        ReleaseCapture();
        hwnd_ = nullptr;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (hwnd_ == hwnd) hwnd_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

#include "layer.h"
#include "app.h"
#include "resource.h"

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

HHOOK LayerWnd::hook_{};
HWND LayerWnd::owner_{};

bool LayerWnd::start(HWND owner)
{
    if (hook_) return true; // 已在放置模式

    owner_ = owner;
    hook_ = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
    if (!hook_) {
        owner_ = nullptr;
        return false;
    }
    SetCursor(placePinCursor());
    return true;
}

void LayerWnd::cancel()
{
    if (hook_) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
        owner_ = nullptr;
        // 立即恢复光标（系统会在鼠标移动时自然接管）
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    }
}

bool LayerWnd::active()
{
    return hook_ != nullptr;
}

LRESULT CALLBACK LayerWnd::mouseHook(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        const auto* ms = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        switch (wParam) {
        case WM_MOUSEMOVE:
            // 持续保持图钉光标；返回 1 吞掉事件，防止目标窗口的
            // WM_SETCURSOR 把光标重置回箭头。
            SetCursor(placePinCursor());
            return 1;
        case WM_LBUTTONDOWN: {
            // 用户点击目标：把屏幕坐标交给 App 放置图钉。
            // 吞掉点击本身（返回 1），避免真实点击落在目标窗口上。
            PostMessageW(owner_, App::WM_PINREQ, ms->pt.x, ms->pt.y);
            cancel();
            return 1;
        }
        case WM_RBUTTONDOWN:
            // 右键取消放置模式
            cancel();
            return 1;
        default:
            break;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

#include "layer.h"
#include "app.h"
#include "resource.h"

#include <windowsx.h>

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
    // 临时把系统箭头光标替换为图钉：任何窗口的 WM_SETCURSOR 设置箭头时
    // 都会显示图钉，无需依赖捕获/窗口消息，光标稳定保持。
    if (HCURSOR pin = CopyIcon(placePinCursor())) {
        SetSystemCursor(pin, OCR_NORMAL);
    }
    return true;
}

void LayerWnd::cancel()
{
    if (hook_) {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
        owner_ = nullptr;
        // 恢复全部系统光标（含被替换的箭头）
        SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0);
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
            // 其余事件（移动/滚轮等）一律放行，绝不拦截系统鼠标
            break;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

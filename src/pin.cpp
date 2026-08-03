#include "pin.h"
#include "resource.h"

#include <dwmapi.h>

#include <algorithm>
#include <cstring> // memcpy

namespace {

// 按 DPI 选择图钉位图尺寸：标准(96)~144 DPI 用 16px，144~192 用 24px，更高用 32px。
int pickPinSize(int dpi)
{
    if (dpi <= 120) return 16;
    if (dpi <= 168) return 24;
    return 32;
}

// 从 RCDATA 资源加载预生成好的 premultiplied BGRA（顶向下，尺寸固定）。
// 数据由 tools/gen_pin_assets.py 生成，颜色/预乘在生成时保证，运行时无解码。
HBITMAP loadPinRaw(int resId, int w, int h)
{
    HRSRC hrs = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hrs) return nullptr;
    HGLOBAL hglob = LoadResource(nullptr, hrs);
    if (!hglob) return nullptr;
    const void* data = LockResource(hglob);
    const DWORD len   = SizeofResource(nullptr, hrs);
    if (!data || len != static_cast<DWORD>(w) * h * 4) return nullptr;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; // 顶向下，与数据行序一致
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        return nullptr;
    }
    memcpy(bits, data, len);
    return dib;
}

// 计算图钉位置：优先对准标题栏"最小化"按钮（DWMWA_CAPTION_BUTTON_BOUNDS），
// 无边框窗口取不到时退回右上角。
POINT getPinPos(HWND target, const SIZE& size)
{
    POINT pos{};
    RECT btn{};
    const HRESULT hr = DwmGetWindowAttribute(target, DWMWA_CAPTION_BUTTON_BOUNDS,
                                             &btn, sizeof(btn));
    if (SUCCEEDED(hr) && btn.right > btn.left && btn.bottom > btn.top) {
        // bounds 是"最小化+最大化+关闭"三个按钮的并集，最小化按钮取最左 1/3
        const int btnW = (btn.right - btn.left) / 3;
        const int cx   = btn.left + btnW / 2;
        const int cy   = (btn.top + btn.bottom) / 2;
        pos.x = cx - size.cx / 2;
        pos.y = cy - size.cy / 2;
        return pos;
    }

    // 兜底：标题栏右上角
    RECT rc{};
    GetWindowRect(target, &rc);
    const int captionH = GetSystemMetrics(SM_CYCAPTION);
    pos.x = rc.right - size.cx - 8;
    pos.y = rc.top + std::max(4, static_cast<int>(captionH - size.cy) / 2);
    return pos;
}

} // namespace

HINSTANCE PinWnd::hInst_{};
std::unordered_map<HWND, PinWnd*> PinWnd::all_{};

void PinWnd::setHInstance(HINSTANCE hInst)
{
    hInst_ = hInst;
}

PinWnd::PinWnd(HWND target) : target_(target) {}

PinWnd::~PinWnd()
{
    if (hwnd_ && IsWindow(hwnd_)) {
        DestroyWindow(hwnd_);
    }
    if (bmp_) {
        DeleteObject(bmp_);
    }
}

bool PinWnd::init()
{
    // 惰性注册窗口类
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = wndProc;
        wc.hInstance     = hInst_;
        wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"PinTopPinWnd";
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }

    const int dpi = GetDpiForWindow(target_);
    const int px  = pickPinSize(dpi);
    size_.cx = size_.cy = px;
    bmp_ = loadPinRaw(px == 32 ? IDR_PIN32 : (px == 24 ? IDR_PIN24 : IDR_PIN16), px, px);
    if (!bmp_) return false;

    const POINT pos = getPinPos(target_, size_);
    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"PinTopPinWnd", L"", WS_POPUP | WS_VISIBLE,
        pos.x, pos.y, size_.cx, size_.cy, nullptr, nullptr, hInst_, this);
    if (!hwnd_) return false;

    // 分层绘制：32bpp premultiplied BGRA + ULW_ALPHA，原样显示图标（真 alpha 抗锯齿）。
    // hdcDst 必须是屏幕 DC（GetDC(nullptr)），用窗口 DC 会导致更新失败。
    HDC screenDC = GetDC(nullptr);
    HDC memDC    = CreateCompatibleDC(screenDC);
    HGDIOBJ old  = SelectObject(memDC, bmp_);
    POINT dst{ pos.x, pos.y };
    POINT src{ 0, 0 };
    BLENDFUNCTION blend{};
    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat         = AC_SRC_ALPHA;
    const BOOL ok = UpdateLayeredWindow(hwnd_, screenDC, &dst, &size_, memDC, &src,
                                        0, &blend, ULW_ALPHA);
    SelectObject(memDC, old);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
    if (!ok) return false;
    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    return true;
}

bool PinWnd::create(HWND target)
{
    if (!target || !IsWindow(target)) return false;
    if (find(target)) return true; // 已置顶，幂等

    auto* pin = new PinWnd(target);
    // 先置顶目标窗口，再创建图钉窗口：同为 TOPMOST 时后创建者 Z 序更靠前，
    // 图钉窗口才能盖在目标窗口标题栏上（顺序反了图钉会被目标窗口遮住）。
    SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (!pin->init()) {
        delete pin;
        // 回滚：图钉创建失败时撤销目标窗口的置顶，避免无法取消的残留
        SetWindowPos(target, HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return false;
    }
    all_.emplace(target, pin);
    return true;
}

void PinWnd::remove(HWND target)
{
    PinWnd* pin = find(target);
    if (!pin) return;
    all_.erase(target);
    // 取消置顶（NOTOPMOST 后窗口回到普通 Z 序）。目标可能已销毁（DESTROY 事件路径），需守卫。
    if (IsWindow(target)) {
        SetWindowPos(target, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    delete pin;
}

void PinWnd::removeAll()
{
    for (auto& [hwnd, pin] : all_) {
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        delete pin;
    }
    all_.clear();
}

void PinWnd::repositionFor(HWND target)
{
    PinWnd* pin = find(target);
    if (!pin) return;
    if (!IsWindow(target)) {
        remove(target); // 目标已销毁
        return;
    }
    if (IsIconic(target)) {
        // 最小化：保持置顶状态，只隐藏图钉（恢复时重新显示）
        ShowWindow(pin->hwnd_, SW_HIDE);
        return;
    }
    if (!IsWindowVisible(target)) {
        remove(target); // 真正隐藏（非最小化）→ 解除置顶
        return;
    }

    ShowWindow(pin->hwnd_, SW_SHOWNOACTIVATE);
    const POINT pos = getPinPos(target, pin->size_);
    // 保持图钉在 TOPMOST 层顶部（目标窗口置顶后可能遮挡图钉）
    SetWindowPos(pin->hwnd_, HWND_TOPMOST, pos.x, pos.y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE);
}

bool PinWnd::isPinWindow(HWND hwnd)
{
    for (auto& [t, pin] : all_) {
        if (pin->hwnd_ == hwnd) return true;
    }
    return false;
}

bool PinWnd::isPinned(HWND target)
{
    return find(target) != nullptr;
}

PinWnd* PinWnd::find(HWND target)
{
    auto it = all_.find(target);
    return it == all_.end() ? nullptr : it->second;
}

LRESULT CALLBACK PinWnd::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PinWnd* self = reinterpret_cast<PinWnd*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    switch (msg) {
    case WM_LBUTTONDOWN: {
        // 点击图钉：先记录目标再统一移除，避免在迭代 all_ 期间修改容器
        HWND target = nullptr;
        for (auto& [t, pin] : all_) {
            if (pin->hwnd_ == hwnd) {
                target = t;
                break;
            }
        }
        if (target) remove(target);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

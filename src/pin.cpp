#include "pin.h"
#include "resource.h"

#include <wincodec.h>
#include <objidl.h>
#include <dwmapi.h>

#include <algorithm>

namespace {

// WICPixelFormat32bppBGRA 的 GUID（SDK 文档值）。
// 不用 SDK 头里的同名宏，避免 wincodec.h 的 WIC_GUID_NO_INIT 宏开关差异。
constexpr GUID kPxFormat32bppBGRA{
    0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x13}};

// 按 DPI 选择图钉位图尺寸：标准(96)~144 DPI 用 16px，144~192 用 24px，更高用 32px。
int pickPinSize(int dpi)
{
    if (dpi <= 120) return 16;
    if (dpi <= 168) return 24;
    return 32;
}

// 从 RCDATA 资源解码 PNG → 32bpp BGRA 位图，并用 alpha 生成异形区域。
// 透明像素被替换为品红（颜色键，仅作绘制时不影响形状），HRGN 由窗口接管。
// 返回的 HBITMAP 由调用者 DeleteObject；*outRgn 为新建区域（失败为 nullptr）。
HBITMAP loadPinPng(int resId, SIZE& outSize, HRGN& outRgn)
{
    outRgn = nullptr;
    HRSRC hrs = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hrs) return nullptr;
    HGLOBAL hglob = LoadResource(nullptr, hrs);
    if (!hglob) return nullptr;
    const void* data = LockResource(hglob);
    const DWORD len   = SizeofResource(nullptr, hrs);
    if (!data || len == 0) return nullptr;

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &stream))) return nullptr;
    ULONG written = 0;
    if (FAILED(stream->Write(data, len, &written))) {
        stream->Release();
        return nullptr;
    }
    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder  = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv   = nullptr;
    HBITMAP result = nullptr;
    BITMAPINFO bmi{};

    do {
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_PPV_ARGS(&factory));
        if (FAILED(hr)) break;
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand,
                                              &decoder);
        if (FAILED(hr)) break;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) break;
        hr = factory->CreateFormatConverter(&conv);
        if (FAILED(hr)) break;
        // 直通 BGRA（不做预乘——本方案用 HRGN + BitBlt，不需要预乘）
        hr = conv->Initialize(frame, kPxFormat32bppBGRA,
                              WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) break;

        UINT w = 0, h = 0;
        if (FAILED(conv->GetSize(&w, &h)) || w == 0 || h == 0) break;
        outSize.cx = static_cast<int>(w);
        outSize.cy = static_cast<int>(h);

        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = static_cast<LONG>(w);
        bmi.bmiHeader.biHeight      = -static_cast<LONG>(h); // 顶向下
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) {
            if (dib) DeleteObject(dib);
            break;
        }
        const UINT stride = w * 4;
        hr = conv->CopyPixels(nullptr, stride, stride * h, static_cast<BYTE*>(bits));
        if (FAILED(hr)) {
            DeleteObject(dib);
            break;
        }

        // 用 alpha 构建异形区域（按行合并连续段），透明像素置品红
        auto* px = static_cast<BYTE*>(bits);
        HRGN rgn = CreateRectRgn(0, 0, 0, 0);
        if (!rgn) {
            DeleteObject(dib);
            break;
        }
        for (UINT y = 0; y < h; ++y) {
            UINT x = 0;
            while (x < w) {
                const BYTE a = px[y * stride + x * 4 + 3];
                if (a == 0) {
                    // 透明 → 品红颜色键
                    px[y * stride + x * 4 + 0] = 0xFF;
                    px[y * stride + x * 4 + 1] = 0x00;
                    px[y * stride + x * 4 + 2] = 0xFF;
                    ++x;
                    continue;
                }
                // 连续不透明段 → 一个矩形
                UINT x2 = x + 1;
                while (x2 < w && px[y * stride + x2 * 4 + 3] != 0) ++x2;
                HRGN seg = CreateRectRgn(static_cast<int>(x), static_cast<int>(y),
                                         static_cast<int>(x2), static_cast<int>(y) + 1);
                if (seg) {
                    CombineRgn(rgn, rgn, seg, RGN_OR);
                    DeleteObject(seg);
                }
                x = x2;
            }
        }
        outRgn = rgn;
        result = dib;
    } while (false);

    if (conv) conv->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (stream) stream->Release();
    return result;
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
        wc.hbrBackground = nullptr; // 不预填充背景，由 WM_PAINT 画位图
        wc.lpszClassName = L"PinTopPinWnd";
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }

    const int dpi = GetDpiForWindow(target_);
    const int px  = pickPinSize(dpi);
    HRGN rgn = nullptr;
    bmp_ = loadPinPng(px == 32 ? IDR_PIN32 : (px == 24 ? IDR_PIN24 : IDR_PIN16), size_, rgn);
    if (!bmp_ || !rgn) {
        if (rgn) DeleteObject(rgn);
        return false;
    }

    const POINT pos = getPinPos(target_, size_);
    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"PinTopPinWnd", L"", WS_POPUP | WS_VISIBLE,
        pos.x, pos.y, size_.cx, size_.cy, nullptr, nullptr, hInst_, this);
    if (!hwnd_) {
        DeleteObject(rgn);
        return false;
    }

    // 窗口接管区域（销毁时自动释放），实例挂到窗口 userdata 供 wndProc 使用
    SetWindowRgn(hwnd_, rgn, TRUE);
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
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, self->bmp_);
        BitBlt(dc, 0, 0, self->size_.cx, self->size_.cy, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
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

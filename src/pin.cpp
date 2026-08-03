#include "pin.h"
#include "resource.h"

#include <wincodec.h>
#include <objidl.h>

#include <algorithm>

namespace {

// 按 DPI 选择图钉位图尺寸：标准(96)~144 DPI 用 16px，144~192 用 24px，更高用 32px。
int pickPinSize(int dpi)
{
    if (dpi <= 120) return 16;
    if (dpi <= 168) return 24;
    return 32;
}

// 从 RCDATA 资源解码 PNG → 32bpp premultiplied-alpha 位图（供 UpdateLayeredWindow 使用）。
// 返回的 HBITMAP 由调用者 DeleteObject。失败返回 nullptr。
HBITMAP loadPinPng(int resId, SIZE& outSize)
{
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
    HRESULT hr = stream->Write(data, len, &written);
    if (FAILED(hr)) {
        stream->Release();
        return nullptr;
    }
    // 回到流起点
    LARGE_INTEGER zero{};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder  = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv   = nullptr;
    HBITMAP result = nullptr;
    BITMAPINFO bmi{};

    do {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory));
        if (FAILED(hr)) break;
        hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand,
                                              &decoder);
        if (FAILED(hr)) break;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) break;
        hr = factory->CreateFormatConverter(&conv);
        if (FAILED(hr)) break;
        // 转成预乘 alpha BGRA —— UpdateLayeredWindow(AC_SRC_ALPHA) 的硬性要求
        hr = conv->Initialize(frame, &GUID_WICPixelFormat32bppPremultipliedBGRA,
                              WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) break;

        UINT w = 0, h = 0;
        if (FAILED(conv->GetSize(&w, &h)) || w == 0 || h == 0) break;
        outSize.cx = static_cast<int>(w);
        outSize.cy = static_cast<int>(h);

        // 创建 32bpp 顶向下 DIB
        bmi.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth    = static_cast<LONG>(w);
        bmi.bmiHeader.biHeight   = -static_cast<LONG>(h); // 顶向下
        bmi.bmiHeader.biPlanes   = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!dib || !bits) {
            if (dib) DeleteObject(dib);
            break;
        }
        // WIC 的 stride 是每行字节数（32bpp = w*4）
        const UINT stride = w * 4;
        hr = conv->CopyPixels(nullptr, stride, stride * h, static_cast<BYTE*>(bits));
        if (FAILED(hr)) {
            DeleteObject(dib);
            break;
        }
        result = dib;
    } while (false);

    if (conv) conv->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (stream) stream->Release();
    return result;
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
        wc.lpszClassName = L"PinTopPinWnd";
        if (!RegisterClassExW(&wc)) return false;
        registered = true;
    }

    const int dpi = GetDpiForWindow(target_);
    const int px  = pickPinSize(dpi);
    bmp_ = loadPinPng(px == 32 ? IDR_PIN32 : (px == 24 ? IDR_PIN24 : IDR_PIN16), size_);
    if (!bmp_) return false;

    // 图钉位置：目标窗口右上角标题栏内（右侧留边，避免压住系统按钮）
    RECT rc{};
    GetWindowRect(target_, &rc);
    const int captionH = GetSystemMetrics(SM_CYCAPTION);
    const int x = rc.right - size_.cx - 8;
    const int y = rc.top + std::max(4, (captionH - size_.cy) / 2);

    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"PinTopPinWnd", L"", WS_POPUP | WS_VISIBLE,
        x, y, size_.cx, size_.cy, nullptr, nullptr, hInst_, this);
    if (!hwnd_) return false;

    // 分层窗口：把位图通过 ULW_ALPHA 绘制（32bpp premultiplied alpha）
    HDC screenDC = GetDC(nullptr);
    HDC memDC    = CreateCompatibleDC(screenDC);
    HDC winDC    = GetDC(hwnd_);
    bool ok = false;
    if (screenDC && memDC && winDC) {
        HGDIOBJ old = SelectObject(memDC, bmp_);
        POINT dst{ x, y };
        POINT src{ 0, 0 };
        BLENDFUNCTION blend{};
        blend.BlendOp             = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat         = AC_SRC_ALPHA;
        ok = UpdateLayeredWindow(hwnd_, winDC, &dst, &size_, memDC, &src, 0, &blend, ULW_ALPHA);
        SelectObject(memDC, old);
    }
    if (winDC) ReleaseDC(hwnd_, winDC);
    if (memDC) DeleteDC(memDC);
    if (screenDC) ReleaseDC(nullptr, screenDC);
    return ok;
}

bool PinWnd::create(HWND target)
{
    if (!target || !IsWindow(target)) return false;
    if (find(target)) return true; // 已置顶，幂等

    auto* pin = new PinWnd(target);
    if (!pin->init()) {
        delete pin;
        return false;
    }
    // 置顶目标窗口（图钉窗口自身已是 WS_EX_TOPMOST）
    SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
    // 目标已销毁、隐藏或最小化 → 移除图钉。
    // 注意：最小化窗口仍带 WS_VISIBLE，IsWindowVisible 不可靠，需 IsIconic。
    if (!IsWindow(target) || !IsWindowVisible(target) || IsIconic(target)) {
        remove(target);
        return;
    }
    RECT rc{};
    GetWindowRect(target, &rc);
    const int captionH = GetSystemMetrics(SM_CYCAPTION);
    const int x = rc.right - pin->size_.cx - 8;
    const int y = rc.top + std::max(4, (captionH - pin->size_.cy) / 2);
    SetWindowPos(pin->hwnd_, nullptr, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
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
    switch (msg) {
    case WM_LBUTTONDOWN: {
        // 先记录目标再统一移除，避免在迭代 all_ 期间修改容器
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
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

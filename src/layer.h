#pragma once

#include <windows.h>

// 置顶放置模式：低级鼠标钩子（WH_MOUSE_LL）实现。
// 与旧版 1×1 捕获窗口不同：不依赖 SetCapture / 焦点 / 菜单模态，
// 钩子回调全局可靠收到鼠标事件，光标由钩子持续保持为图钉。
class LayerWnd {
public:
    // 进入放置模式（安装钩子 + 切换光标）。返回是否成功。
    static bool start(HWND owner);
    // 取消放置模式（卸载钩子 + 恢复光标）。
    static void cancel();
    static bool active();

private:
    static LRESULT CALLBACK mouseHook(int nCode, WPARAM wParam, LPARAM lParam);

    static HHOOK hook_;
    static HWND owner_;
};

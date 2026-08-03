#include "app.h"

// 入口：无控制台窗口（WIN32 子系统），直接进入消息循环。
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int)
{
    return App::instance().run(hInstance);
}

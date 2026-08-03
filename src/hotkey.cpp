#include "hotkey.h"

bool HotKey::registerDefault(HWND owner)
{
    return RegisterHotKey(owner, ID, MODS, VK) != FALSE;
}

void HotKey::unregister(HWND owner)
{
    UnregisterHotKey(owner, ID);
}

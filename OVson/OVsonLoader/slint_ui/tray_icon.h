#pragma once
#include <Windows.h>
#include <functional>

namespace Tray {

void install(HWND owner,
             std::function<void()> onShow,
             std::function<void()> onQuit);

void notify(const wchar_t *title, const wchar_t *body);

void uninstall();

} // namespace Tray

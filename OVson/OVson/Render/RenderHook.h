#pragma once
#include <functional>

namespace RenderHook {
bool install();
void uninstall();
void poll();

// True after uninstall() decided it could not safely remove one of our
// hooks because another injected module chained onto it. When this is set,
// code in this module is still reachable from that other module, so the
// DLL must NOT be unloaded -- doing so leaves them jumping into unmapped
// memory. Leaking the module until the game exits is the cheap outcome.
bool mustStayLoaded();

// Raw HWND of the game window RenderHook subclassed (null before install,
// or if the window went away). Returned as void* so callers do not have to
// drag <Windows.h> in through this header. Launchers such as Badlion can
// own the top-level foreground window from a *different* process while this
// surface still belongs to us, so anything that wants to ask "is the game
// focused?" has to compare against this rather than trusting the PID of
// GetForegroundWindow().
void *gameWindowHandle();

void enqueueTask(std::function<void()> task);
float getDelta();
} // namespace RenderHook

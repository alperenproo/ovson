#pragma once
#include <functional>

namespace RenderHook {
bool install();
void uninstall();
void poll();

bool mustStayLoaded();

void *gameWindowHandle();

void enqueueTask(std::function<void()> task);
float getDelta();
} // namespace RenderHook

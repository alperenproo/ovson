#pragma once
#include <string>

namespace NumberDenicker {

void onChatMessage(const std::string &message);
void onWorldChange();
bool isEnabled();
} // namespace NumberDenicker

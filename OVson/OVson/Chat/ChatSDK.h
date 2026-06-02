#pragma once
#include <string>
#include "../SDK/Minecraft.h"

namespace ChatSDK {
	bool sendClientChat(const std::string& text);
	bool showClientMessage(const std::string& text);
	bool showJsonMessage(const std::string& json,
	                     const std::string& fallback = std::string());
	std::string formatPrefix();
	bool showPrefixed(const std::string& message);
	bool showPrefixedf(const char* fmt, ...);
	void initialize();
}



#pragma once
#include <string>
#include "../SDK/Minecraft.h"

namespace ChatSDK {
	bool sendClientChat(const std::string& text);
	bool showClientMessage(const std::string& text);
	bool showJsonMessage(const std::string& json,
	                     const std::string& fallback = std::string());
	bool showTagsMessage(const std::string& msg,
	                     const std::vector<std::pair<std::string, std::string>>& tags);
	bool showActionBar(const std::string& text);
	bool showTitle(const std::string& title, const std::string& subtitle,
	               int fadeIn, int stay, int fadeOut);
	bool clearChat();
	std::vector<std::string> getChatHistory(int maxCount);
	std::vector<std::string> getSentHistory(int maxCount);

	std::string formatPrefix();
	bool showPrefixed(const std::string& message);
	bool showPrefixedf(const char* fmt, ...);
	void initialize();
}



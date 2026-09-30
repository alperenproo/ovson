#pragma once
#include <string>

namespace ChatHook {
	bool install();
	void uninstall();
	bool onClientSendMessage(const std::string& message);
	std::string processIncomingChat(const std::string& unformattedText, const std::string& rawJson);
	bool wasMessageSentRecentlyBySelf(const std::string& message);
}


#pragma once

#include <string>
#include <vector>

namespace IrcService {

struct IrcUser {
    std::string username;
    std::string rankTitle;
    std::string rankColor;
};

void initialize();
void shutdown(bool wait = false);

bool isConnected();
bool isMuted();
void setMuted(bool muted);
void toggleMuted();

bool isAppearOffline();
void setAppearOffline(bool offline);
void toggleAppearOffline();

void reconnect();
void checkPlayerNameRefresh(bool forceWorldChange = false);
std::string getCurrentUsername();

void sendMessage(const std::string& text);

void sendDirectMessage(const std::string& target, const std::string& text);

void requestUserList();

struct CustomRank {
    std::string rankTitle;
    std::string rankColor;
    std::string nameColor;
};

void setCustomRank(const std::string& username, const std::string& rankTitle, const std::string& rankColor, const std::string& nameColor = "");
void clearCustomRanks();
void loadCustomRanksFromJson(const std::string& json);
void fetchCustomRanksFromUrl(const std::string& url);
void syncCustomRanks();

} // namespace IrcService

#define WIN32_LEAN_AND_MEAN
#include "IrcService.h"

#include <windows.h>
#include <mmsystem.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "../Chat/ChatSDK.h"
#include "../Config/Config.h"
#include "../Render/RenderHook.h"
#include "../Utils/Logger.h"
#include "../Logic/StatsTracker.h"
#include "../Logic/StatsTracker.internal.h"
#include "../Java.h"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winmm.lib")

namespace IrcService {

static std::atomic<bool> s_running{false};
static std::atomic<bool> s_connected{false};
static std::atomic<bool> s_muted{false};
static std::atomic<bool> s_appearOffline{false};
static std::atomic<uint64_t> s_sessionGen{0};

static std::thread s_workerThread;
static std::thread s_senderThread;

static std::mutex s_sendMutex;
static std::condition_variable s_sendCv;
static std::queue<std::string> s_sendQueue;

static std::condition_variable s_reconnectCv;
static std::mutex s_reconnectMutex;

static std::mutex s_currentUsernameMutex;
static std::string s_currentIrcUsername;
static std::chrono::steady_clock::time_point s_lastPeriodicCheck{};

std::string getCurrentUsername() {
    std::lock_guard<std::mutex> lock(s_currentUsernameMutex);
    return s_currentIrcUsername;
}

static void setCurrentUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(s_currentUsernameMutex);
    s_currentIrcUsername = username;
}

static std::recursive_mutex s_handleMutex;
static HINTERNET s_hSession = nullptr;
static HINTERNET s_hConnect = nullptr;
static HINTERNET s_hRequest = nullptr;
static HINTERNET s_hWebSocket = nullptr;

static void interruptibleSleep(int seconds, uint64_t myGen) {
    std::unique_lock<std::mutex> lock(s_reconnectMutex);
    s_reconnectCv.wait_for(lock, std::chrono::seconds(seconds), [myGen] {
        return !s_running.load() || !Config::isIrcEnabled() || s_sessionGen.load() != myGen;
    });
}

static std::string escapeJson(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 16);
    for (char c : in) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\b') out += "\\b";
        else if (c == '\f') out += "\\f";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

static std::string extractJsonString(const std::string& json, const std::string& key) {
    if (json.empty() || key.empty()) return "";
    try {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return "";

        pos += needle.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

        if (pos >= json.length()) return "";

        if (json[pos] == '"') {
            pos++;
            std::string res;
            res.reserve(64);
            while (pos < json.length()) {
                if (json[pos] == '\\' && pos + 1 < json.length()) {
                    char esc = json[pos + 1];
                    if (esc == 'u' && pos + 5 < json.length()) {
                        try {
                            std::string hexStr = json.substr(pos + 2, 4);
                            unsigned long cp = std::stoul(hexStr, nullptr, 16);
                            if (cp < 0x80) {
                                res += (char)cp;
                            } else if (cp < 0x800) {
                                res += (char)(0xC0 | (cp >> 6));
                                res += (char)(0x80 | (cp & 0x3F));
                            } else {
                                res += (char)(0xE0 | (cp >> 12));
                                res += (char)(0x80 | ((cp >> 6) & 0x3F));
                                res += (char)(0x80 | (cp & 0x3F));
                            }
                            pos += 6;
                            continue;
                        } catch (...) {}
                    }
                    if (esc == 'n') res += '\n';
                    else if (esc == 'r') res += '\r';
                    else if (esc == 't') res += '\t';
                    else if (esc == '"') res += '"';
                    else if (esc == '\\') res += '\\';
                    else res += esc;
                    pos += 2;
                } else if (json[pos] == '"') {
                    break;
                } else {
                    res += json[pos++];
                }
            }
            return res;
        }
    } catch (...) {}
    return "";
}

static bool extractJsonBool(const std::string& json, const std::string& key, bool defaultVal = false) {
    if (json.empty() || key.empty()) return defaultVal;
    try {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return defaultVal;

        pos += needle.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.length()) return defaultVal;

        if (json.compare(pos, 4, "true") == 0) return true;
        if (json.compare(pos, 5, "false") == 0) return false;
        if (json[pos] == '1') return true;
        if (json[pos] == '0') return false;
        if (json[pos] == '"') {
            if (json.compare(pos, 6, "\"true\"") == 0) return true;
            if (json.compare(pos, 7, "\"false\"") == 0) return false;
            if (json.compare(pos, 3, "\"1\"") == 0) return true;
            if (json.compare(pos, 3, "\"0\"") == 0) return false;
        }
    } catch (...) {}
    return defaultVal;
}

static int extractJsonInt(const std::string& json, const std::string& key, int defaultVal = 0) {
    if (json.empty() || key.empty()) return defaultVal;
    try {
        std::string needle = "\"" + key + "\":";
        size_t pos = json.find(needle);
        if (pos == std::string::npos) return defaultVal;

        pos += needle.length();
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.length()) return defaultVal;

        return std::stoi(json.substr(pos));
    } catch (...) {
        return defaultVal;
    }
}

static std::string computeHmacSha256(const std::string& key, const std::string& data) {
    try {
        BCRYPT_ALG_HANDLE hAlg = nullptr;
        BCRYPT_HASH_HANDLE hHash = nullptr;
        std::string resultHex;

        if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) == 0) {
            DWORD hashObjSize = 0, cbData = 0;
            if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&hashObjSize, sizeof(DWORD), &cbData, 0) == 0) {
                std::vector<BYTE> hashObj(hashObjSize);
                if (BCryptCreateHash(hAlg, &hHash, hashObj.data(), hashObjSize, (PBYTE)key.data(), (ULONG)key.size(), 0) == 0) {
                    if (BCryptHashData(hHash, (PBYTE)data.data(), (ULONG)data.size(), 0) == 0) {
                        BYTE hashBuf[32];
                        if (BCryptFinishHash(hHash, hashBuf, sizeof(hashBuf), 0) == 0) {
                            std::ostringstream oss;
                            for (int i = 0; i < 32; ++i) {
                                oss << std::hex << std::setw(2) << std::setfill('0') << (int)hashBuf[i];
                            }
                            resultHex = oss.str();
                        }
                    }
                    BCryptDestroyHash(hHash);
                }
            }
            BCryptCloseAlgorithmProvider(hAlg, 0);
        }
        return resultHex;
    } catch (...) {
        return "";
    }
}

static std::string generateNonce() {
    try {
        BYTE rnd[16]{};
        BCryptGenRandom(nullptr, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        std::ostringstream oss;
        for (int i = 0; i < 16; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << (int)rnd[i];
        }
        return oss.str();
    } catch (...) {
        return "fallbacknonce1234";
    }
}

static void printToChat(const std::string& message) {
    if (message.empty()) return;
    std::string safeMsg = message;
    if (safeMsg.size() > 2048) safeMsg.resize(2048);
    RenderHook::enqueueTask([safeMsg]() {
        try {
            ChatSDK::showClientMessage(safeMsg);
        } catch (...) {}
    });
}

static std::atomic<bool> s_dingPending{false};
static std::atomic<ULONGLONG> s_lastDingPlayTime{0};

static void playDingSound() {
    if (!Config::isIrcDingEnabled()) return;

    if (s_dingPending.exchange(true)) {
        return;
    }

    ULONGLONG requestTime = GetTickCount64();

    RenderHook::enqueueTask([requestTime]() {
        struct DingCleanup {
            ~DingCleanup() { s_dingPending.store(false); }
        } _cleanup;

        try {
            ULONGLONG now = GetTickCount64();

            if (now >= requestTime && (now - requestTime) > 2500) {
                return;
            }

            if (now >= s_lastDingPlayTime.load() && (now - s_lastDingPlayTime.load()) < 500) {
                return;
            }
            s_lastDingPlayTime.store(now);

            JNIEnv* env = lc ? lc->getEnv() : nullptr;
            if (!env) return;
            if (env->ExceptionCheck()) env->ExceptionClear();
            CMinecraft mc;
            CPlayer player = mc.GetLocalPlayer();
            if (!player.Get()) return;
            jclass entityClass = lc->GetClass("net.minecraft.entity.Entity");
            if (entityClass) {
                jmethodID sound = lc->GetMethodID(entityClass, "playSound",
                                                  "(Ljava/lang/String;FF)V",
                                                  "func_85030_a", "a");
                if (sound) {
                    jstring sName = Lunar::createSafeJString(env, "random.orb");
                    if (sName) {
                        env->CallVoidMethod(player.Get(), sound, sName, 0.6f, 1.25f);
                        if (env->ExceptionCheck()) env->ExceptionClear();
                        env->DeleteLocalRef(sName);
                    }
                }
            }
            player.Cleanup();
        } catch (...) {}
    });
}

static std::mutex s_ranksMutex;
static std::unordered_map<std::string, CustomRank> s_customRanks;

static std::string toLowerStr(const std::string& s) {
    std::string l = s;
    for (char &c : l) c = (char)tolower((unsigned char)c);
    return l;
}

static char findClosestMinecraftColor(int r, int g, int b) {
    static const struct { int r, g, b; char code; } mcColors[16] = {
        {   0,   0,   0, '0' },
        {   0,   0, 170, '1' },
        {   0, 170,   0, '2' },
        {   0, 170, 170, '3' },
        { 170,   0,   0, '4' },
        { 170,   0, 170, '5' },
        { 255, 170,   0, '6' },
        { 170, 170, 170, '7' },
        {  85,  85,  85, '8' },
        {  85,  85, 255, '9' },
        {  85, 255,  85, 'a' },
        {  85, 255, 255, 'b' },
        { 255,  85,  85, 'c' },
        { 255,  85, 255, 'd' },
        { 255, 255,  85, 'e' },
        { 255, 255, 255, 'f' }
    };
    int maxC = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    int minC = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    bool isSaturated = (maxC - minC) > 20;

    long bestDist = 0x7FFFFFFFL;
    char bestCode = 'f';
    for (int i = 0; i < 16; ++i) {
        long rmean = ((long)r + mcColors[i].r) / 2;
        long dr = (long)r - mcColors[i].r;
        long dg = (long)g - mcColors[i].g;
        long db = (long)b - mcColors[i].b;
        long dist = (((512 + rmean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rmean) * db * db) >> 8);

        if (isSaturated && (mcColors[i].code == '0' || mcColors[i].code == '7' || mcColors[i].code == '8' || mcColors[i].code == 'f')) {
            dist += 50000;
        }

        if (dist < bestDist) {
            bestDist = dist;
            bestCode = mcColors[i].code;
        }
    }
    return bestCode;
}

static bool tryParseHexColor(const std::string& str, int& outR, int& outG, int& outB) {
    std::string s = str;
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) s = s.substr(2);
    else if (!s.empty() && s[0] == '#') s = s.substr(1);

    if (s.size() == 6) {
        for (char c : s) {
            if (!isxdigit((unsigned char)c)) return false;
        }
        try {
            unsigned long val = std::stoul(s, nullptr, 16);
            outR = (int)((val >> 16) & 0xFF);
            outG = (int)((val >> 8) & 0xFF);
            outB = (int)(val & 0xFF);
            return true;
        } catch (...) { return false; }
    } else if (s.size() == 3) {
        for (char c : s) {
            if (!isxdigit((unsigned char)c)) return false;
        }
        try {
            char rHex[3] = { s[0], s[0], '\0' };
            char gHex[3] = { s[1], s[1], '\0' };
            char bHex[3] = { s[2], s[2], '\0' };
            outR = (int)std::strtoul(rHex, nullptr, 16);
            outG = (int)std::strtoul(gHex, nullptr, 16);
            outB = (int)std::strtoul(bHex, nullptr, 16);
            return true;
        } catch (...) { return false; }
    }
    return false;
}

static std::string buildMinecraftFormatting(
    const std::string& colorInput,
    const std::string& formatInput,
    bool bold,
    bool italic,
    bool underline,
    bool strikethrough,
    bool obfuscated,
    const std::string& defaultColor = ""
) {
    bool hasColor = false;
    char colorCode = ' ';
    bool hasReset = false;
    bool isBold = bold;
    bool isItalic = italic;
    bool isUnderline = underline;
    bool isStrike = strikethrough;
    bool isObfuscated = obfuscated;

    auto processString = [&](const std::string& text) {
        if (text.empty()) return;

        for (size_t i = 0; i < text.size(); ++i) {
            char codeChar = 0;
            if ((unsigned char)text[i] == 0xC2 && i + 1 < text.size() && (unsigned char)text[i + 1] == 0xA7) {
                if (i + 2 < text.size()) {
                    codeChar = text[i + 2];
                    i += 2;
                }
            } else if (text[i] == '&' || (unsigned char)text[i] == 0xA7) {
                if (i + 1 < text.size()) {
                    codeChar = text[i + 1];
                    i += 1;
                }
            }
            if (codeChar != 0) {
                char lower = (char)tolower((unsigned char)codeChar);
                if ((lower >= '0' && lower <= '9') || (lower >= 'a' && lower <= 'f')) {
                    hasColor = true;
                    colorCode = lower;
                } else if (lower == 'k') {
                    isObfuscated = true;
                } else if (lower == 'l') {
                    isBold = true;
                } else if (lower == 'm') {
                    isStrike = true;
                } else if (lower == 'n') {
                    isUnderline = true;
                } else if (lower == 'o') {
                    isItalic = true;
                } else if (lower == 'r') {
                    hasReset = true;
                }
            }
        }

        std::string current;
        auto handleToken = [&](const std::string& tok) {
            if (tok.empty()) return;
            std::string t = toLowerStr(tok);
            while (!t.empty() && (t.front() == '"' || t.front() == '\'')) t.erase(t.begin());
            while (!t.empty() && (t.back() == '"' || t.back() == '\'')) t.pop_back();
            if (t.empty()) return;

            if (t == "bold") { isBold = true; return; }
            if (t == "italic" || t == "italics") { isItalic = true; return; }
            if (t == "underline" || t == "underlined") { isUnderline = true; return; }
            if (t == "strike" || t == "strikethrough") { isStrike = true; return; }
            if (t == "magic" || t == "obfuscated") { isObfuscated = true; return; }
            if (t == "reset") { hasReset = true; return; }

            if (t == "black") { hasColor = true; colorCode = '0'; return; }
            if (t == "dark_blue" || t == "darkblue" || t == "navy") { hasColor = true; colorCode = '1'; return; }
            if (t == "dark_green" || t == "darkgreen") { hasColor = true; colorCode = '2'; return; }
            if (t == "dark_aqua" || t == "darkaqua" || t == "cyan" || t == "teal") { hasColor = true; colorCode = '3'; return; }
            if (t == "dark_red" || t == "darkred" || t == "maroon") { hasColor = true; colorCode = '4'; return; }
            if (t == "dark_purple" || t == "darkpurple" || t == "purple") { hasColor = true; colorCode = '5'; return; }
            if (t == "gold" || t == "orange") { hasColor = true; colorCode = '6'; return; }
            if (t == "gray" || t == "grey") { hasColor = true; colorCode = '7'; return; }
            if (t == "dark_gray" || t == "darkgray" || t == "darkgrey") { hasColor = true; colorCode = '8'; return; }
            if (t == "blue") { hasColor = true; colorCode = '9'; return; }
            if (t == "green" || t == "lime") { hasColor = true; colorCode = 'a'; return; }
            if (t == "aqua" || t == "lightblue" || t == "light_blue") { hasColor = true; colorCode = 'b'; return; }
            if (t == "red") { hasColor = true; colorCode = 'c'; return; }
            if (t == "light_purple" || t == "lightpurple" || t == "pink" || t == "magenta") { hasColor = true; colorCode = 'd'; return; }
            if (t == "yellow") { hasColor = true; colorCode = 'e'; return; }
            if (t == "white") { hasColor = true; colorCode = 'f'; return; }

            if (t.size() == 1 && ((t[0] >= '0' && t[0] <= '9') || (t[0] >= 'a' && t[0] <= 'f'))) {
                hasColor = true;
                colorCode = t[0];
                return;
            }

            if (t.size() == 1) {
                if (t[0] == 'k') { isObfuscated = true; return; }
                if (t[0] == 'l') { isBold = true; return; }
                if (t[0] == 'm') { isStrike = true; return; }
                if (t[0] == 'n') { isUnderline = true; return; }
                if (t[0] == 'o') { isItalic = true; return; }
                if (t[0] == 'r') { hasReset = true; return; }
            }

            int r = 0, g = 0, b = 0;
            if (tryParseHexColor(t, r, g, b)) {
                hasColor = true;
                colorCode = findClosestMinecraftColor(r, g, b);
                return;
            }
        };

        for (char c : text) {
            if (c == ' ' || c == ',' || c == ';' || c == '+' || c == '|' || c == '\t' || c == '\r' || c == '\n') {
                if (!current.empty()) {
                    handleToken(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            handleToken(current);
        }
    };

    processString(colorInput);
    processString(formatInput);

    std::string result;
    if (hasReset) {
        result += "\xC2\xA7";
        result += "r";
    }

    if (hasColor) {
        result += "\xC2\xA7";
        result += colorCode;
    } else if (!defaultColor.empty()) {
        result += defaultColor;
    }

    if (isObfuscated) {
        result += "\xC2\xA7";
        result += "k";
    }
    if (isBold) {
        result += "\xC2\xA7";
        result += "l";
    }
    if (isStrike) {
        result += "\xC2\xA7";
        result += "m";
    }
    if (isUnderline) {
        result += "\xC2\xA7";
        result += "n";
    }
    if (isItalic) {
        result += "\xC2\xA7";
        result += "o";
    }

    return result;
}

static std::string normalizeColorCode(const std::string& input) {
    return buildMinecraftFormatting(input, "", false, false, false, false, false, "");
}

void setCustomRank(const std::string& username, const std::string& rankTitle, const std::string& rankColor, const std::string& nameColor) {
    std::lock_guard<std::mutex> lock(s_ranksMutex);
    s_customRanks[toLowerStr(username)] = { rankTitle, normalizeColorCode(rankColor), normalizeColorCode(nameColor) };
}

void clearCustomRanks() {
    std::lock_guard<std::mutex> lock(s_ranksMutex);
    s_customRanks.clear();
}

void loadCustomRanksFromJson(const std::string& json) {
    if (json.empty()) return;
    try {
        std::lock_guard<std::mutex> lock(s_ranksMutex);
        size_t pos = 0;
        while ((pos = json.find('{', pos)) != std::string::npos) {
            int depth = 1;
            size_t closeBrace = pos + 1;
            while (closeBrace < json.size() && depth > 0) {
                if (json[closeBrace] == '{') depth++;
                else if (json[closeBrace] == '}') depth--;
                if (depth == 0) break;
                closeBrace++;
            }
            if (depth != 0) break;

            std::string keyBefore;
            if (pos > 0) {
                int p = (int)pos - 1;
                while (p >= 0 && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n')) p--;
                if (p >= 0 && json[p] == ':') {
                    p--;
                    while (p >= 0 && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n')) p--;
                    if (p >= 0 && json[p] == '"') {
                        int qEnd = p;
                        int qStart = qEnd - 1;
                        while (qStart >= 0 && json[qStart] != '"') qStart--;
                        if (qStart >= 0) {
                            keyBefore = json.substr(qStart + 1, qEnd - qStart - 1);
                        }
                    }
                }
            }

            std::string sub = json.substr(pos, closeBrace - pos + 1);

            std::string u = extractJsonString(sub, "username");
            if (u.empty()) u = extractJsonString(sub, "user");
            if (u.empty()) u = extractJsonString(sub, "name");
            if (u.empty() && !keyBefore.empty()) {
                std::string lk = toLowerStr(keyBefore);
                if (lk != "ranks" && lk != "users" && lk != "players" && lk != "data") {
                    u = keyBefore;
                }
            }

            if (!u.empty()) {
                std::string title = extractJsonString(sub, "rank");
                if (title.empty()) title = extractJsonString(sub, "title");
                if (title.empty()) title = extractJsonString(sub, "prefix");

                std::string col = extractJsonString(sub, "rankColor");
                if (col.empty()) col = extractJsonString(sub, "color");
                if (col.empty()) col = extractJsonString(sub, "prefixColor");

                std::string ncol = extractJsonString(sub, "nameColor");
                if (ncol.empty()) ncol = extractJsonString(sub, "playerColor");
                if (ncol.empty()) ncol = extractJsonString(sub, "userColor");

                bool b = extractJsonBool(sub, "bold");
                bool it_flag = extractJsonBool(sub, "italic") || extractJsonBool(sub, "italics");
                bool u_flag = extractJsonBool(sub, "underline") || extractJsonBool(sub, "underlined");
                bool s_flag = extractJsonBool(sub, "strike") || extractJsonBool(sub, "strikethrough");
                bool k_flag = extractJsonBool(sub, "magic") || extractJsonBool(sub, "obfuscated");
                std::string fmt = extractJsonString(sub, "format");
                if (fmt.empty()) fmt = extractJsonString(sub, "rankFormat");

                bool nb = extractJsonBool(sub, "nameBold") || extractJsonBool(sub, "playerBold");
                bool nit = extractJsonBool(sub, "nameItalic") || extractJsonBool(sub, "playerItalic");
                bool nu = extractJsonBool(sub, "nameUnderline") || extractJsonBool(sub, "nameUnderlined") || extractJsonBool(sub, "playerUnderline");
                bool ns = extractJsonBool(sub, "nameStrike") || extractJsonBool(sub, "nameStrikethrough") || extractJsonBool(sub, "playerStrike");
                bool nk = extractJsonBool(sub, "nameMagic") || extractJsonBool(sub, "nameObfuscated") || extractJsonBool(sub, "playerMagic");
                std::string nfmt = extractJsonString(sub, "nameFormat");
                if (nfmt.empty()) nfmt = extractJsonString(sub, "playerFormat");

                std::string finalRankColor = buildMinecraftFormatting(col, fmt, b, it_flag, u_flag, s_flag, k_flag, "\xC2\xA7" "b");
                std::string finalNameColor = "";
                if (!ncol.empty() || nb || nit || nu || ns || nk || !nfmt.empty()) {
                    finalNameColor = buildMinecraftFormatting(ncol, nfmt, nb, nit, nu, ns, nk, "\xC2\xA7" "7");
                }

                if (!title.empty() || !finalNameColor.empty()) {
                    s_customRanks[toLowerStr(u)] = { title, finalRankColor, finalNameColor };
                }
            }
            pos = pos + 1;
        }
    } catch (...) {}
}

void fetchCustomRanksFromUrl(const std::string& url);

struct ParsedUrl {
    std::wstring host;
    INTERNET_PORT port;
    std::wstring path;
    bool isSecure;
};

static bool parseWsUrl(const std::string& urlStr, ParsedUrl& out) {
    try {
        std::string s = urlStr;
        bool secure = true;
        if (s.rfind("wss://", 0) == 0) {
            secure = true;
            s = s.substr(6);
        } else if (s.rfind("ws://", 0) == 0) {
            secure = false;
            s = s.substr(5);
        } else if (s.rfind("https://", 0) == 0) {
            secure = true;
            s = s.substr(8);
        } else if (s.rfind("http://", 0) == 0) {
            secure = false;
            s = s.substr(7);
        }

        size_t slash = s.find('/');
        std::string hostPort = (slash == std::string::npos) ? s : s.substr(0, slash);
        std::string path = (slash == std::string::npos) ? "/" : s.substr(slash);

        INTERNET_PORT port = secure ? 443 : 80;
        size_t colon = hostPort.find(':');
        if (colon != std::string::npos) {
            try {
                port = (INTERNET_PORT)std::stoi(hostPort.substr(colon + 1));
            } catch (...) {}
            hostPort = hostPort.substr(0, colon);
        }

        if (hostPort.empty()) return false;

        out.host = std::wstring(hostPort.begin(), hostPort.end());
        out.port = port;
        out.path = std::wstring(path.begin(), path.end());
        out.isSecure = secure;
        return true;
    } catch (...) {
        return false;
    }
}

static void closeHandles() {
    s_connected.store(false);
    s_sendCv.notify_all();
    s_reconnectCv.notify_all();

    std::lock_guard<std::recursive_mutex> lock(s_handleMutex);
    if (s_hWebSocket) {
        WinHttpWebSocketShutdown(s_hWebSocket, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
        WinHttpCloseHandle(s_hWebSocket);
        s_hWebSocket = nullptr;
    }
    if (s_hRequest) {
        WinHttpCloseHandle(s_hRequest);
        s_hRequest = nullptr;
    }
    if (s_hConnect) {
        WinHttpCloseHandle(s_hConnect);
        s_hConnect = nullptr;
    }
    if (s_hSession) {
        WinHttpCloseHandle(s_hSession);
        s_hSession = nullptr;
    }
}

static void handleIncomingPacket(const std::string& json) {
    if (json.empty()) return;
    try {
        std::string type = extractJsonString(json, "type");
        if (type.empty()) return;

        if (type == "auth_success") {
            s_connected.store(true);
            s_sendCv.notify_all();
            syncCustomRanks();
            std::string user = extractJsonString(json, "username");
            if (user.empty()) user = extractJsonString(json, "user");
            if (!user.empty()) {
                setCurrentUsername(user);
            }
            if (s_appearOffline.load()) {
                std::string packet = "{\"type\":\"status\",\"status\":\"offline\"}";
                std::lock_guard<std::mutex> lock(s_sendMutex);
                if (s_sendQueue.size() < 100) s_sendQueue.push(packet);
                s_sendCv.notify_one();
            }
            std::string motd = extractJsonString(json, "motd");
            if (!motd.empty()) {
                printToChat(motd);
            }
        } else if (type == "chat") {
            if (s_muted.load()) return;

            std::string sender = extractJsonString(json, "sender");
            std::string text = extractJsonString(json, "text");

            std::string rankTitle = "User";
            std::string rankColor = "§7";
            std::string nameColor = "§7";

            bool customFound = false;
            {
                std::lock_guard<std::mutex> lock(s_ranksMutex);
                auto it = s_customRanks.find(toLowerStr(sender));
                if (it != s_customRanks.end()) {
                    rankTitle = it->second.rankTitle;
                    rankColor = it->second.rankColor;
                    if (!it->second.nameColor.empty()) nameColor = it->second.nameColor;
                    customFound = true;
                }
            }

            if (!customFound) {
                size_t rankPos = json.find("\"rank\":");
                if (rankPos != std::string::npos) {
                    std::string rankSub = json.substr(rankPos);
                    std::string t = extractJsonString(rankSub, "title");
                    if (t.empty()) t = extractJsonString(rankSub, "name");
                    std::string c = extractJsonString(rankSub, "color");
                    std::string nc = extractJsonString(rankSub, "nameColor");
                    bool b = extractJsonBool(rankSub, "bold");
                    bool it_flag = extractJsonBool(rankSub, "italic") || extractJsonBool(rankSub, "italics");
                    bool u_flag = extractJsonBool(rankSub, "underline") || extractJsonBool(rankSub, "underlined");
                    bool s_flag = extractJsonBool(rankSub, "strike") || extractJsonBool(rankSub, "strikethrough");
                    bool k_flag = extractJsonBool(rankSub, "magic") || extractJsonBool(rankSub, "obfuscated");
                    std::string fmt = extractJsonString(rankSub, "format");

                    bool nb = extractJsonBool(rankSub, "nameBold");
                    bool nit = extractJsonBool(rankSub, "nameItalic");
                    bool nu = extractJsonBool(rankSub, "nameUnderline") || extractJsonBool(rankSub, "nameUnderlined");
                    bool ns = extractJsonBool(rankSub, "nameStrike") || extractJsonBool(rankSub, "nameStrikethrough");
                    bool nk = extractJsonBool(rankSub, "nameMagic") || extractJsonBool(rankSub, "nameObfuscated");
                    std::string nfmt = extractJsonString(rankSub, "nameFormat");

                    if (!t.empty()) rankTitle = t;
                    if (!c.empty() || b || it_flag || u_flag || s_flag || k_flag || !fmt.empty()) {
                        rankColor = buildMinecraftFormatting(c, fmt, b, it_flag, u_flag, s_flag, k_flag, "\xC2\xA7" "b");
                    }
                    if (!nc.empty() || nb || nit || nu || ns || nk || !nfmt.empty()) {
                        nameColor = buildMinecraftFormatting(nc, nfmt, nb, nit, nu, ns, nk, "\xC2\xA7" "7");
                    }
                }
            }

            std::string rankPrefix = "";
            if (!rankTitle.empty() && rankTitle != "User") {
                rankPrefix = rankColor + "[" + rankTitle + "] ";
            }

            std::string formatted = "§b[IRC] " + rankPrefix + nameColor + sender + "§7: §f" + text;
            printToChat(formatted);
            playDingSound();
        } else if (type == "dm") {
            std::string text = extractJsonString(json, "text");
            std::string dir = extractJsonString(json, "direction");

            std::string rankTitle = "User";
            std::string rankColor = "§7";
            std::string nameColor = "§7";

            std::string otherUser = (dir == "incoming") ? extractJsonString(json, "from") : extractJsonString(json, "to");
            bool customFound = false;
            {
                std::lock_guard<std::mutex> lock(s_ranksMutex);
                auto it = s_customRanks.find(toLowerStr(otherUser));
                if (it != s_customRanks.end()) {
                    rankTitle = it->second.rankTitle;
                    rankColor = it->second.rankColor;
                    if (!it->second.nameColor.empty()) nameColor = it->second.nameColor;
                    customFound = true;
                }
            }

            if (!customFound) {
                size_t rankPos = json.find("\"rank\":");
                if (rankPos != std::string::npos) {
                    std::string rankSub = json.substr(rankPos);
                    std::string t = extractJsonString(rankSub, "title");
                    if (t.empty()) t = extractJsonString(rankSub, "name");
                    std::string c = extractJsonString(rankSub, "color");
                    std::string nc = extractJsonString(rankSub, "nameColor");
                    bool b = extractJsonBool(rankSub, "bold");
                    bool it_flag = extractJsonBool(rankSub, "italic") || extractJsonBool(rankSub, "italics");
                    bool u_flag = extractJsonBool(rankSub, "underline") || extractJsonBool(rankSub, "underlined");
                    bool s_flag = extractJsonBool(rankSub, "strike") || extractJsonBool(rankSub, "strikethrough");
                    bool k_flag = extractJsonBool(rankSub, "magic") || extractJsonBool(rankSub, "obfuscated");
                    std::string fmt = extractJsonString(rankSub, "format");

                    bool nb = extractJsonBool(rankSub, "nameBold");
                    bool nit = extractJsonBool(rankSub, "nameItalic");
                    bool nu = extractJsonBool(rankSub, "nameUnderline") || extractJsonBool(rankSub, "nameUnderlined");
                    bool ns = extractJsonBool(rankSub, "nameStrike") || extractJsonBool(rankSub, "nameStrikethrough");
                    bool nk = extractJsonBool(rankSub, "nameMagic") || extractJsonBool(rankSub, "nameObfuscated");
                    std::string nfmt = extractJsonString(rankSub, "nameFormat");

                    if (!t.empty()) rankTitle = t;
                    if (!c.empty() || b || it_flag || u_flag || s_flag || k_flag || !fmt.empty()) {
                        rankColor = buildMinecraftFormatting(c, fmt, b, it_flag, u_flag, s_flag, k_flag, "\xC2\xA7" "b");
                    }
                    if (!nc.empty() || nb || nit || nu || ns || nk || !nfmt.empty()) {
                        nameColor = buildMinecraftFormatting(nc, nfmt, nb, nit, nu, ns, nk, "\xC2\xA7" "7");
                    }
                }
            }

            std::string rankPrefix = "";
            if (!rankTitle.empty() && rankTitle != "User") {
                rankPrefix = rankColor + "[" + rankTitle + "] ";
            }

            std::string formatted;
            if (dir == "incoming") {
                formatted = "§d[IRC DM] §7From: " + rankPrefix + nameColor + otherUser + "§7: §f" + text;
                playDingSound();
            } else {
                formatted = "§d[IRC DM] §7To: " + rankPrefix + nameColor + otherUser + "§7: §f" + text;
            }
            printToChat(formatted);
        } else if (type == "user_list") {
            int reportedCount = extractJsonInt(json, "count", 0);
            std::string myName = toLowerStr(getCurrentUsername());
            if (myName.empty()) myName = toLowerStr(OVson::getRealLocalUsername(true));
            if (myName.empty()) myName = toLowerStr(OVson::g_localName);

            std::vector<std::string> visibleUsers;

            size_t usersPos = json.find("\"users\":");
            if (usersPos != std::string::npos) {
                size_t openArr = json.find('[', usersPos);
                size_t endArr = (openArr != std::string::npos) ? json.find(']', openArr) : std::string::npos;
                if (openArr != std::string::npos && endArr != std::string::npos && endArr > openArr) {
                    std::string arr = json.substr(openArr, endArr - openArr + 1);
                    size_t p = 0;
                    while ((p = arr.find("\"username\":", p)) != std::string::npos) {
                        p += 11;
                        while (p < arr.size() && (arr[p] == ' ' || arr[p] == '\t')) p++;
                        if (p < arr.size() && arr[p] == '"') p++;
                        size_t endName = arr.find('"', p);
                        if (endName == std::string::npos) break;
                        std::string uName = arr.substr(p, endName - p);

                        size_t objEnd = arr.find('}', endName);
                        std::string sub = (objEnd != std::string::npos && objEnd > endName) ? arr.substr(endName, objEnd - endName) : "";

                        std::string status = extractJsonString(sub, "status");
                        if (status == "offline") {
                            if (endName + 1 > p) p = endName + 1; else break;
                            continue;
                        }

                        if (s_appearOffline.load() && !myName.empty() && toLowerStr(uName) == myName) {
                            if (endName + 1 > p) p = endName + 1; else break;
                            continue;
                        }

                        std::string rTitle = "User", rColor = "§7", nColor = "§7";
                        bool customFound = false;
                        {
                            std::lock_guard<std::mutex> lock(s_ranksMutex);
                            auto it = s_customRanks.find(toLowerStr(uName));
                            if (it != s_customRanks.end()) {
                                rTitle = it->second.rankTitle;
                                rColor = it->second.rankColor;
                                if (!it->second.nameColor.empty()) nColor = it->second.nameColor;
                                customFound = true;
                            }
                        }

                        if (!customFound && !sub.empty()) {
                            std::string t = extractJsonString(sub, "title");
                            if (t.empty()) t = extractJsonString(sub, "name");
                            std::string c = extractJsonString(sub, "color");
                            std::string nc = extractJsonString(sub, "nameColor");
                            bool b = extractJsonBool(sub, "bold");
                            bool it_flag = extractJsonBool(sub, "italic") || extractJsonBool(sub, "italics");
                            bool u_flag = extractJsonBool(sub, "underline") || extractJsonBool(sub, "underlined");
                            bool s_flag = extractJsonBool(sub, "strike") || extractJsonBool(sub, "strikethrough");
                            bool k_flag = extractJsonBool(sub, "magic") || extractJsonBool(sub, "obfuscated");
                            std::string fmt = extractJsonString(sub, "format");

                            bool nb = extractJsonBool(sub, "nameBold");
                            bool nit = extractJsonBool(sub, "nameItalic");
                            bool nu = extractJsonBool(sub, "nameUnderline") || extractJsonBool(sub, "nameUnderlined");
                            bool ns = extractJsonBool(sub, "nameStrike") || extractJsonBool(sub, "nameStrikethrough");
                            bool nk = extractJsonBool(sub, "nameMagic") || extractJsonBool(sub, "nameObfuscated");
                            std::string nfmt = extractJsonString(sub, "nameFormat");

                            if (!t.empty()) rTitle = t;
                            if (!c.empty() || b || it_flag || u_flag || s_flag || k_flag || !fmt.empty()) {
                                rColor = buildMinecraftFormatting(c, fmt, b, it_flag, u_flag, s_flag, k_flag, "\xC2\xA7" "b");
                            }
                            if (!nc.empty() || nb || nit || nu || ns || nk || !nfmt.empty()) {
                                nColor = buildMinecraftFormatting(nc, nfmt, nb, nit, nu, ns, nk, "\xC2\xA7" "7");
                            }
                        }

                        std::string item;
                        if (!rTitle.empty() && rTitle != "User") {
                            item += rColor + "[" + rTitle + "] ";
                        }
                        item += nColor + uName;
                        visibleUsers.push_back(item);

                        if (endName + 1 > p) {
                            p = endName + 1;
                        } else {
                            break;
                        }
                    }
                }
            }

            int finalCount = (int)visibleUsers.size();
            std::ostringstream oss;
            oss << "§b[IRC] §fOnline Users (" << finalCount << "): ";
            if (visibleUsers.empty()) {
                oss << "§7None";
            } else {
                for (size_t i = 0; i < visibleUsers.size(); ++i) {
                    if (i > 0) oss << "§7, ";
                    oss << visibleUsers[i];
                }
            }
            printToChat(oss.str());
        } else if (type == "join") {
            if (s_muted.load()) return;
            std::string user = extractJsonString(json, "username");
            std::string myName = toLowerStr(getCurrentUsername());
            if (myName.empty()) myName = toLowerStr(OVson::getRealLocalUsername(true));
            if (myName.empty()) myName = toLowerStr(OVson::g_localName);
            if (s_appearOffline.load() && !myName.empty() && toLowerStr(user) == myName) return;

            int count = extractJsonInt(json, "onlineCount", 0);
            std::string rankTitle = "User", rankColor = "§7", nameColor = "§7";
            bool customFound = false;
            {
                std::lock_guard<std::mutex> lock(s_ranksMutex);
                auto it = s_customRanks.find(toLowerStr(user));
                if (it != s_customRanks.end()) {
                    rankTitle = it->second.rankTitle;
                    rankColor = it->second.rankColor;
                    if (!it->second.nameColor.empty()) nameColor = it->second.nameColor;
                    customFound = true;
                }
            }
            if (!customFound) {
                size_t rankPos = json.find("\"rank\":");
                if (rankPos != std::string::npos) {
                    std::string rankSub = json.substr(rankPos);
                    std::string t = extractJsonString(rankSub, "title");
                    if (t.empty()) t = extractJsonString(rankSub, "name");
                    std::string c = extractJsonString(rankSub, "color");
                    std::string nc = extractJsonString(rankSub, "nameColor");
                    bool b = extractJsonBool(rankSub, "bold");
                    bool it_flag = extractJsonBool(rankSub, "italic") || extractJsonBool(rankSub, "italics");
                    bool u_flag = extractJsonBool(rankSub, "underline") || extractJsonBool(rankSub, "underlined");
                    bool s_flag = extractJsonBool(rankSub, "strike") || extractJsonBool(rankSub, "strikethrough");
                    bool k_flag = extractJsonBool(rankSub, "magic") || extractJsonBool(rankSub, "obfuscated");
                    std::string fmt = extractJsonString(rankSub, "format");

                    bool nb = extractJsonBool(rankSub, "nameBold");
                    bool nit = extractJsonBool(rankSub, "nameItalic");
                    bool nu = extractJsonBool(rankSub, "nameUnderline") || extractJsonBool(rankSub, "nameUnderlined");
                    bool ns = extractJsonBool(rankSub, "nameStrike") || extractJsonBool(rankSub, "nameStrikethrough");
                    bool nk = extractJsonBool(rankSub, "nameMagic") || extractJsonBool(rankSub, "nameObfuscated");
                    std::string nfmt = extractJsonString(rankSub, "nameFormat");

                    if (!t.empty()) rankTitle = t;
                    if (!c.empty() || b || it_flag || u_flag || s_flag || k_flag || !fmt.empty()) {
                        rankColor = buildMinecraftFormatting(c, fmt, b, it_flag, u_flag, s_flag, k_flag, "\xC2\xA7" "b");
                    }
                    if (!nc.empty() || nb || nit || nu || ns || nk || !nfmt.empty()) {
                        nameColor = buildMinecraftFormatting(nc, nfmt, nb, nit, nu, ns, nk, "\xC2\xA7" "7");
                    }
                }
            }
            std::string rankPrefix = "";
            if (!rankTitle.empty() && rankTitle != "User") {
                rankPrefix = rankColor + "[" + rankTitle + "] ";
            }
            std::string msg = "§8[IRC] §a+ " + rankPrefix + nameColor + user + " §7joined. (§b" + std::to_string(count) + " online§7)";
            printToChat(msg);
        } else if (type == "leave") {
            if (s_muted.load()) return;
            std::string user = extractJsonString(json, "username");
            std::string myName = toLowerStr(getCurrentUsername());
            if (myName.empty()) myName = toLowerStr(OVson::getRealLocalUsername(true));
            if (myName.empty()) myName = toLowerStr(OVson::g_localName);
            if (s_appearOffline.load() && !myName.empty() && toLowerStr(user) == myName) return;

            std::string rankTitle = "User", rankColor = "§7", nameColor = "§7";
            bool customFound = false;
            {
                std::lock_guard<std::mutex> lock(s_ranksMutex);
                auto it = s_customRanks.find(toLowerStr(user));
                if (it != s_customRanks.end()) {
                    rankTitle = it->second.rankTitle;
                    rankColor = it->second.rankColor;
                    if (!it->second.nameColor.empty()) nameColor = it->second.nameColor;
                    customFound = true;
                }
            }
            std::string rankPrefix = "";
            if (!rankTitle.empty() && rankTitle != "User") {
                rankPrefix = rankColor + "[" + rankTitle + "] ";
            }
            std::string msg = "§8[IRC] §c- " + rankPrefix + nameColor + user + " §7left.";
            printToChat(msg);
        } else if (type == "error") {
            std::string err = extractJsonString(json, "message");
            if (!err.empty()) {
                printToChat(err);
            }
        }
    } catch (const std::exception& e) {
        Logger::error("[IrcService] Exception in handleIncomingPacket: %s", e.what());
    } catch (...) {
        Logger::error("[IrcService] Unknown exception in handleIncomingPacket.");
    }
}

static bool performHandshake(HINTERNET hWs) {
    try {
        if (!hWs) return false;
        std::string username = OVson::getRealLocalUsername(true);
        if (username.empty()) username = OVson::g_localName;
        if (username.empty()) username = "Player";
        setCurrentUsername(username);
        std::string uuid = "";

        auto now = std::chrono::system_clock::now();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

        std::string nonce = generateNonce();
        std::string secret = Config::getIrcSecretKey();
        if (secret.empty()) secret = "ovson_secure_irc_2026_salt";

        std::string dataToSign = username + ":" + uuid + ":" + std::to_string(timestamp) + ":" + nonce;
        std::string hmacSig = computeHmacSha256(secret, dataToSign);

        std::ostringstream oss;
        oss << "{\"type\":\"auth\","
            << "\"username\":\"" << escapeJson(username) << "\","
            << "\"uuid\":\"" << escapeJson(uuid) << "\","
            << "\"timestamp\":" << timestamp << ","
            << "\"nonce\":\"" << nonce << "\","
            << "\"hmac\":\"" << hmacSig << "\","
            << "\"status\":\"" << (s_appearOffline.load() ? "offline" : "online") << "\"}";

        std::string packet = oss.str();
        DWORD err = WinHttpWebSocketSend(hWs, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                         (PVOID)packet.data(), (DWORD)packet.length());
        return (err == ERROR_SUCCESS);
    } catch (...) {
        return false;
    }
}

static void senderLoop(uint64_t myGen) {
    try {
        Logger::info("[IrcService] Sender thread %llu started.", myGen);
        while (s_running.load() && s_sessionGen.load() == myGen) {
            std::string packet;
            {
                std::unique_lock<std::mutex> lock(s_sendMutex);
                s_sendCv.wait(lock, [myGen] {
                    return !s_running.load() || s_sessionGen.load() != myGen || 
                           (!s_sendQueue.empty() && s_connected.load());
                });

                if (!s_running.load() || s_sessionGen.load() != myGen) break;
                if (s_sendQueue.empty() || !s_connected.load()) continue;

                packet = std::move(s_sendQueue.front());
                s_sendQueue.pop();
            }

            if (!packet.empty() && s_connected.load()) {
                std::lock_guard<std::recursive_mutex> hLock(s_handleMutex);
                if (s_hWebSocket && s_connected.load() && s_sessionGen.load() == myGen) {
                    DWORD err = WinHttpWebSocketSend(s_hWebSocket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                                     (PVOID)packet.data(), (DWORD)packet.length());
                    if (err != ERROR_SUCCESS) {
                        Logger::error("[IrcService] WinHttpWebSocketSend failed: %d", err);
                    }
                }
            }
        }
        Logger::info("[IrcService] Sender thread %llu stopped.", myGen);
    } catch (const std::exception& e) {
        Logger::error("[IrcService] Exception in sender thread %llu: %s", myGen, e.what());
    } catch (...) {
        Logger::error("[IrcService] Unknown exception in sender thread %llu.", myGen);
    }
}

static void workerLoop(uint64_t myGen) {
    try {
        Logger::info("[IrcService] Worker thread %llu started.", myGen);

        while (s_running.load() && s_sessionGen.load() == myGen) {
            if (!Config::isIrcEnabled()) {
                interruptibleSleep(1, myGen);
                continue;
            }

            std::string url = Config::getIrcServerUrl();
            ParsedUrl pu;
            if (!parseWsUrl(url, pu)) {
                interruptibleSleep(3, myGen);
                continue;
            }

            if (s_sessionGen.load() != myGen || !s_running.load()) break;

            {
                std::lock_guard<std::recursive_mutex> lock(s_handleMutex);
                s_hSession = WinHttpOpen(L"OVson-IRC-Client/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
            }
            if (!s_hSession) {
                interruptibleSleep(2, myGen);
                continue;
            }

            if (s_sessionGen.load() != myGen || !s_running.load()) {
                closeHandles();
                break;
            }

            {
                std::lock_guard<std::recursive_mutex> lock(s_handleMutex);
                if (s_hSession) {
                    s_hConnect = WinHttpConnect(s_hSession, pu.host.c_str(), pu.port, 0);
                }
            }
            if (!s_hConnect) {
                closeHandles();
                interruptibleSleep(2, myGen);
                continue;
            }

            if (s_sessionGen.load() != myGen || !s_running.load()) {
                closeHandles();
                break;
            }

            DWORD reqFlags = pu.isSecure ? WINHTTP_FLAG_SECURE : 0;
            {
                std::lock_guard<std::recursive_mutex> lock(s_handleMutex);
                if (s_hConnect) {
                    s_hRequest = WinHttpOpenRequest(s_hConnect, L"GET", pu.path.c_str(),
                                                    nullptr, WINHTTP_NO_REFERER,
                                                    WINHTTP_DEFAULT_ACCEPT_TYPES, reqFlags);
                }
            }
            if (!s_hRequest) {
                closeHandles();
                interruptibleSleep(2, myGen);
                continue;
            }

            if (!WinHttpSetOption(s_hRequest, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) {
                closeHandles();
                interruptibleSleep(2, myGen);
                continue;
            }

            if (!WinHttpSendRequest(s_hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
                closeHandles();
                interruptibleSleep(5, myGen);
                continue;
            }

            if (!WinHttpReceiveResponse(s_hRequest, nullptr)) {
                closeHandles();
                interruptibleSleep(5, myGen);
                continue;
            }

            DWORD statusCode = 0;
            DWORD dwSize = sizeof(statusCode);
            WinHttpQueryHeaders(s_hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

            if (statusCode != 101) {
                Logger::error("[IrcService] WebSocket upgrade rejected by server (HTTP %d)", statusCode);
                closeHandles();
                interruptibleSleep(5, myGen);
                continue;
            }

            if (s_sessionGen.load() != myGen || !s_running.load()) {
                closeHandles();
                break;
            }

            {
                std::lock_guard<std::recursive_mutex> lock(s_handleMutex);
                if (s_hRequest) {
                    s_hWebSocket = WinHttpWebSocketCompleteUpgrade(s_hRequest, 0);
                    WinHttpCloseHandle(s_hRequest);
                    s_hRequest = nullptr;
                }
            }
            if (!s_hWebSocket) {
                closeHandles();
                interruptibleSleep(3, myGen);
                continue;
            }

            Logger::info("[IrcService] WebSocket connected successfully! Performing auth handshake...");

            bool handshakeOk = false;
            {
                std::lock_guard<std::recursive_mutex> lock(s_handleMutex);
                if (s_hWebSocket) {
                    handshakeOk = performHandshake(s_hWebSocket);
                }
            }

            if (!handshakeOk) {
                Logger::error("[IrcService] Handshake send failed.");
                closeHandles();
                interruptibleSleep(3, myGen);
                continue;
            }

            std::vector<char> recvBuffer(8192);
            std::string accumulatedMessage;
            const size_t MAX_ACCUMULATED_SIZE = 65536;
            auto lastRankSyncTime = std::chrono::steady_clock::now();

            while (s_running.load() && Config::isIrcEnabled() && s_sessionGen.load() == myGen) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - lastRankSyncTime).count() >= 300) {
                    lastRankSyncTime = now;
                    syncCustomRanks();
                }

                DWORD bytesRead = 0;
                WINHTTP_WEB_SOCKET_BUFFER_TYPE bufType;
                
                HINTERNET wsHandle = nullptr;
                {
                    std::lock_guard<std::recursive_mutex> lock(s_handleMutex);
                    wsHandle = s_hWebSocket;
                }
                if (!wsHandle) break;

                DWORD rxErr = WinHttpWebSocketReceive(wsHandle, recvBuffer.data(), (DWORD)recvBuffer.size(),
                                                      &bytesRead, &bufType);

                if (rxErr != ERROR_SUCCESS) {
                    Logger::info("[IrcService] Connection terminated or socket read error (%d)", rxErr);
                    break;
                }

                if (bufType == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) {
                    Logger::info("[IrcService] Server closed WebSocket connection.");
                    break;
                }

                if (bytesRead > 0 && bytesRead <= recvBuffer.size()) {
                    if (accumulatedMessage.size() + bytesRead > MAX_ACCUMULATED_SIZE) {
                        Logger::error("[IrcService] Incoming message exceeded max buffer size (64KB). Dropping.");
                        accumulatedMessage.clear();
                    } else {
                        accumulatedMessage.append(recvBuffer.data(), bytesRead);
                    }
                }

                if (bufType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
                    try {
                        handleIncomingPacket(accumulatedMessage);
                    } catch (const std::exception& e) {
                        Logger::error("[IrcService] Exception handling packet: %s", e.what());
                    } catch (...) {
                        Logger::error("[IrcService] Unknown exception handling packet.");
                    }
                    accumulatedMessage.clear();
                }
            }

            closeHandles();
            interruptibleSleep(3, myGen);
        }

        closeHandles();
        Logger::info("[IrcService] Worker thread %llu stopped.", myGen);
    } catch (const std::exception& e) {
        closeHandles();
        Logger::error("[IrcService] Exception in worker thread %llu: %s", myGen, e.what());
    } catch (...) {
        closeHandles();
        Logger::error("[IrcService] Unknown exception in worker thread %llu.", myGen);
    }
}

void initialize() {
    uint64_t myGen = ++s_sessionGen;
    s_running.store(true);
    s_connected.store(false);
    s_muted.store(Config::isIrcMuted());
    s_appearOffline.store(Config::isIrcAppearOffline());

    syncCustomRanks();

    if (s_workerThread.joinable()) s_workerThread.detach();
    if (s_senderThread.joinable()) s_senderThread.detach();

    s_workerThread = std::thread([myGen]() { workerLoop(myGen); });
    s_senderThread = std::thread([myGen]() { senderLoop(myGen); });
}

void shutdown(bool wait) {
    s_sessionGen.fetch_add(1);
    s_running.store(false);
    s_sendCv.notify_all();
    s_reconnectCv.notify_all();
    closeHandles();

    if (wait) {
        try {
            if (s_workerThread.joinable()) {
                s_workerThread.join();
            }
            if (s_senderThread.joinable()) {
                s_senderThread.join();
            }
        } catch (...) {}
    } else {
        std::thread cleanup([w = std::move(s_workerThread), s = std::move(s_senderThread)]() mutable {
            try {
                if (w.joinable()) w.join();
                if (s.joinable()) s.join();
            } catch (...) {}
        });
        cleanup.detach();
    }

    std::lock_guard<std::mutex> lock(s_sendMutex);
    while (!s_sendQueue.empty()) s_sendQueue.pop();
    s_dingPending.store(false);
}

bool isConnected() {
    return s_connected.load();
}

bool isMuted() {
    return s_muted.load();
}

void setMuted(bool muted) {
    s_muted.store(muted);
    Config::setIrcMuted(muted);
}

void toggleMuted() {
    bool next = !s_muted.load();
    setMuted(next);
    if (next) {
        printToChat("§8[IRC] §7IRC notifications §cmuted§7.");
    } else {
        printToChat("§8[IRC] §7IRC notifications §aunmuted§7.");
    }
}

bool isAppearOffline() {
    return s_appearOffline.load();
}

void setAppearOffline(bool offline) {
    bool changed = (s_appearOffline.exchange(offline) != offline);
    Config::setIrcAppearOffline(offline);
    if (changed && s_connected.load()) {
        std::string packet = std::string("{\"type\":\"status\",\"status\":\"") + (offline ? "offline" : "online") + "\"}";
        {
            std::lock_guard<std::mutex> lock(s_sendMutex);
            if (s_sendQueue.size() >= 100) {
                s_sendQueue.pop();
            }
            s_sendQueue.push(packet);
        }
        s_sendCv.notify_one();
    }
}

void toggleAppearOffline() {
    bool next = !s_appearOffline.load();
    setAppearOffline(next);
    if (next) {
        printToChat("§8[IRC] §7Status: §cAppear Offline §7(Hidden from user list)");
    } else {
        printToChat("§8[IRC] §7Status: §aOnline §7(Visible to everyone)");
    }
}

void reconnect() {
    if (!s_running.load() || !Config::isIrcEnabled()) return;
    Logger::info("[IrcService] Reconnect requested.");
    closeHandles();
}

void checkPlayerNameRefresh(bool forceWorldChange) {
    try {
        if (!Config::isIrcEnabled()) return;

        auto now = std::chrono::steady_clock::now();
        if (!forceWorldChange) {
            auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - s_lastPeriodicCheck).count();
            if (s_lastPeriodicCheck.time_since_epoch().count() != 0 && elapsedSec < 60) {
                return;
            }
        }
        s_lastPeriodicCheck = now;

        std::string detectedName = OVson::getRealLocalUsername(true);
        if (detectedName.empty()) return;

        std::string currentName = getCurrentUsername();
        if (currentName.empty()) {
            setCurrentUsername(detectedName);
            if (s_connected.load()) {
                reconnect();
            }
            return;
        }

        if (detectedName != currentName) {
            Logger::info("[IrcService] Player name changed detected: '%s' -> '%s'. Reconnecting IRC...",
                         currentName.c_str(), detectedName.c_str());
            printToChat("§8[IRC] §7Account switched to §e" + detectedName + "§7. Reconnecting IRC...");
            setCurrentUsername(detectedName);
            reconnect();
        }
    } catch (...) {
        Logger::error("[IrcService] Exception in checkPlayerNameRefresh.");
    }
}

static std::chrono::steady_clock::time_point s_lastMessageTime{};
static std::mutex s_rateLimitMutex;

static bool checkClientRateLimit() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(s_rateLimitMutex);
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_lastMessageTime).count();
    if (s_lastMessageTime.time_since_epoch().count() != 0 && elapsedMs < 1000) {
        printToChat("§c[IRC] Please wait! Rate limit is 1 message per second.");
        return false;
    }
    s_lastMessageTime = now;
    return true;
}

void sendMessage(const std::string& text) {
    try {
        if (!Config::isIrcEnabled()) {
            printToChat("§c[IRC] IRC system is disabled in settings.");
            return;
        }
        if (!s_connected.load()) {
            printToChat("§c[IRC] Not connected to IRC server! Please wait.");
            return;
        }

        std::string clean = text;
        while (!clean.empty() && (clean.front() == ' ' || clean.front() == '\t')) clean.erase(clean.begin());
        while (!clean.empty() && (clean.back() == ' ' || clean.back() == '\t' || clean.back() == '\r' || clean.back() == '\n')) clean.pop_back();

        if (clean.empty()) {
            printToChat("§c[IRC] Usage: @<message>");
            return;
        }

        if (!checkClientRateLimit()) return;

        if (clean.size() > 1000) clean.resize(1000);

        std::string packet = "{\"type\":\"chat\",\"text\":\"" + escapeJson(clean) + "\"}";
        {
            std::lock_guard<std::mutex> lock(s_sendMutex);
            if (s_sendQueue.size() >= 100) {
                s_sendQueue.pop(); // discard oldest if flooded
            }
            s_sendQueue.push(packet);
        }
        s_sendCv.notify_one();
    } catch (...) {}
}

void sendDirectMessage(const std::string& target, const std::string& text) {
    try {
        if (!Config::isIrcEnabled()) {
            printToChat("§c[IRC] IRC system is disabled in settings.");
            return;
        }
        if (!s_connected.load()) {
            printToChat("§c[IRC] Not connected to IRC server! Please wait.");
            return;
        }

        std::string cleanTarget = target;
        std::string cleanText = text;
        while (!cleanText.empty() && (cleanText.front() == ' ' || cleanText.front() == '\t')) cleanText.erase(cleanText.begin());
        if (cleanTarget.empty() || cleanText.empty()) {
            printToChat("§c[IRC] Usage: .msg <user> <message>");
            return;
        }

        if (!checkClientRateLimit()) return;

        if (cleanTarget.size() > 64) cleanTarget.resize(64);
        if (cleanText.size() > 1000) cleanText.resize(1000);

        std::string packet = "{\"type\":\"msg\",\"target\":\"" + escapeJson(cleanTarget) + "\",\"text\":\"" + escapeJson(cleanText) + "\"}";
        {
            std::lock_guard<std::mutex> lock(s_sendMutex);
            if (s_sendQueue.size() >= 100) {
                s_sendQueue.pop();
            }
            s_sendQueue.push(packet);
        }
        s_sendCv.notify_one();
    } catch (...) {}
}

void requestUserList() {
    try {
        if (!Config::isIrcEnabled()) {
            printToChat("§c[IRC] IRC system is disabled in settings.");
            return;
        }
        if (!s_connected.load()) {
            printToChat("§c[IRC] Not connected to IRC server! Please wait.");
            return;
        }

        std::string packet = "{\"type\":\"list\"}";
        {
            std::lock_guard<std::mutex> lock(s_sendMutex);
            if (s_sendQueue.size() >= 100) {
                s_sendQueue.pop();
            }
            s_sendQueue.push(packet);
        }
        s_sendCv.notify_one();
    } catch (...) {}
}

void fetchCustomRanksFromUrl(const std::string& url) {
    if (url.empty()) return;
    std::thread([url]() {
        try {
            ParsedUrl pu;
            if (!parseWsUrl(url, pu)) return;
            HINTERNET hS = WinHttpOpen(L"OVson-Rank-Sync/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!hS) return;
            HINTERNET hC = WinHttpConnect(hS, pu.host.c_str(), pu.port, 0);
            if (!hC) { WinHttpCloseHandle(hS); return; }
            DWORD flags = pu.isSecure ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hR = WinHttpOpenRequest(hC, L"GET", pu.path.c_str(), nullptr,
                                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
            if (!hR) { WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return; }

            DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(hR, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

            DWORD secFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                             SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                             SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                             SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
            WinHttpSetOption(hR, WINHTTP_OPTION_SECURITY_FLAGS, &secFlags, sizeof(secFlags));

            if (WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(hR, nullptr)) {
                std::string respData;
                char buf[2048];
                DWORD read = 0;
                while (WinHttpReadData(hR, buf, sizeof(buf), &read) && read > 0) {
                    respData.append(buf, read);
                }
                if (!respData.empty()) {
                    loadCustomRanksFromJson(respData);
                    Logger::info("[IrcService] Custom ranks successfully synchronized from remote URL.");
                }
            }
            WinHttpCloseHandle(hR);
            WinHttpCloseHandle(hC);
            WinHttpCloseHandle(hS);
        } catch (...) {}
    }).detach();
}

void syncCustomRanks() {
    std::string url = Config::getIrcRanksUrl();
    if (!url.empty()) {
        fetchCustomRanksFromUrl(url);
    }
}

} // namespace IrcService

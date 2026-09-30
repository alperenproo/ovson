#include "ChatHook.h"
#include "Commands.h"
#include "../Utils/Logger.h"
#include "../Java.h"
#include "../Plugins/PluginLoader.h"
#include "../Config/Config.h"
#include "ChatSDK.h"
#include "../Logic/StatsTracker.h"
#include "ChatAPI_Bridge.h"
#include "../Logic/StatsTracker.internal.h"
#include "../Logic/Bedwars/BedwarsRuntime.h"
#include "../Config/StatColors.h"
#include "../Utils/BedwarsPrestiges.h"
#include "../Render/RenderHook.h"
#include "../Plugins/EventDispatcher.h"
#include "../Logic/BowDistance.h"
#include "../Services/IrcService.h"
#include <thread>

static jmethodID g_sendChatMessage = nullptr;
static jmethodID g_printChatMessage = nullptr;
static jmethodID g_getUnformattedText = nullptr;
static bool g_ignoreNextChat = false;
static std::string s_lastSentChatMessage;
static ULONGLONG s_lastSentChatTime = 0;

static std::string stripFormattingCodes(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == 0xC2 && i + 2 < text.size() && (unsigned char)text[i+1] == 0xA7) {
            i += 2; // skip § and the color code char
            continue;
        }
        if (c == 0xA7 && i + 1 < text.size()) {
            i += 1;
            continue;
        }
        if (c == 0x15 && i + 1 < text.size()) {
            i += 1;
            continue;
        }
        result += text[i];
    }
    return result;
}

static bool isChatStatsActiveGame() {
    if (!OVson::isInHypixelGame() || OVson::isInPreGameLobby() || OVson::isInReplay()) {
        return false;
    }
    if (OVson::getGameMode() == 0) {
        const auto snap = OVson::Bedwars::Runtime::instance().snapshot();
        if (!snap.lifecycleStatus.empty() && !snap.active) {
            return false;
        }
    }
    return true;
}

static bool shouldIgnoreChatMessage(const std::string& unformatted) {
    std::string clean = stripFormattingCodes(unformatted);
    size_t firstNonSpace = clean.find_first_not_of(" \t\r\n");
    if (firstNonSpace != std::string::npos) {
        clean = clean.substr(firstNonSpace);
    } else {
        return true;
    }

    if (clean.rfind("Party >", 0) == 0 || clean.rfind("Party>", 0) == 0 ||
        clean.rfind("[Party]", 0) == 0 || clean.rfind("Party:", 0) == 0 ||
        clean.rfind("Party |", 0) == 0) {
        return true;
    }

    if (clean.rfind("To:", 0) == 0 || clean.rfind("To ", 0) == 0 ||
        clean.rfind("To [", 0) == 0 || clean.rfind("[To]", 0) == 0) {
        return true;
    }
    if (clean.rfind("From:", 0) == 0 || clean.rfind("From ", 0) == 0 ||
        clean.rfind("From [", 0) == 0 || clean.rfind("[From]", 0) == 0) {
        return true;
    }

    if (clean.rfind("Guild >", 0) == 0 || clean.rfind("Guild>", 0) == 0 ||
        clean.rfind("Officer >", 0) == 0 || clean.rfind("Officer>", 0) == 0 ||
        clean.rfind("Co-op >", 0) == 0) {
        return true;
    }

    size_t colonPos = clean.find(": ");
    if (colonPos == std::string::npos) {
        return true;
    }

    std::string beforeColon = clean.substr(0, colonPos);
    if (beforeColon.find("Party >") != std::string::npos ||
        beforeColon.find("Guild >") != std::string::npos ||
        beforeColon.find("Officer >") != std::string::npos ||
        beforeColon.find("From ") != std::string::npos ||
        beforeColon.find("To ") != std::string::npos) {
        return true;
    }

    return false;
}

static std::string extractNameFromChat(const std::string& unformatted) {
    if (shouldIgnoreChatMessage(unformatted)) {
        return "";
    }

    std::string clean = stripFormattingCodes(unformatted);
    size_t firstNonSpace = clean.find_first_not_of(" \t\r\n");
    if (firstNonSpace != std::string::npos) {
        clean = clean.substr(firstNonSpace);
    }

    size_t colonPos = clean.find(": ");
    if (colonPos == std::string::npos) return "";
    
    size_t endName = colonPos;
    while (endName > 0 && clean[endName - 1] == ' ') endName--;
    
    size_t startName = endName;
    while (startName > 0) {
        char c = clean[startName - 1];
        if (!isalnum(c) && c != '_') break;
        startName--;
    }
    
    if (endName > startName && (endName - startName) <= 16) {
        std::string name = clean.substr(startName, endName - startName);
        if (name == "From" || name == "To" || name == "Party" || name == "Guild") return "";
        return name;
    }
    return "";
}


static std::string escapeJsonChat(const std::string &str) {
    std::string result;
    for (char c : str) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '/') result += "\\/";
        else if (c == '\b') result += "\\b";
        else if (c == '\f') result += "\\f";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c == '\t') result += "\\t";
        else result += c;
    }
    return result;
}

static std::string getAbbrChat(const std::string &raw) {
    std::string t = raw;
    for (auto &c : t) c = (char)toupper((unsigned char)c);
    if (t.find("REPLAY") != std::string::npos || t.find("REPLAYS_NEEDED") != std::string::npos) return "\xC2\xA7""6[RN]";
    if (t.find("BLATANT") != std::string::npos) return "\xC2\xA7""4[BC]";
    if (t.find("CLOSET") != std::string::npos) return "\xC2\xA7""4[CC]";
    if (t.find("CONFIRMED") != std::string::npos) return "\xC2\xA7""5[C]";
    if (t.find("CHEATER") != std::string::npos) return "\xC2\xA7""5[C]";
    if (t.find("CAUTION") != std::string::npos) return "\xC2\xA7""e[!]";
    if (t.find("SUSPICIOUS") != std::string::npos) return "\xC2\xA7""6[?]";
    if (t.find("SNIPER") != std::string::npos) return "\xC2\xA7""6[S]";
    if (t.find("INFO") != std::string::npos) return "\xC2\xA7""a[I]";
    return "";
}

static std::string mcCodeToJsonColor(const std::string& code) {
    if (code.length() >= 2 && (code[0] == '\xC2' || code[0] == '\xA7' || code[0] == '&')) {
        char c = code[code.length()-1];
        switch (c) {
            case '0': return "black";
            case '1': return "dark_blue";
            case '2': return "dark_green";
            case '3': return "dark_aqua";
            case '4': return "dark_red";
            case '5': return "dark_purple";
            case '6': return "gold";
            case '7': return "gray";
            case '8': return "dark_gray";
            case '9': return "blue";
            case 'a': return "green";
            case 'b': return "aqua";
            case 'c': return "red";
            case 'd': return "light_purple";
            case 'e': return "yellow";
            case 'f': return "white";
        }
    }
    return "white";
}

static bool hasStarInChat(const std::string& unformatted, const std::string& rawJson) {
    std::string clean = stripFormattingCodes(unformatted);
    size_t colonPos = clean.find(": ");
    if (colonPos != std::string::npos) {
        std::string prefix = clean.substr(0, colonPos);
        size_t openBracket = 0;
        while ((openBracket = prefix.find('[', openBracket)) != std::string::npos) {
            size_t closeBracket = prefix.find(']', openBracket);
            if (closeBracket == std::string::npos) break;

            std::string tag = prefix.substr(openBracket + 1, closeBracket - openBracket - 1);
            size_t s = tag.find_first_not_of(" \t");
            if (s != std::string::npos && isdigit((unsigned char)tag[s])) {
                return true;
            }
            openBracket = closeBracket + 1;
        }
    }

    static const char* s_starSymbols[] = {
        "\xE2\x9C\xAA", // ✫ (U+272A)
        "\xE2\x9C\xAB", // ✬ (U+272B)
        "\xE2\x9C\xAD", // ✭ (U+272D)
        "\xE2\x9C\xAE", // ✮ (U+272E)
        "\xE2\x9C\xAF", // ✯ (U+272F)
        "\xE2\x9C\xB0", // ✰ (U+2730)
        "\xE2\x9C\xA6", // ✦ (U+2726)
        "\xE2\x9C\xA7", // ✧ (U+2727)
        "\xE2\x9A\x9D", // ⚝ (U+269D)
        "\xE2\x9C\xA5", // ✥ (U+2725)
        "\xE2\x9C\xA4", // ✤ (U+2724)
        "\xE2\x9D\x87", // ❈ (U+2747)
        "\xE2\x9D\xA4", // ❤ (U+2764)
        "\xE2\x98\xA0", // ☠ (U+2620)
        "\xE2\x9A\xA1", // ⚡ (U+26A1)
        "\xE2\x9A\x94", // ⚔ (U+2694)
        "\xE2\x9D\x84", // ❄ (U+2744)
        "\xE2\x9C\xBF", // ✿ (U+273F)
        "\xE2\x9C\x80", // ❀ (U+2740)
        "\xE2\x9C\x81", // ❁ (U+2741)
        "\xE2\x9D\x86", // ❅ (U+2746)
        "\xE2\x9D\x88", // ❉ (U+2748)
        "\xE2\x9C\xB4", // ✴ (U+2734)
        "\xE2\x9C\xB3", // ✳ (U+2733)
        "\xE2\x9C\xB6", // ✶ (U+2736)
        "\xE2\x9C\xB7", // ✷ (U+2737)
        "\xE2\x9C\xB8", // ✸ (U+2738)
        "\xE2\x9C\x94", // ✔ (U+2714)
        "\xE2\x9C\x88", // ✈ (U+2708)
        "\xE2\x9A\x99", // ⚙ (U+2699)
        "\xE2\x98\x82", // ☂ (U+2602)
        "\xE2\x99\xAA", // ♪ (U+266A)
        "\xE2\x99\xAB", // ♫ (U+266B)
        "\xE2\x99\x9B", // ♛ (U+265B)
        "\xE2\x99\x9A", // ♚ (U+265A)
        "\xE2\x97\x86", // ◆ (U+25C6)
        "\xE2\x97\x87", // ◇ (U+25C7)
        "\xE2\x97\x8F", // ● (U+25CF)
        "\xE2\x97\x8B", // ○ (U+25CB)
        "\xE2\x96\xB2", // ▲ (U+25B2)
        "\xE2\x96\xBC", // ▼ (U+25BC)
        "\xE2\x97\x80", // ◀ (U+25C0)
        "\xE2\x96\xB6", // ▶ (U+25B6)
        "\xE2\x98\x85", // ★ (U+2605)
        "\xE2\x98\x86"  // ☆ (U+2606)
    };

    size_t colonInJson = rawJson.find("\"text\":\":");
    std::string searchRegion = (colonInJson != std::string::npos) ? rawJson.substr(0, colonInJson) : rawJson;
    for (const char* sym : s_starSymbols) {
        if (searchRegion.find(sym) != std::string::npos) {
            return true;
        }
    }

    size_t bracketPos = searchRegion.find('[');
    while (bracketPos != std::string::npos) {
        size_t nextPos = bracketPos + 1;
        while (nextPos < searchRegion.size() && (searchRegion[nextPos] == ' ' || searchRegion[nextPos] == '\\' || searchRegion[nextPos] == '\"')) {
            nextPos++;
        }
        if (nextPos < searchRegion.size() && isdigit((unsigned char)searchRegion[nextPos])) {
            return true;
        }
        bracketPos = searchRegion.find('[', bracketPos + 1);
    }

    return false;
}

static std::string injectStatsIntoJson(std::string rawJson, const std::string& playerName, const Hypixel::PlayerStats& stats, const std::string& formatOpt, const std::string& unformatted) {
    std::string injectNodes = "";
    
    if (stats.isNicked) {
        injectNodes += "{\"text\":\" \xC2\xA7""4[NICKED]\"},";
    } else if (Hypixel::isFreshAccount(stats)) {
        injectNodes += "{\"text\":\" \xC2\xA7""5[FRESH]\"},";
    } else {
        std::string statStr = "";
        std::string statColorCode = "gray"; 
        std::string styleOpt = Config::getChatStatsStyle();
        bool isCompact = (styleOpt == "compact" || styleOpt == "Compact" || styleOpt == "Colon");

        if (formatOpt == "fkdr") {
            double fkdr = (stats.bedwarsFinalDeaths == 0) ? stats.bedwarsFinalKills : (double)stats.bedwarsFinalKills / stats.bedwarsFinalDeaths;
            char buf[32];
            snprintf(buf, sizeof(buf), isCompact ? "%.2f" : "%.2f FKDR", fkdr);
            statStr = buf;
            statColorCode = StatColors::getMcColor(StatColors::StatType::FKDR, fkdr);
        } else if (formatOpt == "wlr") {
            double wlr = (stats.bedwarsLosses == 0) ? stats.bedwarsWins : (double)stats.bedwarsWins / stats.bedwarsLosses;
            char buf[32];
            snprintf(buf, sizeof(buf), isCompact ? "%.2f" : "%.2f WLR", wlr);
            statStr = buf;
            statColorCode = StatColors::getMcColor(StatColors::StatType::WLR, wlr);
        } else if (formatOpt == "fk") {
            char buf[32];
            snprintf(buf, sizeof(buf), isCompact ? "%d" : "%d Finals", stats.bedwarsFinalKills);
            statStr = buf;
            statColorCode = StatColors::getMcColor(StatColors::StatType::FinalKills, stats.bedwarsFinalKills);
        } else if (formatOpt == "wins") {
            char buf[32];
            snprintf(buf, sizeof(buf), isCompact ? "%d" : "%d Wins", stats.bedwarsWins);
            statStr = buf;
            statColorCode = StatColors::getMcColor(StatColors::StatType::Wins, stats.bedwarsWins);
        } else if (formatOpt == "blr") {
            double blr = (stats.bedwarsBedsLost == 0) ? stats.bedwarsBedsBroken : (double)stats.bedwarsBedsBroken / stats.bedwarsBedsLost;
            char buf[32];
            snprintf(buf, sizeof(buf), isCompact ? "%.2f" : "%.2f BBLR", blr);
            statStr = buf;
            statColorCode = StatColors::getMcColor(StatColors::StatType::BLR, blr);
        }
        
        std::string jsonColor = mcCodeToJsonColor(statColorCode);
        std::string statsInject = "{\"text\":\" \"},{\"text\":\"[\",\"color\":\"gray\"},{\"text\":\"" + statStr + "\",\"color\":\"" + jsonColor + "\"},{\"text\":\"]\",\"color\":\"gray\"},";
        
        if (!stats.tagsDisplay.empty()) {
            for (const auto& raw : stats.rawTags) {
                if (raw == "URCHIN_CHECKED" || raw == "SERAPH_CHECKED") continue;
                size_t sep = raw.find('\x1F');
                if (sep != std::string::npos) {
                    std::string service_type = raw.substr(0, sep);
                    std::string reason = raw.substr(sep + 1);
                    
                    std::string service = "";
                    std::string type = "";
                    size_t colon = service_type.find(':');
                    if (colon != std::string::npos) {
                        service = service_type.substr(0, colon);
                        type = service_type.substr(colon + 1);
                    } else {
                        type = service_type;
                    }
                    
                    std::string abbr = getAbbrChat(type);
                    if (abbr.empty()) {
                        if (service == "URCHIN") abbr = "\xC2\xA7""4[U]";
                        else if (service == "SERAPH") abbr = "\xC2\xA7""4[S]";
                        else abbr = "\xC2\xA7""5[T]";
                    }
                    
                    std::string escReason = escapeJsonChat(reason);
                    injectNodes += "{\"text\":\" \"},{\"text\":\"" + abbr + "\",\"hoverEvent\":{\"action\":\"show_text\",\"value\":\"" + escReason + "\"}},";
                }
            }
        }
        
        injectNodes += statsInject;
    }

    size_t namePos = rawJson.find(playerName);
    size_t insertPos = std::string::npos;
    size_t textColonPos = rawJson.find("\"text\":\":");
    if (textColonPos != std::string::npos) {
        insertPos = rawJson.rfind('{', textColonPos);
    }
    if (insertPos != std::string::npos) {
        rawJson.insert(insertPos, injectNodes);
    } else {
        if (namePos != std::string::npos) {
            size_t componentStart = rawJson.rfind('{', namePos);
            if (componentStart != std::string::npos) {
                int braceCount = 0;
                size_t bracePos = std::string::npos;
                for (size_t i = componentStart; i < rawJson.length(); ++i) {
                    if (rawJson[i] == '{') braceCount++;
                    else if (rawJson[i] == '}') {
                        braceCount--;
                        if (braceCount == 0) {
                            bracePos = i;
                            break;
                        }
                    }
                }
                if (bracePos != std::string::npos) {
                    if (bracePos + 1 < rawJson.length() && rawJson[bracePos + 1] == ',') {
                        rawJson.insert(bracePos + 2, injectNodes);
                    }
                }
            }
        }
    }
    
    if (!hasStarInChat(unformatted, rawJson)) {
        if (!stats.isNicked) {
            std::string lvlStr = BedwarsStars::GetFormattedLevel(stats);
            std::string lvlInject = "{\"text\":\"" + lvlStr + " \"},";
            
            size_t insertPos = std::string::npos;
            size_t shoutPos = rawJson.find("[SHOUT]");
            if (shoutPos == std::string::npos) shoutPos = rawJson.find("[SPECTATOR]");
            if (shoutPos == std::string::npos) shoutPos = rawJson.find("[TEAM]");
            if (shoutPos == std::string::npos) shoutPos = rawJson.find("[ALL]");
            
            size_t currentNamePos = rawJson.find(playerName);
            if (shoutPos != std::string::npos && currentNamePos != std::string::npos && shoutPos < currentNamePos) {
                size_t shoutStart = rawJson.rfind('{', shoutPos);
                if (shoutStart != std::string::npos) {
                    int braceCount = 0;
                    size_t shoutEnd = std::string::npos;
                    for (size_t i = shoutStart; i < rawJson.length(); ++i) {
                        if (rawJson[i] == '{') braceCount++;
                        else if (rawJson[i] == '}') {
                            braceCount--;
                            if (braceCount == 0) { shoutEnd = i; break; }
                        }
                    }
                    
                    if (shoutEnd != std::string::npos) {
                        size_t nextOpen = rawJson.find('{', shoutEnd);
                        if (nextOpen != std::string::npos && nextOpen <= currentNamePos) {
                            insertPos = nextOpen;
                        }
                    }
                }
            }
            
            if (insertPos != std::string::npos) {
                rawJson.insert(insertPos, lvlInject);
            } else {
                size_t extraPos = rawJson.find("\"extra\":[");
                if (extraPos != std::string::npos) {
                    rawJson.insert(extraPos + 9, lvlInject);
                } else if (currentNamePos != std::string::npos) {
                    size_t componentStart = rawJson.rfind('{', currentNamePos);
                    if (componentStart != std::string::npos) {
                        rawJson.insert(componentStart, lvlInject);
                    }
                }
            }
        }
    }
    
    return rawJson;
}

static void JNICALL onMethodEntry(jvmtiEnv *jvmti_env, JNIEnv *jni_env, jthread thread, jmethodID method)
{
	if (method == g_printChatMessage) {
		if (g_ignoreNextChat) {
			g_ignoreNextChat = false;
			return;
		}

		jobject chatComponent = nullptr;
		jvmtiError err = jvmti_env->GetLocalObject(thread, 0, 1, &chatComponent);
		if (err == JVMTI_ERROR_NONE && chatComponent != nullptr) {
			bool shouldCancel = false;
			jstring jstr = (jstring)jni_env->CallObjectMethod(chatComponent, g_getUnformattedText);
			if (jstr) {
				const char* chars = jni_env->GetStringUTFChars(jstr, nullptr);
				if (chars) {
					std::string unformatted(chars);
					OVson::enqueueNativeChat(unformatted);
					
					if (Config::isChatStatsEnabled() && isChatStatsActiveGame() && !shouldIgnoreChatMessage(unformatted)) {
						std::string playerName = extractNameFromChat(unformatted);
						Logger::info("[ChatHook] Intercepted chat. Extracted name: '%s', text: '%s'", playerName.c_str(), unformatted.c_str());
						if (!playerName.empty()) {
							bool found = false;
							Hypixel::PlayerStats stats;
							{
								std::lock_guard<std::mutex> lock(OVson::g_cacheMutex);
								auto it = OVson::g_persistentStatsCache.find(playerName);
								if (it != OVson::g_persistentStatsCache.end() && it->second.stats.isFetched) {
									stats = it->second.stats;
									found = true;
								}
							}

							jclass serializerCls = jni_env->FindClass("net/minecraft/util/IChatComponent$Serializer");
							if (serializerCls) {
								jmethodID toJson = lc->GetStaticMethodID(serializerCls, "componentToJson", "(Lnet/minecraft/util/IChatComponent;)Ljava/lang/String;", "func_150699_a", "a", "(Leu;)Ljava/lang/String;");
								if (toJson) {
									jstring jsonJStr = (jstring)jni_env->CallStaticObjectMethod(serializerCls, toJson, chatComponent);
									if (jsonJStr) {
										const char* jsonChars = jni_env->GetStringUTFChars(jsonJStr, nullptr);
										if (jsonChars) {
											std::string rawJson(jsonChars);
											shouldCancel = true;
											
											if (found) {
												Logger::info("[ChatHook] Stats found in cache for %s. Injecting instantly.", playerName.c_str());
												std::string newJson = injectStatsIntoJson(rawJson, playerName, stats, Config::getChatStatsFormat(), unformatted);
												RenderHook::enqueueTask([newJson]() {
													g_ignoreNextChat = true;
													ChatSDK::showJsonMessage(newJson);
												});
											} else {
												Logger::info("[ChatHook] Stats NOT in cache for %s. Delaying chat message...", playerName.c_str());
												OVson::requestStatsForVisiblePlayer(playerName, "");
												std::string fmtOpt = Config::getChatStatsFormat();
												std::thread([playerName, rawJson, fmtOpt, unformatted]() {
													try {
														ULONGLONG start = GetTickCount64();
														bool tFound = false;
														Hypixel::PlayerStats tStats;
														while(GetTickCount64() - start < 3000) {
															{
																std::lock_guard<std::mutex> lock(OVson::g_cacheMutex);
																auto it = OVson::g_persistentStatsCache.find(playerName);
																if (it != OVson::g_persistentStatsCache.end() && it->second.stats.isFetched) {
																	tStats = it->second.stats;
																	tFound = true;
																	break;
																}
															}
															Sleep(50);
														}
														if (tFound) {
															Logger::info("[ChatHook] Async wait success for %s. Injecting.", playerName.c_str());
															std::string newJson = injectStatsIntoJson(rawJson, playerName, tStats, fmtOpt, unformatted);
															RenderHook::enqueueTask([newJson]() {
																try {
																	g_ignoreNextChat = true;
																	ChatSDK::showJsonMessage(newJson);
																} catch (...) {}
															});
														} else {
															Logger::info("[ChatHook] Async wait timeout for %s. Sending original.", playerName.c_str());
															RenderHook::enqueueTask([rawJson]() {
																try {
																	g_ignoreNextChat = true;
																	ChatSDK::showJsonMessage(rawJson);
																} catch (...) {}
															});
														}
													} catch (...) {}
												}).detach();
											}
											jni_env->ReleaseStringUTFChars(jsonJStr, jsonChars);
										}
										jni_env->DeleteLocalRef(jsonJStr);
									}
								}
								jni_env->DeleteLocalRef(serializerCls);
							}
						}
					}
					jni_env->ReleaseStringUTFChars(jstr, chars);
				}
				jni_env->DeleteLocalRef(jstr);
			}
			jni_env->DeleteLocalRef(chatComponent);
			
			if (shouldCancel) {
				jvmti_env->ForceEarlyReturnVoid(thread);
			}
		}
	}
}

bool ChatHook::install()
{
	JNIEnv* env = lc->getEnv();
	if (!lc->jvmti || !env)
		return false;
	Logger::info("Installing chat hook");

	jclass playerCls = lc->GetClass("net.minecraft.client.entity.EntityPlayerSP");
	if (!playerCls){
		Logger::error("EntityPlayerSP not found");
		return false;
	}
	g_sendChatMessage = lc->GetMethodID(playerCls, "sendChatMessage", "(Ljava/lang/String;)V", "func_71165_d", "e");
	if (!g_sendChatMessage){
		Logger::error("sendChatMessage method not found");
		return false;
	}

	jclass gncCls = lc->GetClass("net.minecraft.client.gui.GuiNewChat");
	if (gncCls) {
		g_printChatMessage = lc->GetMethodID(gncCls, "printChatMessage", "(Lnet/minecraft/util/IChatComponent;)V", "func_146227_a", "a", "(Leu;)V");
	}
	
	jclass iccCls = lc->GetClass("net.minecraft.util.IChatComponent");
	if (iccCls) {
		g_getUnformattedText = lc->GetMethodID(iccCls, "getUnformattedText", "()Ljava/lang/String;", "func_150260_c", "c");
	}

	Logger::info("Native Chat hook initialized (using Netty Pipeline)");
	return true;
}

void ChatHook::uninstall()
{
	if (!lc->jvmti)
		return;
	lc->jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_METHOD_ENTRY, nullptr);
	jvmtiEventCallbacks cbs{};
	lc->jvmti->SetEventCallbacks(&cbs, sizeof(cbs));
}

bool ChatHook::onClientSendMessage(const std::string &message)
{
  try {
	// 1. Check for @prefix (OVson IRC Chat)
	if (!message.empty() && message[0] == '@') {
		if (Config::isIrcEnabled()) {
			std::string ircText = message.substr(1);
			IrcService::sendMessage(ircText);
			return true; // Block sending to vanilla/server chat
		}
	}

	if (CommandRegistry::instance().tryDispatch(message))
	{
		// command handled. block original send
		return true;
	}

    JNIEnv* env = lc ? lc->getEnv() : nullptr;
    if (env) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        jclass eventCls = PluginLoader::loadAPIClass(env, "net.ovson.api.event.ChatSendEvent");
        if (eventCls) {
            jmethodID ctor = env->GetMethodID(eventCls, "<init>", "(Ljava/lang/String;)V");
            if (ctor) {
                jstring jmsg = Lunar::createSafeJString(env, message);
                if (jmsg) {
                    jobject eventObj = env->NewObject(eventCls, ctor, jmsg);
                    if (eventObj) {
                        PluginLoader::postEvent(eventObj);
                        
                        jmethodID isCancelled = env->GetMethodID(eventCls, "isCancelled", "()Z");
                        if (isCancelled && env->CallBooleanMethod(eventObj, isCancelled)) {
                            env->DeleteLocalRef(eventObj);
                            env->DeleteLocalRef(jmsg);
                            env->DeleteLocalRef(eventCls);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            return true;
                        }
                        env->DeleteLocalRef(eventObj);
                    }
                    env->DeleteLocalRef(jmsg);
                }
            }
            env->DeleteLocalRef(eventCls);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    const std::string& prefix = Config::getCommandPrefix();
    if (!message.empty() && message.substr(0, prefix.length()) == prefix) {
        if (!(message.length() >= prefix.length() * 2 && message.substr(prefix.length(), prefix.length()) == prefix)) {
            ChatSDK::showPrefixed("§cUnknown command: §f" + message);
            return true;
        }
    }

    s_lastSentChatMessage = message;
    s_lastSentChatTime = GetTickCount64();

	return false;
  } catch (...) {
    return false;
  }
}

bool ChatHook::wasMessageSentRecentlyBySelf(const std::string& message) {
    if (s_lastSentChatMessage.empty() || s_lastSentChatTime == 0) return false;
    ULONGLONG now = GetTickCount64();
    if (now - s_lastSentChatTime > 5000) return false;

    std::string a = message;
    std::string b = s_lastSentChatMessage;
    while (!a.empty() && (a.back() == ' ' || a.back() == '\r' || a.back() == '\n' || a.back() == '\t')) a.pop_back();
    while (!b.empty() && (b.back() == ' ' || b.back() == '\r' || b.back() == '\n' || b.back() == '\t')) b.pop_back();
    while (!a.empty() && (a.front() == ' ' || a.front() == '\t')) a.erase(a.begin());
    while (!b.empty() && (b.front() == ' ' || b.front() == '\t')) b.erase(b.begin());
    return (!a.empty() && a == b);
}

std::string ChatHook::processIncomingChat(const std::string& unformatted, const std::string& rawJson) {
	if (g_ignoreNextChat) {
		g_ignoreNextChat = false;
		return rawJson;
	}

	EventDispatcher::postChatReceivedEvent(unformatted);
	OVson::Bedwars::Runtime::instance().onChatMessage(unformatted);

	OVson::enqueueNativeChat(unformatted);

	std::string modifiedBowJson;
	if (BowDistance::processChat(unformatted, rawJson, modifiedBowJson)) {
		return modifiedBowJson;
	}

	if (!Config::isChatStatsEnabled()) {
		return rawJson;
	}

	if (!isChatStatsActiveGame()) {
		return rawJson;
	}

	if (shouldIgnoreChatMessage(unformatted)) {
		return rawJson;
	}

	std::string playerName = extractNameFromChat(unformatted);

	if (playerName.empty()) {
		return rawJson;
	}

	Logger::info("[ChatHook-Netty] Intercepted chat. Extracted name: '%s', text: '%s'", playerName.c_str(), unformatted.c_str());

	bool found = false;
	Hypixel::PlayerStats stats;
	{
		std::lock_guard<std::mutex> lock(OVson::g_cacheMutex);
		auto it = OVson::g_persistentStatsCache.find(playerName);
		if (it != OVson::g_persistentStatsCache.end() && it->second.stats.isFetched) {
			stats = it->second.stats;
			found = true;
		}
	}
	if (!found) {
		std::lock_guard<std::recursive_mutex> lock(OVson::g_statsMutex);
		auto it2 = OVson::g_playerStatsMap.find(playerName);
		if (it2 != OVson::g_playerStatsMap.end() && it2->second.isFetched) {
			stats = it2->second;
			found = true;
		}
	}

	if (found) {
		Logger::info("[ChatHook-Netty] Stats found in cache for %s. Injecting instantly.", playerName.c_str());
		return injectStatsIntoJson(rawJson, playerName, stats, Config::getChatStatsFormat(), unformatted);
	} else {
		Logger::info("[ChatHook-Netty] Stats NOT in cache for %s. Delaying chat message...", playerName.c_str());
		OVson::requestStatsForVisiblePlayer(playerName, "");
		std::string fmtOpt = Config::getChatStatsFormat();
		std::string styleOpt = Config::getChatStatsStyle();
		std::thread([playerName, rawJson, fmtOpt, styleOpt, unformatted]() {
			try {
				ULONGLONG start = GetTickCount64();
				bool tFound = false;
				Hypixel::PlayerStats tStats;
				while(GetTickCount64() - start < 3000) {
					{
						std::lock_guard<std::mutex> lock(OVson::g_cacheMutex);
						auto it = OVson::g_persistentStatsCache.find(playerName);
						if (it != OVson::g_persistentStatsCache.end() && it->second.stats.isFetched) {
							tStats = it->second.stats;
							tFound = true;
							break;
						}
					}
					if (!tFound) {
						std::lock_guard<std::recursive_mutex> lock(OVson::g_statsMutex);
						auto it2 = OVson::g_playerStatsMap.find(playerName);
						if (it2 != OVson::g_playerStatsMap.end() && it2->second.isFetched) {
							tStats = it2->second;
							tFound = true;
							break;
						}
					}
					Sleep(50);
				}
				if (tFound) {
					Logger::info("[ChatHook-Netty] Async wait success for %s. Injecting.", playerName.c_str());
					std::string newJson = injectStatsIntoJson(rawJson, playerName, tStats, fmtOpt, unformatted);
					RenderHook::enqueueTask([newJson]() {
						try {
							g_ignoreNextChat = true;
							ChatSDK::showJsonMessage(newJson);
						} catch (...) {}
					});
				} else {
					Logger::info("[ChatHook-Netty] Async wait timeout for %s. Sending original.", playerName.c_str());
					RenderHook::enqueueTask([rawJson]() {
						try {
							g_ignoreNextChat = true;
							ChatSDK::showJsonMessage(rawJson);
						} catch (...) {}
					});
				}
			} catch (...) {}
		}).detach();

		return "[CANCEL]";
	}
}


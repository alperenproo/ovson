#pragma once

#include <string>
#include <vector>

namespace OVson::NickRoll {

struct PageSegment {
  std::string text;
  std::string clickAction;
  std::string clickValue;
};

struct BookPage {
  bool parsed = false;
  std::vector<PageSegment> segments;
  std::string plainText;
};

BookPage parsePageJson(const std::string &json);

bool isValidUsername(const std::string &value);

bool nicknameMatchesTargetWord(const std::string &nickname,
                               const std::string &targetWord);

bool shouldStopReroll(bool scorePasses, const std::string &nickname,
                      const std::string &targetWord);

bool isGeneratedNamePage(const BookPage &page);

std::string findGeneratedName(const BookPage &page);

std::string findButtonCommand(const BookPage &page, const std::string &label);

// tests
std::string stripFormattingCodes(const std::string &text);
std::string trim(const std::string &text);

} // namespace OVson::NickRoll

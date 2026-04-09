#include "ChatBypasser.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

std::string ChatBypasser::process(const std::string &input) {
  // basic mapping for bypass
  static const std::unordered_map<char, std::string> mapping = {
      {'a', "á"}, {'b', "b"}, {'c', "c"}, {'d', "d"}, {'e', "é"}, {'f', "f"},
      {'g', "g"}, {'h', "h"}, {'i', "í"}, {'j', "j"}, {'k', "k"}, {'l', "l"},
      {'m', "m"}, {'n', "n"}, {'o', "ó"}, {'p', "p"}, {'q', "q"}, {'r', "r"},
      {'s', "s"}, {'t', "t"}, {'u', "ú"}, {'v', "v"}, {'w', "w"}, {'x', "x"},
      {'y', "y"}, {'z', "z"}, {'A', "Á"}, {'B', "B"}, {'C', "C"}, {'D', "D"},
      {'E', "É"}, {'F', "F"}, {'G', "G"}, {'H', "H"}, {'I', "Í"}, {'J', "J"},
      {'K', "K"}, {'L', "L"}, {'M', "M"}, {'N', "N"}, {'O', "Ó"}, {'P', "P"},
      {'Q', "Q"}, {'R', "R"}, {'S', "S"}, {'T', "T"}, {'U', "Ú"}, {'V', "V"},
      {'W', "W"}, {'X', "X"}, {'Y', "Y"}, {'Z', "Z"}};

  std::string result;
  for (char ch : input) {
    if (mapping.count(ch)) {
      result += mapping.at(ch);
    } else {
      result += ch;
    }
  }
  return result;
}

std::string ChatBypasser::smartProcess(const std::string &input) {
  static const std::vector<std::string> blacklist = {
      "ez",   "kkk",  "nigga",  "retard", "fuck",   "hoe",  "whore",
      "rape", "porn", "faggot", "kys",    "nigger", "bitch", "niga", "niger"};

  auto normalize = [](const std::string& s) {
      std::string res;
      for (char c : s) {
          char cl = (char)std::tolower((unsigned char)c);
          if (res.empty() || cl != res.back()) {
              res += cl;
          }
      }
      return res;
  };

  std::string result;
  std::string word;

  auto flushWord = [&]() {
    if (!word.empty()) {
      bool shouldBypass = false;
      std::string lower = word;
      for (auto &c : lower)
        c = (char)std::tolower((unsigned char)c);

      std::string normalizedWord = normalize(word);

      for (const auto &bl : blacklist) {
        std::string normalizedBL = normalize(bl);
        // Short words like "ez", "kys", or normalized "kkk" ("k") require more precise matching
        if (normalizedBL.length() <= 2) {
          if (normalizedWord == normalizedBL) {
              shouldBypass = true;
              break;
          }
        } else if (normalizedWord.find(normalizedBL) != std::string::npos) {
          shouldBypass = true;
          break;
        }
      }

      if (shouldBypass) {
        if (lower.find("kkk") != std::string::npos) {
            result += (word[0] == 'K') ? "KK.K" : "kk.k";
        } else {
            result += process(word);
        }
      } else {
        result += word;
      }
      word.clear();
    }
  };

  for (char ch : input) {
    if (std::isalnum((unsigned char)ch)) {
      word += ch;
    } else {
      flushWord();
      result += ch;
    }
  }
  flushWord();
  return result;
}

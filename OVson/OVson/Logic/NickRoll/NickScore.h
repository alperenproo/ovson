#pragma once

#include <string>
#include <vector>

namespace OVson::NickRoll {

struct ScoreTell {
  std::string id;
  std::string label;
  int weight = 0;
  int share = 0;
  std::string detail;
};

struct NickScore {
  std::string nickname;
  bool valid = false;
  int score = 0;
  int tell = 0;
  int appeal = 0;
  int threshold = 70;
  bool passes = false;
  bool cappedByTell = false;
  std::string verdict;
  std::string pattern;
  std::string summary;
  std::vector<ScoreTell> tells;
  std::vector<std::string> parts;
};

NickScore scoreNickname(const std::string &nickname, int threshold = 70);

bool vocabularyAvailable();

// test
std::vector<std::string> splitWords(const std::string &nickname);

} // namespace OVson::NickRoll

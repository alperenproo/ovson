#pragma once

#include <string>
#include <vector>

// Scores a Hypixel /nick candidate. Pure logic: no JNI, no Windows, no state.
// The whole file is testable off-game, which matters because the command that
// opens the book is limited to six uses a day.
//
// This is a port of nickname-scoring-lab/js/scorer.js engine 3. The two are
// kept identical by tests/NickScoreParity.cpp, which replays every name in
// both corpora through both engines and fails on any disagreement. If you
// change a weight here, change it there.
//
// The model, in one line:
//
//     score = min(appeal, 100 - tell)
//
//   tell    how obviously the name reads as generator output   0..100
//   appeal  how much it reads like a real, owned account       0..100
//
// The two are never added. A tell CAPS the score, so no amount of appeal can
// buy past one. The question is deliberately not "did the generator make this"
// -- if it were, the right answer would be to reject every rollable name --
// but "would another player, seeing this in the tab list, think it is a nick".
namespace OVson::NickRoll {

struct ScoreTell {
  std::string id;     // stable identifier, e.g. "year_suffix"
  std::string label;  // human-readable, for the alert
  int weight = 0;     // the rule's full weight
  int share = 0;      // what it actually contributed after corroboration
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
  std::string verdict;  // Excellent / Good / Borderline / Reject / Invalid
  std::string pattern;  // strongest tell, as a short label
  std::string summary;  // one sentence for the alert
  std::vector<ScoreTell> tells;
  std::vector<std::string> parts;  // how the name was split
};

// `threshold` is the accept line, clamped to 0..100. Everything else is fixed:
// the weights were fitted against 2075 captured names and are not worth a knob.
NickScore scoreNickname(const std::string &nickname, int threshold = 70);

// False when the build has no private vocabulary header, in which case the
// word-aware rules are inert and only the shape rules fire. Scoring still
// works; it is just blunter. Surface this somewhere so a vocabulary-less build
// is never mistaken for a working one.
bool vocabularyAvailable();

// Exposed for tests and for the parity harness.
std::vector<std::string> splitWords(const std::string &nickname);

} // namespace OVson::NickRoll

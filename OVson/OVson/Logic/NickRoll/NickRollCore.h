#pragma once

#include <string>
#include <vector>

// Pure logic for the Hypixel /nick roll capture. No JNI, no Windows: the whole
// file is testable off-game, which matters more here than usual because the
// command that opens the book is limited to six uses a day.
namespace OVson::NickRoll {

// One piece of a rendered book page. `clickValue` is the command the piece
// would run if clicked, empty when the piece is not clickable -- which is how
// the generated name is told apart from the buttons underneath it.
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

// Minecraft book pages are IChatComponent JSON. Returns parsed=false on
// anything malformed rather than throwing or guessing.
BookPage parsePageJson(const std::string &json);

// Minecraft account name rules: 3-16 of [A-Za-z0-9_].
bool isValidUsername(const std::string &value);

// Case-insensitive substring match used by target-word rerolling. An empty
// target never matches; an exact match is naturally included.
bool nicknameMatchesTargetWord(const std::string &nickname,
                               const std::string &targetWord);

// A non-empty target replaces (rather than supplements) the score threshold
// as the auto-reroll stop rule.
bool shouldStopReroll(bool scorePasses, const std::string &nickname,
                      const std::string &targetWord);

// True when this page is the one offering a generated name.
bool isGeneratedNamePage(const BookPage &page);

// The generated name, or empty when this is not that page or the name could
// not be read with confidence. Never guesses: an unreadable page must leave
// the caller doing nothing, because a wrong action costs a daily /nick use.
std::string findGeneratedName(const BookPage &page);

// The command a labelled button would run if it were clicked -- the label is
// matched case-insensitively, e.g. "try again". Empty when the label is absent
// from this page, or when its click is some action other than run_command, in
// which case the caller must not improvise: a wrong guess here costs one of
// the six daily /nick uses.
std::string findButtonCommand(const BookPage &page, const std::string &label);

// Exposed for tests.
std::string stripFormattingCodes(const std::string &text);
std::string trim(const std::string &text);

} // namespace OVson::NickRoll

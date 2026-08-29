#include "NickScore.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <unordered_set>

#if __has_include("NickScoreVocabulary.h")
#include "NickScoreVocabulary.h"
#else
// A public checkout has no vocabulary. Everything still compiles and every
// shape rule still fires; only the rules that need to know a word go quiet.
#define OVSON_NICKSCORE_HAS_VOCABULARY 0
namespace OVson::NickRoll::Vocabulary {
inline constexpr const char *kFirstNames[] = {nullptr};
inline constexpr const char *kSurnames[] = {nullptr};
inline constexpr const char *kPoolWords[] = {nullptr};
inline constexpr const char *kAdjectives[] = {nullptr};
inline constexpr const char *kAmbiguousSurnames[] = {nullptr};
inline constexpr const char *kSegmentWords[] = {nullptr};
inline constexpr const char *kSeedWords[] = {nullptr};
inline constexpr const char *kSeedNames[] = {nullptr};
} // namespace OVson::NickRoll::Vocabulary
#endif

namespace OVson::NickRoll {
namespace {

using Words = std::unordered_set<std::string>;

template <std::size_t N> Words makeSet(const char *const (&source)[N]) {
  Words out;
  for (std::size_t index = 0; index < N; ++index) {
    if (source[index] != nullptr) out.insert(source[index]);
  }
  return out;
}

const Words &firstNames() { static const Words set = makeSet(Vocabulary::kFirstNames); return set; }
const Words &surnames() { static const Words set = makeSet(Vocabulary::kSurnames); return set; }
const Words &poolWords() { static const Words set = makeSet(Vocabulary::kPoolWords); return set; }
const Words &adjectives() { static const Words set = makeSet(Vocabulary::kAdjectives); return set; }
const Words &ambiguousSurnames() { static const Words set = makeSet(Vocabulary::kAmbiguousSurnames); return set; }
const Words &seedWords() { static const Words set = makeSet(Vocabulary::kSeedWords); return set; }
const Words &seedNames() { static const Words set = makeSet(Vocabulary::kSeedNames); return set; }

// The mined pool plus the curated lists, three letters or more. The captured
// pool contains one- and two-letter extraction artefacts ("b", "th", "rpy")
// and segmenting on those would find words inside anything.
const Words &segmentVocabulary() {
  static const Words set = [] {
    Words out;
    auto absorb = [&out](const Words &source) {
      for (const std::string &word : source) {
        if (word.size() >= 3) out.insert(word);
      }
    };
    absorb(makeSet(Vocabulary::kSegmentWords));
    absorb(poolWords());
    absorb(adjectives());
    absorb(firstNames());
    return out;
  }();
  return set;
}

bool has(const Words &set, const std::string &word) { return set.find(word) != set.end(); }

bool isUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool isLower(char c) { return c >= 'a' && c <= 'z'; }
bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isLetter(char c) { return isUpper(c) || isLower(c); }

std::string lower(const std::string &value) {
  std::string out = value;
  for (char &c : out) {
    if (isUpper(c)) c = static_cast<char>(c + 32);
  }
  return out;
}

bool isLegalName(const std::string &value) {
  if (value.size() < 3 || value.size() > 16) return false;
  for (char c : value) {
    if (!isLetter(c) && !isDigit(c) && c != '_') return false;
  }
  return true;
}

// Trailing run of digits, e.g. "AmyDunn2015" -> "2015".
std::string trailingDigits(const std::string &value) {
  std::size_t end = value.size();
  std::size_t start = end;
  while (start > 0 && isDigit(value[start - 1])) --start;
  return value.substr(start, end - start);
}

// 1980-2029 hanging off letters. The generator's signature, and the one
// pattern almost no real Hypixel account uses.
bool endsInYear(const std::string &value) {
  if (value.size() < 4) return false;
  const std::string tail = value.substr(value.size() - 4);
  for (char c : tail) {
    if (!isDigit(c)) return false;
  }
  if (value.size() > 4 && isDigit(value[value.size() - 5])) return false;
  const int year = std::stoi(tail);
  const bool inRange = (year >= 1980 && year <= 1999) || (year >= 2000 && year <= 2029);
  if (!inRange) return false;
  return std::any_of(value.begin(), value.end(), [](char c) { return isLetter(c); });
}

bool isXxWrapped(const std::string &value) {
  if (value.size() < 4) return false;
  const std::string head = lower(value.substr(0, 2));
  const std::string tail = lower(value.substr(value.size() - 2));
  return head == "xx" && tail == "xx";
}

// `(^|[A-Za-z])[0-9]+[A-Za-z]` -- a digit standing in for a letter INSIDE a
// word, as opposed to a suffix. "Fal3con" yes, "Falcon3" no.
bool hasLetterSubstitution(const std::string &value) {
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (!isDigit(value[index])) continue;
    if (index > 0 && !isLetter(value[index - 1])) continue;
    std::size_t end = index;
    while (end < value.size() && isDigit(value[end])) ++end;
    if (end < value.size() && isLetter(value[end])) return true;
  }
  return false;
}

// Digits that plausibly stand in for a letter. 2, 6 and 9 are absent on
// purpose: nobody reads those as letters.
int countLeetDigits(const std::string &value) {
  static const std::array<char, 7> kLeet = {'0', '1', '3', '4', '5', '7', '8'};
  std::string inner = value;
  const std::string tail = trailingDigits(inner);
  inner.resize(inner.size() - tail.size());
  int count = 0;
  for (char c : inner) {
    if (std::find(kLeet.begin(), kLeet.end(), c) != kLeet.end()) ++count;
  }
  return count;
}

double vowelRatio(const std::string &word) {
  int letters = 0;
  int vowels = 0;
  for (char c : word) {
    if (!isLower(c)) continue;
    ++letters;
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y') ++vowels;
  }
  if (letters == 0) return 0.0;
  return static_cast<double>(vowels) / static_cast<double>(letters);
}

bool pronounceable(const std::string &word) {
  if (word.size() <= 2) return false;
  const double ratio = vowelRatio(word);
  // The upper bound has to be generous: "evie", "aria" and "ivy" are 75%
  // vowels and are obviously sayable. Only the consonant-soup end matters.
  if (ratio < 0.2 || ratio > 0.8) return false;
  int run = 0;
  for (char c : word) {
    const bool consonant = isLower(c) && c != 'a' && c != 'e' && c != 'i' && c != 'o' && c != 'u' && c != 'y';
    run = consonant ? run + 1 : 0;
    if (run >= 4) return false;
  }
  return true;
}

// A token that could plausibly be somebody's given name without having to be
// in the list. The captured vocabulary has a long tail -- 640 parts appeared
// exactly once -- so a rule that only fired on known names would miss every
// name nobody has rolled yet.
bool nameShaped(const std::string &word) {
  if (word.size() < 3) return false;
  for (char c : word) {
    if (!isLower(c)) return false;
  }
  return pronounceable(word);
}

// `[A-Z][a-z]+ | [A-Z]+(?![a-z]) | [a-z]+ | \d+`, by hand. Matching the
// regex's backtracking matters: "AAce" has to come out "A" + "Ace", not
// "AA" + "ce".
void tokenizeChunk(const std::string &chunk, std::vector<std::string> &out) {
  std::size_t index = 0;
  const std::size_t size = chunk.size();
  while (index < size) {
    const char c = chunk[index];
    if (isUpper(c)) {
      if (index + 1 < size && isLower(chunk[index + 1])) {
        std::size_t end = index + 1;
        while (end < size && isLower(chunk[end])) ++end;
        out.push_back(chunk.substr(index, end - index));
        index = end;
      } else {
        std::size_t end = index;
        while (end < size && isUpper(chunk[end])) ++end;
        if (end < size && isLower(chunk[end])) --end;  // the lookahead backtrack
        out.push_back(chunk.substr(index, end - index));
        index = end;
      }
    } else if (isLower(c)) {
      std::size_t end = index;
      while (end < size && isLower(chunk[end])) ++end;
      out.push_back(chunk.substr(index, end - index));
      index = end;
    } else if (isDigit(c)) {
      std::size_t end = index;
      while (end < size && isDigit(chunk[end])) ++end;
      out.push_back(chunk.substr(index, end - index));
      index = end;
    } else {
      ++index;
    }
  }
}

// "thetoxicmeow" is the + toxic + meow. An all-lowercase name has no camel
// seams, and 42% of the captured corpus arrives that way, so every rule that
// counts words was blind to nearly half of it.
//
// The split is only believed when EVERY piece is known vocabulary. A partial
// match is not a match: "officialgod", "atomicunit" and "technoblade" each
// contain a known word and none of them decompose, so they stay one word.
// Fewest pieces wins, so a name is never chopped finer than it has to be.
//
// Two pieces is an ordinary compound and stays one word: a seamless WordWord
// reads as a believable handle and half the real accounts in the game are one.
// Only a pile of three or more is a tell.
bool segmentKnown(const std::string &word, std::vector<std::string> &out) {
  if (word.size() < 9) return false;
  for (char c : word) {
    if (!isLower(c)) return false;
  }
  const std::size_t size = word.size();
  std::vector<std::vector<std::string>> best(size + 1);
  std::vector<bool> reachable(size + 1, false);
  reachable[0] = true;
  for (std::size_t end = 3; end <= size; ++end) {
    for (std::size_t start = 0; start + 3 <= end; ++start) {
      if (!reachable[start]) continue;
      if (!has(segmentVocabulary(), word.substr(start, end - start))) continue;
      if (!reachable[end] || best[start].size() + 1 < best[end].size()) {
        best[end] = best[start];
        best[end].push_back(word.substr(start, end - start));
        reachable[end] = true;
      }
    }
  }
  if (!reachable[size] || best[size].size() < 3) return false;
  out = best[size];
  return true;
}

// Surnames carry the recognition here, not first names. "Sanchez", "Hawkins"
// and "Robinson" are unmistakably surnames; first names overlap with ordinary
// words -- Max, Rose, Summer, Sky -- so keying the split on them would drag
// real handles in with the nicks.
//
// Plenty of English surnames are also ordinary words: Stone, Green, Wood,
// King, Fox. When the surname half is one of those, the other half has to be a
// known given name before the split is believed, or "Moonstone", "Evergreen"
// and "Silverstone" all read as personas.
bool splitGlued(const std::string &word, std::string &head, std::string &tail) {
  if (word.size() < 6) return false;
  for (const std::string &surname : surnames()) {
    if (word.size() <= surname.size() + 2) continue;
    if (word.compare(word.size() - surname.size(), surname.size(), surname) != 0) continue;
    const std::string candidate = word.substr(0, word.size() - surname.size());
    const bool strict = !has(ambiguousSurnames(), surname);
    if (has(firstNames(), candidate) ||
        (strict && nameShaped(candidate) && !has(adjectives(), candidate))) {
      head = candidate;
      tail = surname;
      return true;
    }
  }
  // The tail must be a KNOWN surname here. Accepting merely name-shaped tails
  // turned "maxfly" into "max + fly" and rejected it as a person.
  for (const std::string &first : firstNames()) {
    if (word.size() <= first.size() + 3) continue;
    if (word.compare(0, first.size(), first) != 0) continue;
    const std::string rest = word.substr(first.size());
    if (has(surnames(), rest)) {
      head = first;
      tail = rest;
      return true;
    }
  }
  return false;
}

// "Does this read as an actual human being's name." The most decisive signal
// in the corpus: 39% of captured names are one. People do not name a Minecraft
// account after a plausible person -- "ZacharySanchez" is not a handle, it is
// a persona.
bool personName(const std::vector<std::string> &parts, std::string &description) {
  if (parts.size() == 2) {
    const std::string &a = parts[0];
    const std::string &b = parts[1];
    if (has(surnames(), b) && !has(poolWords(), b) &&
        (has(firstNames(), a) ||
         (!has(ambiguousSurnames(), b) && nameShaped(a) && !has(adjectives(), a)))) {
      description = a + " " + b;
      return true;
    }
    // Given name plus given name, or given name plus surname. Deliberately NOT
    // "given name plus anything sayable": that read "TommyInnit" as a person
    // and it is one of the best-known real accounts in the game.
    if (has(firstNames(), a) && b.size() >= 4 && !has(poolWords(), b) && !has(adjectives(), b) &&
        (has(surnames(), b) || has(firstNames(), b))) {
      description = a + " " + b;
      return true;
    }
    return false;
  }
  if (parts.size() == 1) {
    std::string head;
    std::string tail;
    if (splitGlued(parts[0], head, tail)) {
      description = head + " " + tail;
      return true;
    }
  }
  return false;
}

// "lastandmax" -- the generator's X-and-Y compound with the capital letters
// gone. Both halves must be recognisable vocabulary before the split is
// believed, otherwise "alexander", "island", "brandon" and "random" all read
// as and-compounds.
bool gluedAndCompound(const std::string &value, std::string &description) {
  auto known = [](const std::string &word) {
    return has(poolWords(), word) || has(adjectives(), word) || has(firstNames(), word);
  };
  for (std::size_t index = 1; index + 3 < value.size(); ++index) {
    if (value.compare(index, 3, "and") != 0) continue;
    const std::string left = value.substr(0, index);
    const std::string right = value.substr(index + 3);
    if (left.size() >= 2 && right.size() >= 2 && known(left) && known(right)) {
      description = left + " and " + right;
      return true;
    }
  }
  return false;
}

const char *patternLabel(const std::string &id) {
  if (id == "person_name") return "Person name";
  if (id == "year_suffix") return "Name plus year";
  if (id == "xx_wrapper") return "Xx wrapper";
  if (id == "and_infix") return "X-and-Y compound";
  if (id == "stock_prefix") return "Im / Its / Itz / The prefix";
  if (id == "three_digits") return "Name plus three digits";
  if (id == "digit_run") return "Name plus digit run";
  if (id == "adjective_name") return "Adjective plus name";
  if (id == "compound_digit") return "Compound plus a filler digit";
  if (id == "pool_underscore") return "Stock words split by an underscore";
  if (id == "word_pile") return "Stacked words";
  if (id == "leet") return "Leetspeak";
  if (id == "very_long") return "Long name";
  return "Generated";
}

} // namespace

bool vocabularyAvailable() { return OVSON_NICKSCORE_HAS_VOCABULARY && !firstNames().empty(); }

std::vector<std::string> splitWords(const std::string &nickname) {
  std::string body = nickname;
  if (isXxWrapped(body)) body = body.substr(2, body.size() - 4);
  const std::string tail = trailingDigits(body);
  body.resize(body.size() - tail.size());

  std::vector<std::string> raw;
  std::string chunk;
  for (char c : body) {
    if (c == '_') {
      if (!chunk.empty()) tokenizeChunk(chunk, raw);
      chunk.clear();
    } else {
      chunk.push_back(c);
    }
  }
  if (!chunk.empty()) tokenizeChunk(chunk, raw);

  std::vector<std::string> parts;
  for (const std::string &piece : raw) {
    if (std::any_of(piece.begin(), piece.end(), [](char c) { return isLetter(c); })) {
      parts.push_back(lower(piece));
    }
  }
  return parts;
}

NickScore scoreNickname(const std::string &nickname, int threshold) {
  NickScore result;
  result.nickname = nickname;
  result.threshold = std::min(100, std::max(0, threshold));

  if (!isLegalName(nickname)) {
    result.verdict = "Invalid";
    result.summary = "Not a legal Minecraft name (3-16 of A-Z a-z 0-9 _).";
    return result;
  }
  result.valid = true;

  const std::string canonical = lower(nickname);
  const std::string digits = trailingDigits(nickname);
  const int length = static_cast<int>(nickname.size());

  int digitCount = 0;
  int underscoreCount = 0;
  for (char c : nickname) {
    if (isDigit(c)) ++digitCount;
    else if (c == '_') ++underscoreCount;
  }

  std::vector<std::string> parts = splitWords(nickname);

  // A seamless blob is split by hand: first by full vocabulary segmentation,
  // then by a known prefix. "itztimmy" is the same name as "ItzTimmy" and has
  // to be read the same.
  if (parts.size() == 1) {
    std::vector<std::string> pieces;
    if (segmentKnown(parts[0], pieces)) {
      parts = pieces;
    } else {
      static const char *kPrefixes[] = {"im", "its", "itz", "the", "th3"};
      for (const char *prefix : kPrefixes) {
        const std::string head(prefix);
        if (parts[0].size() <= head.size() || parts[0].compare(0, head.size(), head) != 0) continue;
        const std::string rest = parts[0].substr(head.size());
        if (has(firstNames(), rest) || has(poolWords(), rest) || has(adjectives(), rest)) {
          parts = {head, rest};
          break;
        }
      }
    }
  }
  result.parts = parts;

  const int leetDigits = hasLetterSubstitution(nickname) ? countLeetDigits(nickname) : 0;

  // ---- tells ------------------------------------------------------------
  std::vector<ScoreTell> tells;
  auto add = [&tells](const char *id, const char *label, int weight, const std::string &detail) {
    if (weight > 0) tells.push_back(ScoreTell{id, label, weight, 0, detail});
  };

  std::string person;
  if (personName(parts, person)) {
    add("person_name", "Reads as a real person's name", 95, "first name and surname: " + person);
  }

  if (endsInYear(nickname)) {
    add("year_suffix", "Ends in a year", 82, "ends in " + nickname.substr(nickname.size() - 4));
  } else if (digits.size() == 3) {
    // Three digits reads as filler. Two does not -- real accounts do that too.
    add("three_digits", "Random three-digit tail", 68, "ends in " + digits);
  } else if (digits.size() >= 5) {
    add("digit_run", "Long run of digits", 55, "ends in " + std::to_string(digits.size()) + " digits");
  }

  {
    std::string stripped;
    for (char c : canonical) {
      if (c != '_') stripped.push_back(c);
    }
    const std::string strippedTail = trailingDigits(stripped);
    stripped.resize(stripped.size() - strippedTail.size());

    // Both boundary tests read the ORIGINAL casing, not the lowered copy.
    // "ToxicHandMiss" and "The_Sans_Hand" contain "and" preceded by a capital,
    // which is a word boundary inside "Hand" rather than a connector; testing
    // the lowered string made both of them read as X-and-Y compounds.
    bool andInfix = false;
    std::string andDetail = "generator's X-and-Y compound";
    for (std::size_t index = 1; index + 3 <= nickname.size(); ++index) {
      if (nickname.compare(index, 3, "and") != 0) continue;
      if (!isLower(nickname[index - 1]) && nickname[index - 1] != '_') continue;
      const std::size_t after = index + 3;
      const bool rightOk = after >= nickname.size() || isUpper(nickname[after]) ||
                           nickname[after] == '_' || isDigit(nickname[after]);
      if (rightOk) { andInfix = true; break; }
    }
    if (!andInfix) {
      // The literal "_and_", in any casing.
      for (std::size_t index = 0; index + 5 <= canonical.size(); ++index) {
        if (canonical.compare(index, 5, "_and_") == 0) { andInfix = true; break; }
      }
    }
    if (!andInfix && parts.size() >= 3) {
      for (std::size_t index = 1; index < parts.size(); ++index) {
        if (parts[index] == "and") { andInfix = true; break; }
      }
    }
    if (!andInfix) andInfix = gluedAndCompound(stripped, andDetail);
    if (andInfix) add("and_infix", "Uses \"and\" as a connector", 84, andDetail);
  }

  if (isXxWrapped(nickname) && length > 4) {
    add("xx_wrapper", "Wrapped in Xx...xX", 88, "Xx ... xX");
  }

  // A stock prefix only counts when a stock word follows it. "Theo" and
  // "Import" begin with the same letters and are not prefixed at all.
  if (parts.size() >= 2) {
    static const char *kPrefixes[] = {"im", "its", "itz", "the", "th3"};
    for (const char *prefix : kPrefixes) {
      const std::string head(prefix);
      if (canonical.size() <= head.size() || canonical.compare(0, head.size(), head) != 0) continue;
      bool stock = false;
      for (std::size_t index = 1; index < parts.size(); ++index) {
        if (has(firstNames(), parts[index]) || has(poolWords(), parts[index]) ||
            has(adjectives(), parts[index])) {
          stock = true;
          break;
        }
      }
      if (stock) {
        add("stock_prefix", "Generator prefix", 66,
            "starts with " + head + " and continues with a stock word");
      }
      break;
    }
  }

  // "ColdMarc", "LazyRobin", "SansDani" -- adjective plus a given name is a
  // generator grammar. Deliberately narrow: the adjective must be on the left
  // and the given name on the right, so "maxfly" does not fire.
  if (parts.size() == 2 && has(adjectives(), parts[0]) && has(firstNames(), parts[1]) &&
      !has(adjectives(), parts[1])) {
    add("adjective_name", "Adjective plus a given name", 55, parts[0] + " + " + parts[1]);
  }

  // "Cute_Herr", "Ultra_End", "Icy_Soy" -- one underscore between exactly two
  // stock words. Measured on the 1000-name sheet this was the largest family
  // still getting through. Narrow on purpose: it needs BOTH halves known AND
  // the underscore, so "IcyWater" and "DeadlyAir" keep their score.
  if (underscoreCount == 1 && parts.size() == 2 && digits.empty() &&
      has(segmentVocabulary(), parts[0]) && has(segmentVocabulary(), parts[1])) {
    add("pool_underscore", "Two stock words split by an underscore", 40, parts[0] + "_" + parts[1]);
  }

  // Real accounts that carry a digit almost always hang it off a SINGLE word
  // ("Evie77", "Kai86"); it is the two-words-plus-a-number shape that reads as
  // machine output.
  if (parts.size() >= 2 && digits.size() >= 1 && digits.size() <= 2) {
    add("compound_digit", "Compound with a filler digit", 45, "two words plus " + digits);
  }

  // Substituted letters in a SHORT name are an old-account habit ("bl4ze",
  // "0nyx", "4rr0w"), not a generator habit: the generator leets long,
  // multi-word names. Nothing in either corpus leets six characters or fewer.
  if (leetDigits > 0 && length >= 7) {
    add("leet", "Digits standing in for letters", 38,
        std::to_string(leetDigits) + " substituted letter(s)");
  }

  if (parts.size() >= 3) {
    add("word_pile", "Words stacked together", 48, std::to_string(parts.size()) + " words stacked");
  }

  // 74% of the captured corpus is 13 characters or more. Weak alone -- plenty
  // of real accounts are long -- but it corroborates everything else.
  if (length >= 13) {
    add("very_long", "Unusually long", 30, std::to_string(length) + " characters");
  }

  // The tells overlap: a person-name pair nearly always carries a year and is
  // nearly always long. Adding them would count one observation three times,
  // which is how the previous engine drove every long name to zero. The
  // strongest sets the level; the rest add 12% of theirs.
  std::sort(tells.begin(), tells.end(), [](const ScoreTell &left, const ScoreTell &right) {
    if (left.weight != right.weight) return left.weight > right.weight;
    return left.id < right.id;
  });
  double tellTotal = 0.0;
  for (std::size_t index = 0; index < tells.size(); ++index) {
    const double share = index == 0 ? tells[index].weight : tells[index].weight * 0.12;
    tells[index].share = static_cast<int>(std::lround(share));
    tellTotal += share;
  }
  result.tell = tells.empty() ? 0 : std::min(100, static_cast<int>(std::lround(tellTotal)));
  result.tells = tells;

  // ---- appeal -----------------------------------------------------------
  // Length is the spine. Short is prestigious because short names were claimed
  // a decade ago. The curve is gentler than it looks like it should be on
  // purpose: only 45 of 1075 captured names carry no tell at all, so the tells
  // already do the discriminating and a steep length penalty only taxed long
  // real handles.
  double appeal = length <= 4    ? 100.0
                  : length <= 6  ? 96.0
                  : length <= 8  ? 90.0
                  : length <= 10 ? 80.0
                  : length <= 12 ? 68.0
                  : length <= 14 ? 52.0
                                 : 40.0;

  // Modifiers are percentages of the band, multiplied rather than added, so a
  // long name cannot stack small bonuses into a good score and a four-letter
  // name is not punished twice for the same digit.
  auto apply = [&appeal](double percent) { appeal *= 1.0 + percent / 100.0; };

  if (digitCount == 0) apply(10.0);
  else if (digitCount <= 2) apply(-5.0);
  else apply(-15.0);

  if (underscoreCount > 0) apply(-18.0);
  // Gated on length for the same reason the leet TELL is, so the breakdown
  // never contradicts the verdict.
  if (leetDigits > 0 && length >= 7) apply(-15.0);

  if (parts.size() == 1) apply(6.0);
  else if (parts.size() == 2) apply(6.0);
  else if (parts.size() >= 3) apply(-8.0);

  const bool allPronounceable =
      !parts.empty() && std::all_of(parts.begin(), parts.end(), pronounceable);
  apply(allPronounceable ? 5.0 : -10.0);

  // A recognisable word or given name is what makes a short handle feel owned
  // rather than random. It is a REWARD and nothing else -- it is deliberately
  // not evidence of being generated. Treating "this word is in the corpus" as
  // a penalty is what made the previous engine reject "maxfly" and "DeadlyAir".
  if (parts.size() <= 2) {
    const bool recognised = std::any_of(parts.begin(), parts.end(), [](const std::string &word) {
      return has(poolWords(), word) || has(adjectives(), word) || has(seedWords(), word) ||
             has(firstNames(), word) || has(surnames(), word) || has(seedNames(), word);
    });
    if (recognised) apply(5.0);
  }

  result.appeal = std::max(0, std::min(100, static_cast<int>(std::lround(appeal))));
  result.score = std::max(0, std::min(result.appeal, 100 - result.tell));
  result.cappedByTell = result.tell > 0 && (100 - result.tell) < result.appeal;
  result.passes = result.score >= result.threshold;

  if (result.score >= 85) result.verdict = "Excellent";
  else if (result.score >= result.threshold) result.verdict = "Good";
  else if (result.score >= 45) result.verdict = "Borderline";
  else result.verdict = "Reject";

  if (tells.empty()) {
    result.pattern = "Nothing that says nick";
    result.summary = "Nothing here says nick.";
  } else {
    result.pattern = patternLabel(tells.front().id);
    result.summary = "Reads as a nick: " + tells.front().detail + ".";
  }
  return result;
}

} // namespace OVson::NickRoll

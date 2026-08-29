#include "NickRollCore.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace OVson::NickRoll {
namespace {

// --- a small JSON reader ------------------------------------------------
// The codebase has no JSON library, only per-key string scanning, and that is
// not enough here: the same key ("text", "clickEvent") appears at several
// depths and only the nesting says which button a command belongs to. This
// walks the document properly instead, and stops at the first malformed byte
// rather than salvaging what it can.

struct Reader {
  const std::string &src;
  std::size_t at = 0;
  bool ok = true;

  explicit Reader(const std::string &text) : src(text) {}

  void skipSpace() {
    while (at < src.size() && std::isspace(static_cast<unsigned char>(src[at])))
      ++at;
  }
  bool eof() const { return at >= src.size(); }
  char peek() const { return at < src.size() ? src[at] : '\0'; }
  bool consume(char expected) {
    skipSpace();
    if (peek() != expected)
      return false;
    ++at;
    return true;
  }
  void fail() { ok = false; }
};

void skipValue(Reader &r);

std::string readString(Reader &r) {
  std::string out;
  r.skipSpace();
  if (!r.consume('"')) {
    r.fail();
    return out;
  }
  while (r.ok && !r.eof()) {
    const char c = r.src[r.at++];
    if (c == '"')
      return out;
    if (c != '\\') {
      out.push_back(c);
      continue;
    }
    if (r.eof()) {
      r.fail();
      return out;
    }
    const char esc = r.src[r.at++];
    switch (esc) {
    case 'n': out.push_back('\n'); break;
    case 't': out.push_back('\t'); break;
    case 'r': out.push_back('\r'); break;
    case 'b': out.push_back('\b'); break;
    case 'f': out.push_back('\f'); break;
    case '"': out.push_back('"'); break;
    case '\\': out.push_back('\\'); break;
    case '/': out.push_back('/'); break;
    case 'u': {
      // Only the BMP range that matters here; anything else becomes '?' rather
      // than corrupting the byte stream.
      if (r.at + 4 > r.src.size()) { r.fail(); return out; }
      unsigned code = 0;
      for (int i = 0; i < 4; ++i) {
        const char h = r.src[r.at + static_cast<std::size_t>(i)];
        code <<= 4;
        if (h >= '0' && h <= '9') code |= static_cast<unsigned>(h - '0');
        else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned>(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned>(h - 'A' + 10);
        else { r.fail(); return out; }
      }
      r.at += 4;
      if (code < 0x80) {
        out.push_back(static_cast<char>(code));
      } else if (code < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (code >> 6)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
      } else {
        out.push_back(static_cast<char>(0xE0 | (code >> 12)));
        out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
      }
      break;
    }
    default:
      r.fail();
      return out;
    }
  }
  r.fail();
  return out;
}

void skipLiteral(Reader &r) {
  while (!r.eof()) {
    const char c = r.peek();
    const bool part = (c >= '0' && c <= '9') || c == '-' || c == '+' ||
                      c == '.' || c == 'e' || c == 'E' ||
                      (c >= 'a' && c <= 'z');
    if (!part)
      return;
    ++r.at;
  }
}

void skipValue(Reader &r) {
  r.skipSpace();
  if (r.eof()) { r.fail(); return; }
  const char c = r.peek();
  if (c == '"') { readString(r); return; }
  if (c == '{') {
    ++r.at;
    r.skipSpace();
    if (r.consume('}')) return;
    while (r.ok) {
      readString(r);
      if (!r.consume(':')) { r.fail(); return; }
      skipValue(r);
      if (r.consume(',')) continue;
      if (r.consume('}')) return;
      r.fail();
      return;
    }
    return;
  }
  if (c == '[') {
    ++r.at;
    r.skipSpace();
    if (r.consume(']')) return;
    while (r.ok) {
      skipValue(r);
      if (r.consume(',')) continue;
      if (r.consume(']')) return;
      r.fail();
      return;
    }
    return;
  }
  skipLiteral(r);
}

struct Click {
  std::string action;
  std::string value;
};

void readComponent(Reader &r, const Click &inherited,
                   std::vector<PageSegment> &out);

void readExtra(Reader &r, const Click &inherited,
               std::vector<PageSegment> &out) {
  if (!r.consume('[')) { r.fail(); return; }
  r.skipSpace();
  if (r.consume(']')) return;
  while (r.ok) {
    readComponent(r, inherited, out);
    if (r.consume(',')) continue;
    if (r.consume(']')) return;
    r.fail();
    return;
  }
}

void readClickEvent(Reader &r, Click &click) {
  if (!r.consume('{')) { r.fail(); return; }
  r.skipSpace();
  if (r.consume('}')) return;
  while (r.ok) {
    const std::string key = readString(r);
    if (!r.consume(':')) { r.fail(); return; }
    if (key == "action") click.action = readString(r);
    else if (key == "value") click.value = readString(r);
    else skipValue(r);
    if (r.consume(',')) continue;
    if (r.consume('}')) return;
    r.fail();
    return;
  }
}

void readComponent(Reader &r, const Click &inherited,
                   std::vector<PageSegment> &out) {
  r.skipSpace();
  // A bare string is a legal component and carries the parent's click.
  if (r.peek() == '"') {
    PageSegment segment;
    segment.text = readString(r);
    segment.clickAction = inherited.action;
    segment.clickValue = inherited.value;
    out.push_back(segment);
    return;
  }
  if (r.peek() == '[') {
    readExtra(r, inherited, out);
    return;
  }
  if (!r.consume('{')) { r.fail(); return; }

  // "extra" can precede "text" and "clickEvent" in the document, so the whole
  // object is read before anything is emitted; otherwise a child would inherit
  // a click its parent had not been seen to declare yet.
  std::string text;
  bool hasText = false;
  Click own = inherited;
  bool pendingExtra = false;
  std::size_t extraStart = 0;

  r.skipSpace();
  if (!r.consume('}')) {
    while (r.ok) {
      const std::string key = readString(r);
      if (!r.consume(':')) { r.fail(); return; }
      if (key == "text") { text = readString(r); hasText = true; }
      else if (key == "clickEvent") { readClickEvent(r, own); }
      else if (key == "extra") { pendingExtra = true; extraStart = r.at; skipValue(r); }
      else skipValue(r);
      if (r.consume(',')) continue;
      if (r.consume('}')) break;
      r.fail();
      return;
    }
  }
  if (!r.ok) return;

  if (hasText) {
    PageSegment segment;
    segment.text = text;
    segment.clickAction = own.action;
    segment.clickValue = own.value;
    out.push_back(segment);
  }
  if (pendingExtra) {
    Reader sub(r.src);
    sub.at = extraStart;
    readExtra(sub, own, out);
    if (!sub.ok) r.fail();
  }
}

std::string toLower(const std::string &text) {
  std::string out = text;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return out;
}

// The sentence the page is recognised by. Deliberately shorter than the whole
// line: it leaves out the apostrophe in "We've", which is the one character
// most likely to arrive escaped, curled, or re-worded.
constexpr const char *kAnchor = "generated a random username";

constexpr const char *kButtonLabels[] = {"use name", "try again"};

} // namespace

std::string stripFormattingCodes(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    // The section sign is 0xC2 0xA7 in UTF-8; skip it and the code after it.
    const bool section = static_cast<unsigned char>(text[i]) == 0xC2 &&
                         i + 1 < text.size() &&
                         static_cast<unsigned char>(text[i + 1]) == 0xA7;
    if (section) {
      i += 2;
      continue;
    }
    out.push_back(text[i]);
  }
  return out;
}

std::string trim(const std::string &text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;
  return text.substr(begin, end - begin);
}

BookPage parsePageJson(const std::string &json) {
  BookPage page;
  if (trim(json).empty())
    return page;
  Reader reader(json);
  Click none;
  readComponent(reader, none, page.segments);
  if (!reader.ok) {
    page.segments.clear();
    return page;
  }
  reader.skipSpace();
  if (!reader.eof()) {
    page.segments.clear();
    return page;
  }
  page.parsed = true;
  for (auto &segment : page.segments) {
    segment.text = stripFormattingCodes(segment.text);
    page.plainText += segment.text;
  }
  return page;
}

bool isValidUsername(const std::string &value) {
  if (value.size() < 3 || value.size() > 16)
    return false;
  for (const char c : value) {
    const bool allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                         (c >= '0' && c <= '9') || c == '_';
    if (!allowed)
      return false;
  }
  return true;
}

bool nicknameMatchesTargetWord(const std::string &nickname,
                               const std::string &targetWord) {
  if (targetWord.empty())
    return false;
  return toLower(nickname).find(toLower(targetWord)) != std::string::npos;
}

bool shouldStopReroll(bool scorePasses, const std::string &nickname,
                      const std::string &targetWord) {
  return targetWord.empty()
             ? scorePasses
             : nicknameMatchesTargetWord(nickname, targetWord);
}

bool isGeneratedNamePage(const BookPage &page) {
  return page.parsed &&
         toLower(page.plainText).find(kAnchor) != std::string::npos;
}

std::string findGeneratedName(const BookPage &page) {
  if (!isGeneratedNamePage(page))
    return {};

  // Walk forward from the segment carrying the sentence and take the first
  // plausible username. The scan stops at the first clickable segment: the
  // buttons are clickable and the name never is, so this cannot mistake
  // "USE NAME" for the answer even though "USE" is a legal username.
  bool seenAnchor = false;
  for (const auto &segment : page.segments) {
    std::size_t scanFrom = 0;
    if (!seenAnchor) {
      const std::size_t at = toLower(segment.text).find(kAnchor);
      if (at == std::string::npos)
        continue;
      seenAnchor = true;
      // The name lives on the line *after* the sentence, never inside it. That
      // distinction matters: the sentence contains "generated", "random" and
      // "username", every one of which is a legal Minecraft name and would be
      // returned by a scan that started at the beginning of the segment.
      const std::size_t lineEnd = segment.text.find('\n', at);
      if (lineEnd == std::string::npos)
        continue;
      scanFrom = lineEnd + 1;
    }
    if (!segment.clickValue.empty())
      break;

    std::string token;
    for (std::size_t i = scanFrom; i <= segment.text.size(); ++i) {
      const bool boundary =
          i == segment.text.size() ||
          std::isspace(static_cast<unsigned char>(segment.text[i]));
      if (!boundary) {
        token.push_back(segment.text[i]);
        continue;
      }
      if (!token.empty()) {
        const std::string lowerToken = toLower(token);
        bool isLabelWord = false;
        for (const char *label : kButtonLabels) {
          const std::string labelText(label);
          if (labelText.find(lowerToken) != std::string::npos)
            isLabelWord = true;
        }
        if (!isLabelWord && isValidUsername(token))
          return token;
      }
      token.clear();
    }
  }
  return {};
}

std::string findButtonCommand(const BookPage &page, const std::string &label) {
  const std::string wanted = toLower(trim(label));
  if (wanted.empty())
    return {};
  for (const auto &segment : page.segments) {
    if (segment.clickValue.empty())
      continue;
    // Only run_command is acted on. A page that offers change_page or
    // suggest_command under the same label is a page we do not understand, and
    // the right response to not understanding is to do nothing.
    if (segment.clickAction != "run_command")
      continue;
    if (toLower(stripFormattingCodes(segment.text)).find(wanted) == std::string::npos)
      continue;
    return segment.clickValue;
  }
  return {};
}

} // namespace OVson::NickRoll

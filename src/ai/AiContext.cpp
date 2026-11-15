#include "AiContext.h"

#include <vector>

#include "AiProtocol.h"

namespace ai {

namespace {

bool isSpace(const char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

// Collapse whitespace runs to single spaces and trim; `map[i]` is the byte
// offset in `in` of output byte i.
std::string normalize(std::string_view in, std::vector<size_t>* map) {
  std::string out;
  out.reserve(in.size());
  if (map) map->reserve(in.size());
  bool pendingSpace = false;
  for (size_t i = 0; i < in.size(); i++) {
    if (isSpace(in[i])) {
      pendingSpace = !out.empty();
      continue;
    }
    if (pendingSpace) {
      out += ' ';
      if (map) map->push_back(i);
      pendingSpace = false;
    }
    out += in[i];
    if (map) map->push_back(i);
  }
  return out;
}

size_t countChars(std::string_view s) {
  size_t n = 0;
  for (const unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}

// Start of the last `maxChars` code points of `s`.
size_t lastCharsStart(std::string_view s, size_t maxChars) {
  size_t pos = s.size();
  size_t chars = 0;
  while (pos > 0 && chars < maxChars) {
    pos--;
    if ((static_cast<unsigned char>(s[pos]) & 0xC0) != 0x80) chars++;
  }
  // Land on a character start even if a broken sequence was at the front.
  while (pos < s.size() && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) pos++;
  return pos;
}

}  // namespace

std::string contextAround(std::string_view pageText, std::string_view selection, const size_t maxChars) {
  if (pageText.empty() || maxChars == 0) return std::string();
  if (countChars(pageText) <= maxChars) return std::string(pageText);

  std::vector<size_t> map;
  const std::string page = normalize(pageText, &map);
  const std::string sel = normalize(selection, nullptr);
  size_t selStart = sel.empty() ? std::string::npos : page.find(sel);
  if (selStart == std::string::npos && !sel.empty()) {
    // Try the first word only (selections can span a paragraph break the page text lacks).
    const size_t firstSpace = sel.find(' ');
    selStart = page.find(sel.substr(0, firstSpace));
  }
  if (selStart == std::string::npos) return std::string(truncateUtf8(pageText, maxChars));

  const size_t selEnd = std::min(page.size(), selStart + sel.size());
  const std::string_view pageView(page);
  // Up to a fifth of the budget after the selection; the rest before it.
  const size_t selChars = countChars(pageView.substr(selStart, selEnd - selStart));
  const size_t afterBudget = selChars >= maxChars ? 0 : (maxChars - selChars) / 5;
  const std::string_view afterView = truncateUtf8(pageView.substr(selEnd), afterBudget);
  const size_t usedAfter = countChars(afterView);
  const size_t beforeBudget = maxChars > selChars + usedAfter ? maxChars - selChars - usedAfter : 0;
  const size_t windowStart = lastCharsStart(pageView.substr(0, selStart), beforeBudget);
  const size_t windowEnd = selEnd + afterView.size();

  // Map back to the original text so its line breaks are kept.
  const size_t origStart = map[windowStart];
  const size_t origEnd = windowEnd >= map.size() ? pageText.size() : map[windowEnd];
  const std::string_view window = pageText.substr(origStart, origEnd - origStart);
  return std::string(truncateUtf8(window, maxChars));
}

}  // namespace ai

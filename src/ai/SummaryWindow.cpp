#include "SummaryWindow.h"

#include <deque>
#include <vector>

#include "AiProtocol.h"

namespace ai {
namespace {

constexpr size_t MAX_CHAPTER_CHARS = 200;

bool isSpace(const char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

void appendWords(const std::string& text, std::vector<std::string>& out) {
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && isSpace(text[i])) i++;
    const size_t start = i;
    while (i < text.size() && !isSpace(text[i])) i++;
    if (i > start) out.emplace_back(text, start, i - start);
  }
}

size_t countChars(const std::string& s) {
  size_t n = 0;
  for (const unsigned char c : s) n += (c & 0xC0) != 0x80;
  return n;
}

// Joins words[from, to) with single spaces.
std::string join(const std::vector<std::string>& w, size_t from, size_t to) {
  std::string out;
  for (size_t i = from; i < to; i++) {
    if (!out.empty()) out += ' ';
    out += w[i];
  }
  return out;
}

// The last `words` words of `w`, cut from the far end to `maxChars` code points.
std::string tail(const std::vector<std::string>& w, size_t words, size_t maxChars) {
  size_t from = w.size() > words ? w.size() - words : 0;
  size_t chars = 0;
  size_t keep = w.size();
  while (keep > from) {
    const size_t add = countChars(w[keep - 1]) + (keep < w.size() ? 1 : 0);
    if (chars + add > maxChars) break;
    chars += add;
    keep--;
  }
  return join(w, keep, w.size());
}

// The first `words` words of `w`, cut from the far end to `maxChars` code points.
std::string head(const std::vector<std::string>& w, size_t words, size_t maxChars) {
  const size_t to = w.size() < words ? w.size() : words;
  size_t chars = 0;
  size_t keep = 0;
  while (keep < to) {
    const size_t add = countChars(w[keep]) + (keep > 0 ? 1 : 0);
    if (chars + add > maxChars) break;
    chars += add;
    keep++;
  }
  return join(w, 0, keep);
}

}  // namespace

SummaryWindow summaryWindow(const int currentPage, const int pageCount, const PageTextFn& pageText, const size_t words,
                            const size_t maxChars, const int maxPagesPerSide) {
  SummaryWindow result;
  if (pageCount <= 0 || currentPage < 0 || currentPage >= pageCount || words == 0) return result;

  std::string text;
  if (!pageText(currentPage, text)) return result;

  // Before: the current page, then earlier pages (nearest first) until enough words.
  std::deque<std::vector<std::string>> earlier;  // pages in reverse reading order
  std::vector<std::string> page;
  appendWords(text, page);
  size_t have = page.size();
  earlier.push_front(std::move(page));
  for (int p = currentPage - 1; p >= 0 && have < words && currentPage - p <= maxPagesPerSide; p--) {
    if (!pageText(p, text)) break;
    page.clear();
    appendWords(text, page);
    have += page.size();
    earlier.push_front(std::move(page));
  }
  std::vector<std::string> before;
  for (auto& pg : earlier) before.insert(before.end(), pg.begin(), pg.end());
  result.before = tail(before, words, maxChars);

  // After: following pages until enough words.
  std::vector<std::string> after;
  for (int p = currentPage + 1; p < pageCount && after.size() < words && p - currentPage <= maxPagesPerSide; p++) {
    if (!pageText(p, text)) break;
    appendWords(text, after);
  }
  result.after = head(after, words, maxChars);
  return result;
}

std::string chapterLabel(const std::string& tocTitle, const int spineIndex) {
  size_t b = 0, e = tocTitle.size();
  while (b < e && isSpace(tocTitle[b])) b++;
  while (e > b && isSpace(tocTitle[e - 1])) e--;
  std::string label;
  if (e > b) {
    label = tocTitle.substr(b, e - b);
  } else if (spineIndex >= 0) {
    label = "Chapter " + std::to_string(spineIndex + 1);
  }
  return std::string(truncateUtf8(label, MAX_CHAPTER_CHARS));
}

}  // namespace ai

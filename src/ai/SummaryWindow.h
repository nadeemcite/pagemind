#pragma once

#include <cstddef>
#include <functional>
#include <string>

// Reading window for "Summarize so far": the words before and after the reader's
// position, inside the current chapter. Pure logic, so it runs in host tests.
namespace ai {

// Fills `out` with the logical text of page `page` of the chapter; returns false
// when the page cannot be loaded (past the built pages, or a read error).
using PageTextFn = std::function<bool(int page, std::string& out)>;

struct SummaryWindow {
  std::string before;  // last words up to the end of the current page (the reading position)
  std::string after;   // first words after it; only for locating the scene
};

// Walks pages backwards and forwards from `currentPage` (0-based, within a chapter of
// `pageCount` pages) until `words` words are gathered or the chapter ends, so it loads
// only the pages it needs. Words are whitespace-separated and joined with single spaces.
// Each side is then cut to `maxChars` Unicode code points on a word boundary, keeping
// the words nearest the position. A page that cannot be loaded ends that side's walk, and so does reading
// `maxPagesPerSide` pages (poetry or captions with a few words per page must not cause dozens of SD reads).
SummaryWindow summaryWindow(int currentPage, int pageCount, const PageTextFn& pageText, size_t words = 200,
                            size_t maxChars = 3000, int maxPagesPerSide = 12);

// Chapter name for the request: the TOC title if there is one, else "Chapter N"
// (N = spineIndex + 1), else empty. At most 200 characters.
std::string chapterLabel(const std::string& tocTitle, int spineIndex);

}  // namespace ai

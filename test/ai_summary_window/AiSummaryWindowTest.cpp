// Summarize-so-far window extractor: planning/summarize-so-far.md, Phase 3 gates W1-W8.
#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

#include "ai/AiProtocol.h"
#include "ai/SummaryWindow.h"

namespace {

// A chapter of `pages` pages with `wordsPerPage` words each, words numbered w0, w1, ... in reading order.
struct Chapter {
  std::vector<std::string> pages;
  std::vector<int> asked;  // every page index the extractor requested

  Chapter(int pageCount, int wordsPerPage) {
    int n = 0;
    for (int p = 0; p < pageCount; p++) {
      std::string text;
      for (int i = 0; i < wordsPerPage; i++) text += (i ? " w" : "w") + std::to_string(n++);
      pages.push_back(text);
    }
  }
  explicit Chapter(std::vector<std::string> p) : pages(std::move(p)) {}

  ai::PageTextFn source() {
    return [this](int page, std::string& out) {
      asked.push_back(page);
      if (page < 0 || page >= static_cast<int>(pages.size())) return false;
      out = pages[page];
      return true;
    };
  }
  int count() const { return static_cast<int>(pages.size()); }
};

std::vector<std::string> words(const std::string& s) {
  std::vector<std::string> out;
  std::istringstream in(s);
  for (std::string w; in >> w;) out.push_back(w);
  return out;
}

}  // namespace

// W1: mid-chapter: exactly 200 words each side, boundary exact.
TEST(SummaryWindow, MidChapterIsExactAndContiguous) {
  Chapter c(10, 100);  // w0..w999; page 5 ends at w599
  const auto w = ai::summaryWindow(5, c.count(), c.source());
  const auto before = words(w.before), after = words(w.after);
  ASSERT_EQ(before.size(), 200u);
  ASSERT_EQ(after.size(), 200u);
  EXPECT_EQ(before.front(), "w400");
  EXPECT_EQ(before.back(), "w599");  // the reader has read to the end of the current page
  EXPECT_EQ(after.front(), "w600");
  EXPECT_EQ(after.back(), "w799");
}

// W1b: the extractor reads only the pages it needs (one page = one SD read on the device).
TEST(SummaryWindow, ReadsOnlyNeededPages) {
  Chapter c(50, 100);
  ai::summaryWindow(25, c.count(), c.source());
  EXPECT_LE(c.asked.size(), 6u);  // current + 1 before + 2 after + slack
}

// W2: chapter start and end.
TEST(SummaryWindow, ChapterStartHasShortBeforeAndFullAfter) {
  Chapter c(10, 50);
  const auto w = ai::summaryWindow(0, c.count(), c.source());
  EXPECT_EQ(words(w.before).size(), 50u);  // only the first page exists
  EXPECT_EQ(words(w.before).back(), "w49");
  EXPECT_EQ(words(w.after).size(), 200u);
  EXPECT_EQ(words(w.after).front(), "w50");
}

TEST(SummaryWindow, ChapterEndHasFullBeforeAndEmptyAfter) {
  Chapter c(10, 50);
  const auto w = ai::summaryWindow(9, c.count(), c.source());
  EXPECT_EQ(words(w.before).size(), 200u);
  EXPECT_EQ(words(w.before).back(), "w499");
  EXPECT_TRUE(w.after.empty());
}

// W3: chapter shorter than 400 words: all words used once, no overlap.
TEST(SummaryWindow, ShortChapterHasNoOverlap) {
  Chapter c(3, 50);  // 150 words
  const auto w = ai::summaryWindow(1, c.count(), c.source());
  const auto before = words(w.before), after = words(w.after);
  EXPECT_EQ(before.size(), 100u);
  EXPECT_EQ(after.size(), 50u);
  EXPECT_EQ(before.back(), "w99");
  EXPECT_EQ(after.front(), "w100");
}

TEST(SummaryWindow, SinglePageChapterBeforeOnly) {
  Chapter c(1, 80);
  const auto w = ai::summaryWindow(0, 1, c.source());
  EXPECT_EQ(words(w.before).size(), 80u);
  EXPECT_TRUE(w.after.empty());
}

// W4: Hindi and mixed scripts: split only on whitespace, so no word or combining mark is broken.
TEST(SummaryWindow, HindiWordsAreNeverSplit) {
  std::vector<std::string> pages;
  for (int p = 0; p < 6; p++) {
    std::string t;
    for (int i = 0; i < 100; i++) t += "मित्रता-" + std::to_string(p * 100 + i) + "ें ";
    pages.push_back(t);
  }
  Chapter c(pages);
  // Large character limit: this test is about word splitting; the limit is tested in W6.
  const auto w = ai::summaryWindow(2, c.count(), c.source(), 200, 100000);
  EXPECT_EQ(words(w.before).size(), 200u);
  EXPECT_EQ(words(w.after).size(), 200u);
  for (const auto& word : words(w.before)) {
    EXPECT_EQ(word.rfind("मित्रता-", 0), 0u) << word;      // starts with the whole prefix
    EXPECT_EQ(word.substr(word.size() - 6), "ें") << word;  // ends with the whole 3-byte mark
  }
}

// W8b: pages with very few words must not cause many page reads.
TEST(SummaryWindow, ReadsAtMostTwelvePagesPerSide) {
  Chapter c(200, 2);  // 2 words per page: 200 words would need 100 pages
  const auto w = ai::summaryWindow(100, c.count(), c.source());
  EXPECT_LE(c.asked.size(), 1u + 12u + 12u);
  EXPECT_EQ(words(w.before).size(), 26u);  // current page + 12 earlier pages, 2 words each
  EXPECT_EQ(words(w.after).size(), 24u);
}

// W5: never crosses a chapter boundary: the extractor only asks for pages 0..pageCount-1.
TEST(SummaryWindow, NeverAsksOutsideTheChapter) {
  Chapter c(3, 10);
  ai::summaryWindow(0, c.count(), c.source());
  ai::summaryWindow(2, c.count(), c.source());
  for (int p : c.asked) {
    EXPECT_GE(p, 0);
    EXPECT_LT(p, c.count());
  }
}

TEST(SummaryWindow, MissingPageEndsTheWindow) {
  // A page that cannot be loaded (partial build, SD error) stops the walk instead of skipping it.
  Chapter c(10, 100);
  int calls = 0;
  const auto w = ai::summaryWindow(5, c.count(), [&](int page, std::string& out) {
    calls++;
    if (page == 7) return false;
    out = c.pages[page];
    return true;
  });
  EXPECT_EQ(words(w.after).size(), 100u);  // page 6 (100 words); page 7 unavailable
  EXPECT_GT(calls, 0);
}

TEST(SummaryWindow, CurrentPageUnavailableGivesEmpty) {
  const auto w = ai::summaryWindow(3, 10, [](int, std::string&) { return false; });
  EXPECT_TRUE(w.before.empty());
  EXPECT_TRUE(w.after.empty());
}

TEST(SummaryWindow, InvalidArgumentsGiveEmpty) {
  Chapter c(3, 10);
  EXPECT_TRUE(ai::summaryWindow(-1, 3, c.source()).before.empty());
  EXPECT_TRUE(ai::summaryWindow(3, 3, c.source()).before.empty());
  EXPECT_TRUE(ai::summaryWindow(0, 0, c.source()).before.empty());
}

// W6: server limit (3000 characters per window): whole words only, nearest the position kept.
TEST(SummaryWindow, LongWordsAreClippedOnWordBoundariesNearestThePosition) {
  std::vector<std::string> pages;
  for (int p = 0; p < 6; p++) {
    std::string t;
    for (int i = 0; i < 100; i++) t += "w" + std::to_string(p * 100 + i) + std::string(20, 'x') + " ";
    pages.push_back(t);
  }
  Chapter c(pages);  // each word is about 25 characters
  const auto w = ai::summaryWindow(2, c.count(), c.source(), 200, 3000);
  EXPECT_LE(w.before.size(), 3000u);
  EXPECT_LE(w.after.size(), 3000u);
  const auto before = words(w.before), after = words(w.after);
  ASSERT_FALSE(before.empty());
  ASSERT_FALSE(after.empty());
  EXPECT_EQ(before.back().substr(0, 5), "w299x");                                  // last word of the current page kept
  EXPECT_EQ(after.front().substr(0, 5), "w300x");                                  // first unread word kept
  for (const auto& word : before) EXPECT_EQ(word.substr(word.size() - 3), "xxx");  // none cut in half
  for (const auto& word : after) EXPECT_EQ(word.substr(word.size() - 3), "xxx");
}

TEST(SummaryWindow, LimitCountsCodePointsNotBytes) {
  // Two-byte letters: a 100-character limit must keep about 100 characters, not 100 bytes.
  std::string page;
  for (int i = 0; i < 300; i++) page += "éééééééé ";  // 8 chars + space
  Chapter c({page, page});
  const auto w = ai::summaryWindow(0, 2, c.source(), 200, 100);
  size_t chars = 0;
  for (unsigned char ch : w.before) chars += (ch & 0xC0) != 0x80;
  EXPECT_LE(chars, 100u);
  EXPECT_GT(chars, 80u);  // not clipped far below the limit
}

TEST(SummaryWindow, WhitespaceIsCollapsed) {
  Chapter c({"a  b\n\nc\t d", "e f"});
  const auto w = ai::summaryWindow(0, 2, c.source());
  EXPECT_EQ(w.before, "a b c d");
  EXPECT_EQ(w.after, "e f");
}

// W7: chapter name.
TEST(ChapterLabel, PrefersTocTitle) { EXPECT_EQ(ai::chapterLabel("  The Mail \n", 4), "The Mail"); }
TEST(ChapterLabel, FallsBackToChapterNumber) {
  EXPECT_EQ(ai::chapterLabel("", 4), "Chapter 5");
  EXPECT_EQ(ai::chapterLabel("   ", 0), "Chapter 1");
}
TEST(ChapterLabel, EmptyWhenNothingKnown) { EXPECT_EQ(ai::chapterLabel("", -1), ""); }
TEST(ChapterLabel, LongTitleIsClippedToTheServerLimit) {
  const std::string title(300, 'a');
  EXPECT_EQ(ai::chapterLabel(title, 1).size(), 200u);
  std::string hindi;
  for (int i = 0; i < 250; i++) hindi += "अ";
  const auto label = ai::chapterLabel(hindi, 1);
  EXPECT_EQ(label.size(), 200u * 3);  // 200 characters, never half a character
}

# Spike: reading chapter text around the current page (W8)

Question: how does the device get 200 words before and after the current page without
parsing the whole chapter into RAM?

## Findings (code reading, 2026-11-22)

- `Section::loadPage(int page)` (`lib/Epub/Epub/Section.cpp:920`) returns one `Page` for any page index.
  It has no side effects on `currentPage`, and returns `nullptr` for a negative page or a page that is not
  built or on disk yet (partial build). This fits the extractor's "page cannot be loaded" rule.
- `Section::getTextFromSectionFile()` (`Section.cpp:936`) already turns the *current* page into plain text
  (`TextBlock::wordText(i)`, the logical text, not the shaped display form). The Phase 4 adapter does the
  same for an arbitrary page index and passes it to `ai::summaryWindow` as the `PageTextFn`.
- One page is about 70 words at 18 pt (measured in Phase 5 D1: 71 words on a 16-line page), so 200 words is
  about 3 pages each side: roughly 7 `loadPage` calls, one `Page` alive at a time, one short string per page.
- Chapter name: `Epub::getTocIndexForSpineIndex(spineIndex)` (`lib/Epub/Epub.cpp:1247`) then
  `Epub::getTocItem(tocIndex).title`; `-1` means no TOC entry, and `ai::chapterLabel` falls back to `Chapter N`.

## Decisions

- Pages are read on demand through a callback. The extractor never holds more than the pages it still needs
  as word vectors (about 400 short strings at most), so it needs no chapter-wide buffer.
- Cap of 12 pages per side (test `ReadsAtMostTwelvePagesPerSide`) bounds SD reads for poetry or caption pages.
- Words are whitespace-separated. Chinese and Japanese (no spaces) are out of scope (plan decision).
- The reading position is the end of the current page.

## Not measured yet (needs the device)

Extra peak heap and wall time of the walk: recorded in Phase 5 (D8, memory and power) with the serial log.
Expected: under 10 KB transient and under 100 ms for 7 page loads; to be verified, not assumed.

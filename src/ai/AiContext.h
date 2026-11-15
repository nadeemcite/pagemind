#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace ai {

// Text around `selection` inside `pageText`, at most `maxChars` code points,
// mostly from before the selection (what the reader has just read). Whitespace
// differences between the two are ignored when locating the selection; when it
// cannot be found, the start of the page is used. Never splits a UTF-8 character.
std::string contextAround(std::string_view pageText, std::string_view selection, size_t maxChars);

}  // namespace ai

#pragma once

// PageMind demo capture commands (scripts/demo_capture.py). Compiled only in the
// pagemind_x4pro_demo build (PAGEMIND_DEMO=1); release firmware has none of this.
#if PAGEMIND_DEMO

#include <string>

namespace demo {

// Handles one serial line after "CMD:" (e.g. "OPEN /Books/x.epub"). Returns false
// for commands it does not know. Replies are lines starting with "DEMO:".
bool handle(const std::string& command);

// Ask AI task preset by "CMD:ASK <task>" for the next AiAskActivity, or -1.
// Taking it clears it.
int takePresetTask();

// Word range preset by "CMD:SELECT <first> <last>" for the next text selection
// screen. False when none; taking it clears it.
bool takeSelectRange(int& first, int& last);

// Activities answer "WHERE ..." by setting the reply detail, e.g. "x y w h".
void setDetail(const std::string& detail);

}  // namespace demo

#endif

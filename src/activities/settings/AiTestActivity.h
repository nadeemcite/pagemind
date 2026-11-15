#pragma once

#include <I18n.h>

#include <string>

#include "activities/Activity.h"
#include "ai/AiClient.h"

// Settings → Network → AI Assistant → Test connection. Joins Wi-Fi if needed,
// calls /v1/info and one sample /v1/ask, and shows the result with timing and
// heap figures. Ask again repeats; Back leaves (Wi-Fi off, silent restart to Settings).
class AiTestActivity final : public Activity {
 public:
  explicit AiTestActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AiTest", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return true; }
  bool preventAutoSleep() override { return state == State::Working; }

 private:
  enum class State { Working, Done };
  State state = State::Working;
  bool infoDone = false;
  bool tearDownWifiOnExit = false;

  ai::Error result = ai::Error::None;
  ai::Info info;
  ai::Answer answer;
  ai::CallStats stats;
  int askCount = 0;
  // On-screen "Ask again" button (touch boards have no button hints); set by render().
  int askButtonX = 0, askButtonY = 0, askButtonW = 0, askButtonH = 0;

  void runTest();
  void onWifiSelectionComplete(bool connected);
};

// Translated message for an AI error.
StrId aiErrorMessage(ai::Error e);

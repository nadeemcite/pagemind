#pragma once

#include <string>

#include "activities/Activity.h"
#include "ai/AiClient.h"
#include "components/OptionPopup.h"

// Ask AI about a text selection: task picker, optional question, Wi-Fi, the
// request, then the answer in DictionaryDefinitionActivity. Errors offer Retry.
// If this flow turned Wi-Fi on, leaving it restarts into the book (as
// KOReaderSyncActivity does) to give back network heap.
class AiAskActivity final : public Activity {
 public:
  // `request.task` and `request.question` are filled in here.
  AiAskActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, ai::AskRequest request);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool skipLoopDelay() override { return true; }
  bool preventAutoSleep() override { return state == State::Asking; }

 private:
  enum class State { PickTask, Asking, Failed, Done };
  State state = State::PickTask;
  ai::AskRequest request;
  ai::Error error = ai::Error::None;
  bool wifiStartedHere = false;
  OptionPopup popup;

  void pickTask();
  void onTaskPicked(int index);
  void startAsking();
  void runAsk();
  void showAnswer(ai::Answer&& answer);
  void showError();
#if PAGEMIND_DEMO
 public:
  bool demoCommand(const char* command) override;
#endif
};

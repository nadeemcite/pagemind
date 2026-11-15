#include "AiAskActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdio>
#include <memory>
#include <utility>

#include "DictionaryDefinitionActivity.h"
#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/settings/AiTestActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "ai/AiSettingsStore.h"
#include "components/UITheme.h"
#include "demo/DemoCommands.h"
#include "fontIds.h"

namespace {
constexpr StrId TASK_NAMES[] = {StrId::STR_AI_TASK_EXPLAIN, StrId::STR_AI_TASK_DEFINE, StrId::STR_AI_TASK_TRANSLATE,
                                StrId::STR_AI_TASK_ASK};
constexpr ai::Task TASKS[] = {ai::Task::Explain, ai::Task::Define, ai::Task::Translate, ai::Task::Ask};
constexpr StrId ERROR_OPTIONS[] = {StrId::STR_AI_RETRY, StrId::STR_BACK};
}  // namespace

AiAskActivity::AiAskActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, ai::AskRequest request)
    : Activity("AiAsk", renderer, mappedInput), request(std::move(request)) {}

void AiAskActivity::onEnter() {
  Activity::onEnter();
#if PAGEMIND_DEMO
  const int preset = demo::takePresetTask();
  if (preset >= 0) {
    onTaskPicked(preset);
    return;
  }
#endif
  pickTask();
}

void AiAskActivity::onExit() {
  Activity::onExit();
  if (wifiStartedHere && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestartToReader();
  }
}

void AiAskActivity::pickTask() {
  state = State::PickTask;
  popup.show(StrId::STR_AI_ASK_AI, TASK_NAMES, 4, 0, [this](const int index) { onTaskPicked(index); });
  requestUpdate();
}

void AiAskActivity::onTaskPicked(const int index) {
  if (index < 0 || index >= 4) {
    finish();
    return;
  }
  request.task = TASKS[index];
  if (request.task != ai::Task::Ask) {
    startAsking();
    return;
  }
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_AI_QUESTION), "", 500, InputType::Text),
      [this](const ActivityResult& result) {
        if (result.isCancelled || std::get<KeyboardResult>(result.data).text.empty()) {
          pickTask();
          return;
        }
        request.question = std::get<KeyboardResult>(result.data).text;
        startAsking();
      });
}

void AiAskActivity::startAsking() {
  if (WiFi.status() == WL_CONNECTED) {
    state = State::Asking;
    requestUpdate();
    return;
  }
  wifiStartedHere = true;
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             finish();  // back to the book; onExit turns Wi-Fi off
                             return;
                           }
                           state = State::Asking;
                           requestUpdate();
                         });
}

void AiAskActivity::runAsk() {
  ai::Client client;
  ai::Info info;
  error = client.info(info);  // limits for trimming; also a key check
  ai::Answer answer;
  if (error == ai::Error::None) error = client.ask(request, info.limits, answer);
  if (error == ai::Error::None) {
    showAnswer(std::move(answer));
  } else {
    showError();
  }
}

void AiAskActivity::showAnswer(ai::Answer&& answer) {
  state = State::Done;
  std::string title = std::string(tr(STR_AI_ASK_AI)) + " · " + I18N.get(TASK_NAMES[static_cast<int>(request.task)]);
  std::string text = std::move(answer.text);
  if (answer.truncated) text += std::string("\n\n") + tr(STR_AI_ANSWER_TRUNCATED);
  auto viewer =
      makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, std::move(title), std::move(text));
  if (!viewer) {
    LOG_ERR("AI", "OOM: answer viewer");
    error = ai::Error::OutOfMemory;
    showError();
    return;
  }
  startActivityForResult(std::move(viewer), [](const ActivityResult&) { finish(); });
}

void AiAskActivity::showError() {
  state = State::Failed;
  popup.show(aiErrorMessage(error), ERROR_OPTIONS, 2, 0, [this](const int index) {
    if (index == 0) {
      state = State::Asking;
      requestUpdate();
    } else {
      finish();
    }
  });
  requestUpdate();
}

void AiAskActivity::loop() {
  if (state == State::Asking) {
    requestUpdateAndWait();  // paint "Asking AI..." before the blocking request
    runAsk();
    return;
  }
  if (popup.isActive()) {
    if (popup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
    if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
      popup.dismiss();
      finish();
    }
    return;
  }
  // The picker or error popup was closed without a choice (tap outside): back to the book.
  if (state == State::PickTask || state == State::Failed) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) finish();
}

void AiAskActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_AI_ASK_AI));
  if (state == State::Asking) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2, tr(STR_AI_ASKING));
  }
  if (popup.processRender(renderer, mappedInput)) return;
  renderer.displayBuffer();
}

#if PAGEMIND_DEMO
bool AiAskActivity::demoCommand(const char* command) {
  int index = -1;
  if (sscanf(command, "WHERE option %d", &index) != 1) return false;
  freeink::ui::Rect r;
  if (!popup.demoOptionRect(index, r)) return false;
  char buf[48];
  snprintf(buf, sizeof(buf), "%d %d %d %d", r.x, r.y, r.width, r.height);
  demo::setDetail(buf);
  return true;
}
#endif

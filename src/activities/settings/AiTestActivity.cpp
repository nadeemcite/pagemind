#include "AiTestActivity.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstdio>
#include <memory>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "ai/AiSettingsStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

StrId aiErrorMessage(const ai::Error e) {
  switch (e) {
    case ai::Error::NotConfigured:
      return StrId::STR_AI_ERR_NOT_CONFIGURED;
    case ai::Error::NoWifi:
      return StrId::STR_AI_ERR_NO_WIFI;
    case ai::Error::OutOfMemory:
      return StrId::STR_AI_ERR_MEMORY;
    case ai::Error::Connection:
      return StrId::STR_AI_ERR_CONNECTION;
    case ai::Error::BadKey:
      return StrId::STR_AI_ERR_BAD_KEY;
    case ai::Error::TextTooLong:
      return StrId::STR_AI_ERR_TOO_LONG;
    case ai::Error::RateLimited:
      return StrId::STR_AI_ERR_RATE_LIMITED;
    case ai::Error::ProviderFailed:
      return StrId::STR_AI_ERR_PROVIDER;
    case ai::Error::ServerUnavailable:
      return StrId::STR_AI_ERR_SERVER;
    case ai::Error::BadResponse:
    case ai::Error::ClientBug:
    case ai::Error::None:
    default:
      return StrId::STR_AI_ERR_BAD_RESPONSE;
  }
}

void AiTestActivity::onEnter() {
  Activity::onEnter();
  state = State::Working;
  if (!AI_SETTINGS.isConfigured()) {
    result = ai::Error::NotConfigured;
    state = State::Done;
    requestUpdate();
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    requestUpdate();
    return;
  }
  tearDownWifiOnExit = true;
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& r) { onWifiSelectionComplete(!r.isCancelled); });
}

void AiTestActivity::onExit() {
  Activity::onExit();
  // Same as ClockSyncActivity: give the network heap back with a silent restart.
  if (tearDownWifiOnExit && WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void AiTestActivity::onWifiSelectionComplete(const bool connected) {
  if (!connected) {
    finish();
    return;
  }
  state = State::Working;
  requestUpdate();
}

void AiTestActivity::runTest() {
  if (WiFi.status() != WL_CONNECTED) {
    result = ai::Error::NoWifi;
    state = State::Done;
    requestUpdate();
    return;
  }
  ai::Client client;
  if (!infoDone) {
    result = client.info(info, &stats);
    if (result != ai::Error::None) {
      state = State::Done;
      requestUpdate();
      return;
    }
    infoDone = true;
  }
  ai::AskRequest req;
  req.task = ai::Task::Define;
  req.text = "serendipity";
  req.language = AI_SETTINGS.getAnswerLanguage();
  result = client.ask(req, info.limits, answer, &stats);
  askCount++;
  LOG_INF("AI", "Test ask #%d: %s", askCount, ai::errorName(result));
  state = State::Done;
  requestUpdate();
}

void AiTestActivity::loop() {
  if (state == State::Working) {
    requestUpdateAndWait();  // paint "Contacting..." before the blocking request
    runTest();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  int tapX = 0;
  int tapY = 0;
  const bool tappedAsk = mappedInput.wasScreenTapped(tapX, tapY) && askButtonW > 0 && tapX >= askButtonX &&
                         tapX < askButtonX + askButtonW && tapY >= askButtonY && tapY < askButtonY + askButtonH;
  if ((mappedInput.wasPressed(MappedInputManager::Button::Confirm) || tappedAsk) && AI_SETTINGS.isConfigured()) {
    state = State::Working;
    requestUpdate();
  }
}

void AiTestActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_AI_TEST_CONNECTION));

  const int x = metrics.contentSidePadding;
  const int width = pageWidth - 2 * x;
  const int lineH = renderer.getLineHeight(UI_10_FONT_ID) + metrics.verticalSpacing;
  int y = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;

  if (state == State::Working) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageHeight / 2, tr(STR_AI_TESTING));
    renderer.displayBuffer();
    return;
  }

  const bool ok = result == ai::Error::None;
  renderer.drawText(UI_12_FONT_ID, x, y, ok ? tr(STR_AI_TEST_OK) : tr(STR_AI_TEST_FAILED), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + metrics.verticalSpacing * 2;

  char line[96];
  if (!ok) {
    for (const auto& l : renderer.wrappedText(UI_10_FONT_ID, I18N.get(aiErrorMessage(result)), width, 3)) {
      renderer.drawText(UI_10_FONT_ID, x, y, l.c_str());
      y += lineH;
    }
  } else {
    snprintf(line, sizeof(line), "%s / %s", info.provider.c_str(), answer.model.c_str());
    renderer.drawText(UI_10_FONT_ID, x, y, renderer.truncatedText(UI_10_FONT_ID, line, width).c_str());
    y += lineH * 3 / 2;
    for (const auto& l : renderer.wrappedText(UI_10_FONT_ID, answer.text.c_str(), width, 8)) {
      renderer.drawText(UI_10_FONT_ID, x, y, l.c_str());
      y += lineH;
    }
  }

  // Diagnostics (fixed English, like the About screen: support reads these).
  y += lineH / 2;
  snprintf(line, sizeof(line), "HTTP %d, %u ms, ask #%d", stats.status, static_cast<unsigned>(stats.ms), askCount);
  renderer.drawText(SMALL_FONT_ID, x, y, line);
  y += renderer.getLineHeight(SMALL_FONT_ID) + 2;
  snprintf(line, sizeof(line), "Min free heap %u B, min block %u B", static_cast<unsigned>(stats.minFreeHeap),
           static_cast<unsigned>(stats.minLargestBlock));
  renderer.drawText(SMALL_FONT_ID, x, y, line);

  askButtonW = 0;
  if (AI_SETTINGS.isConfigured()) {
    askButtonH = renderer.getLineHeight(UI_12_FONT_ID) + 2 * metrics.verticalSpacing + 8;
    askButtonW = width;
    askButtonX = x;
    askButtonY = pageHeight - metrics.buttonHintsHeight - askButtonH - metrics.verticalSpacing;
    renderer.drawRoundedRect(askButtonX, askButtonY, askButtonW, askButtonH, 2, 8, true);
    renderer.drawCenteredText(UI_12_FONT_ID, askButtonY + (askButtonH - renderer.getLineHeight(UI_12_FONT_ID)) / 2,
                              tr(STR_AI_ASK_AGAIN), true, EpdFontFamily::BOLD);
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), AI_SETTINGS.isConfigured() ? tr(STR_AI_ASK_AGAIN) : "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

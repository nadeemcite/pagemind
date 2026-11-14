#include "AiSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <memory>
#include <string>

#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "ai/AiSettingsStore.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr int IDX_ENABLED = 0;
constexpr int IDX_SERVER_URL = 1;
constexpr int IDX_API_KEY = 2;
constexpr int IDX_LANGUAGE = 3;

constexpr StrId menuNames[AiSettingsActivity::MENU_ITEMS] = {StrId::STR_AI_ASK_AI, StrId::STR_AI_SERVER_URL,
                                                             StrId::STR_AI_API_KEY, StrId::STR_AI_ANSWER_LANGUAGE};

void save() { AI_SETTINGS.saveToFile(); }
}  // namespace

AiSettingsActivity::AiSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("AiSettings", renderer, mappedInput) {
  for (int i = 0; i < MENU_ITEMS; i++) {
    rowItems_[i].label = I18N.get(menuNames[i]);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

const char* AiSettingsActivity::headerTitle() const { return tr(STR_AI_ASSISTANT); }

void AiSettingsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == IDX_ENABLED) {
    AI_SETTINGS.setEnabled(!AI_SETTINGS.isEnabled());
    save();
    requestUpdate();
  } else if (index == IDX_SERVER_URL) {
    const std::string& current = AI_SETTINGS.getServerUrl();
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_AI_SERVER_URL),
                                                current.empty() ? std::string("https://") : current,
                                                ai::MAX_SERVER_URL, InputType::Url),
        [](const ActivityResult& result) {
          if (result.isCancelled) return;
          const auto& kb = std::get<KeyboardResult>(result.data);
          AI_SETTINGS.setServerUrl((kb.text == "https://" || kb.text == "http://") ? std::string() : kb.text);
          save();
        });
  } else if (index == IDX_API_KEY) {
    // Never prefill the key: it is not shown again once saved.
    startActivityForResult(std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_AI_API_KEY), "",
                                                                   ai::MAX_API_KEY, InputType::Password),
                           [](const ActivityResult& result) {
                             if (result.isCancelled) return;
                             const auto& kb = std::get<KeyboardResult>(result.data);
                             if (kb.text.empty()) return;  // empty entry keeps the saved key
                             AI_SETTINGS.setApiKey(kb.text);
                             save();
                           });
  } else if (index == IDX_LANGUAGE) {
    startActivityForResult(
        std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_AI_ANSWER_LANGUAGE),
                                                AI_SETTINGS.getAnswerLanguage(), ai::MAX_LANGUAGE, InputType::Text),
        [](const ActivityResult& result) {
          if (result.isCancelled) return;
          AI_SETTINGS.setAnswerLanguage(std::get<KeyboardResult>(result.data).text);
          save();
        });
  }
}

void AiSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  rowValues_[IDX_ENABLED].clear();

  const std::string& url = AI_SETTINGS.getServerUrl();
  switch (ai::checkServerUrl(url)) {
    case ai::UrlCheck::Ok:
      rowValues_[IDX_SERVER_URL] = url.substr(url.find("://") + 3);
      break;
    case ai::UrlCheck::Insecure:
      rowValues_[IDX_SERVER_URL] = tr(STR_AI_URL_INSECURE);
      break;
    case ai::UrlCheck::Empty:
      rowValues_[IDX_SERVER_URL] = tr(STR_NOT_SET);
      break;
    default:
      rowValues_[IDX_SERVER_URL] = tr(STR_AI_URL_INVALID);
      break;
  }

  rowValues_[IDX_API_KEY] = AI_SETTINGS.hasApiKey()          ? tr(STR_AI_KEY_SAVED)
                            : AI_SETTINGS.apiKeyUnreadable() ? tr(STR_AI_KEY_REENTER)
                                                             : tr(STR_NOT_SET);
  rowValues_[IDX_LANGUAGE] =
      AI_SETTINGS.getAnswerLanguage().empty() ? std::string(tr(STR_AI_DEVICE_LANGUAGE)) : AI_SETTINGS.getAnswerLanguage();

  for (int i = 0; i < MENU_ITEMS; i++) {
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
  }
  GUI.setCheckboxRow(rowItems_[IDX_ENABLED], AI_SETTINGS.isEnabled());

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(MENU_ITEMS);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

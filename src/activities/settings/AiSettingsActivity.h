#pragma once

#include <string>

#include "activities/UiListActivity.h"

// Settings → Network → AI Assistant: on/off, server URL, API key and answer
// language for "Ask AI". Values live in AI_SETTINGS (src/ai/AiSettingsStore.h).
class AiSettingsActivity final : public UiListActivity {
 public:
  explicit AiSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  static constexpr int MENU_ITEMS = 5;

 private:
  int listCount() const override { return MENU_ITEMS; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  // Fixed row storage: buildScreen() only reassigns the value strings.
  std::string rowValues_[MENU_ITEMS];
  freeink::ui::ListItem rowItems_[MENU_ITEMS]{};
};

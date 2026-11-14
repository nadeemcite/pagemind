#include "AiSetupImport.h"

#include <HalStorage.h>
#include <Logging.h>

#include <string>

#include "AiSettingsStore.h"

namespace ai {

void importSetupFile() {
  if (!Storage.exists(SETUP_FILE_PATH)) return;
  std::string json;
  const bool read = Storage.readFileToString("AISET", SETUP_FILE_PATH, 2048, json);
  const bool applied = read && AI_SETTINGS.applySetupJson(json);
  json.assign(json.size(), '\0');  // do not leave the key in freed heap
  if (applied && AI_SETTINGS.saveToFile()) {
    LOG_INF("AISET", "Imported AI settings from %s", SETUP_FILE_PATH);
  } else {
    LOG_ERR("AISET", "Setup file %s is invalid; it is deleted, enter the settings again", SETUP_FILE_PATH);
  }
  // Deleted either way: it holds the API key in plain text.
  if (!Storage.remove(SETUP_FILE_PATH)) LOG_ERR("AISET", "Could not delete %s", SETUP_FILE_PATH);
}

}  // namespace ai

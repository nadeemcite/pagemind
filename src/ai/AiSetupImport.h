#pragma once

namespace ai {

// SD-card root file written by cloud/ai-proxy/deploy.sh.
constexpr char SETUP_FILE_PATH[] = "/pagemind-ai.json";

// If the setup file exists: apply it to AI_SETTINGS, save, and delete the file
// (it holds the API key in plain text). Call once at boot after loading settings.
void importSetupFile();

}  // namespace ai

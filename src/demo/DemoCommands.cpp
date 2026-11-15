#include "DemoCommands.h"

#if PAGEMIND_DEMO

#include <GfxRenderer.h>
#include <Logging.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "activities/RenderLock.h"
#include "ai/AiSettingsStore.h"

extern GfxRenderer renderer;
extern MappedInputManager mappedInputManager;

namespace demo {

namespace {

int presetTask = -1;
int selectFirst = -1;
int selectLast = -1;
std::string detail;

void reply(const char* status, const char* command, const char* detail = "") {
  logSerial.printf("DEMO:%s %s%s%s\n", status, command, detail[0] ? " " : "", detail);
}

int taskIndex(const char* name) {
  static const char* const NAMES[] = {"explain", "define", "translate", "ask"};
  for (int i = 0; i < 4; i++)
    if (strcmp(name, NAMES[i]) == 0) return i;
  return -1;
}

// Panel-orientation framebuffer, plus what the host needs to rotate it.
void sendScreenshot() {
  RenderLock lock;  // wait for an in-progress render to finish
  const uint8_t* fb = renderer.getFrameBuffer();
  const uint32_t size = static_cast<uint32_t>(renderer.getDisplayWidthBytes()) * renderer.getDisplayHeight();
  logSerial.printf("DEMO:SHOT %u %d %d %d %d\n", static_cast<unsigned>(size),
                   static_cast<int>(renderer.getOrientation()), renderer.getDisplayWidth(), renderer.getDisplayHeight(),
                   renderer.getDisplayWidthBytes());
  logSerial.write(fb, size);
  logSerial.printf("\nDEMO:SHOT_END\n");
  logSerial.flush();
}

}  // namespace

int takePresetTask() {
  const int t = presetTask;
  presetTask = -1;
  return t;
}

bool takeSelectRange(int& first, int& last) {
  if (selectFirst < 0) return false;
  first = selectFirst;
  last = selectLast;
  selectFirst = selectLast = -1;
  return true;
}

void setDetail(const std::string& d) { detail = d; }

bool handle(const std::string& command) {
  const size_t space = command.find(' ');
  const std::string verb = command.substr(0, space);
  const std::string arg = space == std::string::npos ? std::string() : command.substr(space + 1);

  if (verb == "STATE") {
    reply("OK", "STATE", activityManager.demoActivityName());
  } else if (verb == "SHOT") {
    sendScreenshot();
  } else if (verb == "OPEN") {
    if (arg.empty()) {
      reply("ERR", "OPEN", "missing path");
    } else {
      activityManager.goToReader(arg);
      reply("OK", "OPEN");
    }
  } else if (verb == "BACK" || verb == "CONFIRM" || verb == "NEXT" || verb == "PREV") {
    const RemoteButton b = verb == "BACK"      ? RemoteButton::Back
                           : verb == "CONFIRM" ? RemoteButton::Confirm
                           : verb == "NEXT"    ? RemoteButton::Next
                                               : RemoteButton::Previous;
    reply(mappedInputManager.injectRemoteClick(b) ? "OK" : "ERR", verb.c_str());
  } else if (verb == "WHERE") {
    // Screen rectangle of a control, for tap markers: "WHERE askai" or "WHERE option <i>".
    detail.clear();
    const bool ok = activityManager.demoCommand(command.c_str()) && !detail.empty();
    reply(ok ? "OK" : "ERR", "WHERE", detail.c_str());
  } else if (verb == "GOTO" || verb == "PAGE") {
    reply(activityManager.demoCommand(command.c_str()) ? "OK" : "ERR", verb.c_str());
  } else if (verb == "LANG") {
    // In memory only: the user's saved answer language is not changed.
    AI_SETTINGS.setAnswerLanguage(arg);
    reply("OK", "LANG");
  } else if (verb == "SELECT") {
    int first = -1, last = -1;
    if (sscanf(arg.c_str(), "%d %d", &first, &last) != 2 || first < 0 || last < first) {
      reply("ERR", "SELECT", "usage: SELECT <first> <last>");
    } else {
      selectFirst = first;
      selectLast = last;
      reply(activityManager.demoCommand("SELECT") ? "OK" : "ERR", "SELECT");
    }
  } else if (verb == "ASK" || verb == "PICKER") {
    presetTask = verb == "ASK" ? taskIndex(arg.c_str()) : -1;
    if (verb == "ASK" && presetTask < 0) {
      reply("ERR", "ASK", "task: explain|define|translate|ask");
    } else {
      reply(activityManager.demoCommand("ASK") ? "OK" : "ERR", verb.c_str());
    }
  } else {
    return false;
  }
  return true;
}

}  // namespace demo

#endif

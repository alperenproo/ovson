#pragma once
#include <string>

namespace Updater {
extern const char *kBuildVersion;

extern const char *kRepoOwner;
extern const char *kRepoName;
extern const char *kAssetName;

enum class State {
  Idle,
  Checking,
  UpToDate,
  UpdateAvailable,
  Downloading,
  Ready,
  Failed
};

struct Info {
  std::string remoteVersion;
  std::string downloadUrl;
  std::string releaseNotes;
  int sizeBytes = 0;
};

void startCheck();
State currentState();
const Info &info();
int downloadPct();
const std::string &lastError();
void startDownload();
bool installAndRelaunch();

} // namespace Updater

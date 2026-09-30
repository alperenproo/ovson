#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OVson::Media {

struct Snapshot {
  bool valid = false;   // a session exists at all
  bool playing = false;
  bool shuffle = false;
  int repeatMode = 0;   // 0 none, 1 track, 2 list
  bool canNext = false;
  bool canPrevious = false;
  bool canPlayPause = false;
  bool canShuffle = false;
  bool canRepeat = false;
  bool canSeek = false;
  std::string title;
  std::string artist;
  std::string sourceApp;      // e.g. "spotify.exe"
  std::int64_t positionMs = 0;
  std::int64_t durationMs = 0;
  std::uint64_t sampledAtMs = 0;
  std::uint64_t artworkVersion = 0;
};

struct Artwork {
  std::uint64_t version = 0;
  int width = 0;
  int height = 0;
  std::vector<unsigned char> rgba;
};

void start();
void shutdown();

[[nodiscard]] Snapshot snapshot();

bool takeArtwork(Artwork &out);

void next();
void previous();
void togglePlayPause();
void toggleShuffle();
void cycleRepeat();
void seekTo(std::int64_t positionMs);

} // namespace OVson::Media

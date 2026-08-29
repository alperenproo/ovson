#pragma once

#include "BlockHitAudio.h"

#include <cstdint>
#include <string>

namespace BlockHitAudioBackend {

enum class EventKind {
  Loaded,
  LoadFailed,
  SelectionChanged,
  NoFiles,
  BackendFailed,
  PlaybackFailed,
};

struct Event {
  EventKind kind = EventKind::LoadFailed;
  BlockHitAudio::WavError wavError = BlockHitAudio::WavError::None;
  std::string filename;
  std::uint64_t durationMs = 0;
};

// All file access, WAV validation, DirectSound setup, and buffer replacement
// happen on one owned worker thread. These request functions only enqueue small
// commands and are safe to call from OVson's render/update path.
void requestLoad(const std::string &soundsDirectory,
                 const std::string &filename, float volumePercent);
void requestPlay();
void requestStop();
void requestVolume(float volumePercent);
void requestSelectNext(const std::string &soundsDirectory,
                       const std::string &currentFilename);
bool isReady() noexcept;
bool pollEvent(Event &event);

// Joins the worker and releases DirectSound before the DLL can unload.
void shutdown();

} // namespace BlockHitAudioBackend

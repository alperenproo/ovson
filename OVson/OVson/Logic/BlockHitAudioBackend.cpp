#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "BlockHitAudioBackend.h"

#include <Windows.h>
#include <mmsystem.h>
#include <dsound.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace BlockHitAudioBackend {
namespace {

constexpr std::size_t kMaximumCommands = 32U;
constexpr std::size_t kMaximumEvents = 16U;

enum class CommandKind { Load, Play, Stop, Volume, SelectNext };

struct Command {
  CommandKind kind = CommandKind::Stop;
  std::string directory;
  std::string filename;
  float volumePercent = BlockHitAudio::kDefaultVolumePercent;
  std::uint64_t loadGeneration = 0U;
};

std::mutex g_commandMutex;
std::condition_variable g_commandReady;
std::deque<Command> g_commands;
std::thread g_worker;
bool g_workerStarted = false;
bool g_stopping = false;
bool g_shutdownPermanent = false;
std::atomic<bool> g_ready{false};
std::atomic<std::uint64_t> g_requestedLoadGeneration{0U};

std::mutex g_eventMutex;
std::deque<Event> g_events;

void releaseBuffer(IDirectSoundBuffer *&buffer) noexcept {
  if (buffer) buffer->Release();
  buffer = nullptr;
}

void releaseDevice(IDirectSound8 *&device) noexcept {
  if (device) device->Release();
  device = nullptr;
}

void pushEvent(Event event) {
  std::lock_guard<std::mutex> lock(g_eventMutex);
  if (g_events.size() >= kMaximumEvents) g_events.pop_front();
  g_events.push_back(std::move(event));
}

LONG directSoundVolume(float volumePercent) noexcept {
  const float sanitized =
      BlockHitAudio::sanitizeVolumePercent(volumePercent);
  if (sanitized <= 0.0f) return DSBVOLUME_MIN;
  if (sanitized >= 100.0f) return DSBVOLUME_MAX;
  const double linear = static_cast<double>(sanitized) / 100.0;
  const long hundredthsDb =
      static_cast<long>(std::lround(2000.0 * std::log10(linear)));
  return std::clamp(hundredthsDb, static_cast<long>(DSBVOLUME_MIN),
                    static_cast<long>(DSBVOLUME_MAX));
}

bool equalsIgnoreCase(const std::string &left,
                      const std::string &right) noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    const unsigned char a = static_cast<unsigned char>(left[index]);
    const unsigned char b = static_cast<unsigned char>(right[index]);
    const unsigned char lowerA =
        a >= 'A' && a <= 'Z' ? static_cast<unsigned char>(a + ('a' - 'A')) : a;
    const unsigned char lowerB =
        b >= 'A' && b <= 'Z' ? static_cast<unsigned char>(b + ('a' - 'A')) : b;
    if (lowerA != lowerB) return false;
  }
  return true;
}

struct FileReadResult {
  BlockHitAudio::WavError error = BlockHitAudio::WavError::MissingFile;
  std::optional<std::vector<std::uint8_t>> bytes;
};

FileReadResult readFile(const std::string &path) {
  HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) return {};

  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size) || size.QuadPart < 0) {
    CloseHandle(file);
    return {};
  }
  if (static_cast<std::uint64_t>(size.QuadPart) >
      BlockHitAudio::kMaximumWavFileBytes) {
    CloseHandle(file);
    return {BlockHitAudio::WavError::Oversized, std::nullopt};
  }

  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size.QuadPart));
  DWORD read = 0;
  const bool ok = bytes.empty() ||
                  (ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                            &read, nullptr) &&
                   read == static_cast<DWORD>(bytes.size()));
  CloseHandle(file);
  if (!ok) return {};
  return {BlockHitAudio::WavError::None, std::move(bytes)};
}

bool initializeDevice(IDirectSound8 *&device) noexcept {
  if (device) return true;
  if (FAILED(DirectSoundCreate8(nullptr, &device, nullptr)) || !device) {
    releaseDevice(device);
    return false;
  }
  HWND window = GetForegroundWindow();
  if (!window) window = GetDesktopWindow();
  if (FAILED(device->SetCooperativeLevel(window, DSSCL_NORMAL))) {
    releaseDevice(device);
    return false;
  }
  return true;
}

bool createBuffer(IDirectSound8 *device, const BlockHitAudio::WavCache &cache,
                  float volumePercent, IDirectSoundBuffer *&buffer) noexcept {
  releaseBuffer(buffer);
  if (!device || !cache.ready()) return false;
  const auto &info = cache.info();
  const auto &bytes = cache.bytes();

  WAVEFORMATEX format{};
  format.wFormatTag = WAVE_FORMAT_PCM;
  format.nChannels = info.channels;
  format.nSamplesPerSec = info.sampleRate;
  format.nAvgBytesPerSec = info.bytesPerSecond;
  format.nBlockAlign = info.blockAlign;
  format.wBitsPerSample = info.bitsPerSample;

  DSBUFFERDESC description{};
  description.dwSize = sizeof(description);
  description.dwFlags = DSBCAPS_CTRLVOLUME;
  description.dwBufferBytes = static_cast<DWORD>(info.dataSize);
  description.lpwfxFormat = &format;
  if (FAILED(device->CreateSoundBuffer(&description, &buffer, nullptr)) ||
      !buffer) {
    releaseBuffer(buffer);
    return false;
  }

  void *first = nullptr;
  void *second = nullptr;
  DWORD firstBytes = 0;
  DWORD secondBytes = 0;
  HRESULT locked = buffer->Lock(0U, description.dwBufferBytes, &first,
                                &firstBytes, &second, &secondBytes, 0U);
  if (locked == DSERR_BUFFERLOST) {
    buffer->Restore();
    locked = buffer->Lock(0U, description.dwBufferBytes, &first, &firstBytes,
                          &second, &secondBytes, 0U);
  }
  if (FAILED(locked)) {
    releaseBuffer(buffer);
    return false;
  }

  const std::uint8_t *source = bytes.data() + info.dataOffset;
  if (first && firstBytes > 0U) std::memcpy(first, source, firstBytes);
  if (second && secondBytes > 0U)
    std::memcpy(second, source + firstBytes, secondBytes);
  if (FAILED(buffer->Unlock(first, firstBytes, second, secondBytes))) {
    releaseBuffer(buffer);
    return false;
  }
  if (FAILED(buffer->SetVolume(directSoundVolume(volumePercent)))) {
    releaseBuffer(buffer);
    return false;
  }
  return true;
}

void selectNextFile(const Command &command) {
  CreateDirectoryA(command.directory.c_str(), nullptr);
  std::string search = command.directory + "\\*.wav";
  WIN32_FIND_DATAA data{};
  HANDLE find = FindFirstFileA(search.c_str(), &data);
  std::vector<std::string> filenames;
  if (find != INVALID_HANDLE_VALUE) {
    do {
      if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U &&
          BlockHitAudio::isSafeWavFilename(data.cFileName)) {
        filenames.emplace_back(data.cFileName);
      }
    } while (FindNextFileA(find, &data));
    FindClose(find);
  }
  std::sort(filenames.begin(), filenames.end(),
            [](const std::string &left, const std::string &right) {
              std::string a = left;
              std::string b = right;
              std::transform(a.begin(), a.end(), a.begin(), [](char value) {
                return value >= 'A' && value <= 'Z'
                           ? static_cast<char>(value + ('a' - 'A'))
                           : value;
              });
              std::transform(b.begin(), b.end(), b.begin(), [](char value) {
                return value >= 'A' && value <= 'Z'
                           ? static_cast<char>(value + ('a' - 'A'))
                           : value;
              });
              return a < b;
            });

  if (filenames.empty()) {
    pushEvent({EventKind::NoFiles, BlockHitAudio::WavError::MissingFile,
               command.filename, 0U});
    return;
  }
  auto current = std::find_if(
      filenames.begin(), filenames.end(), [&](const std::string &candidate) {
        return equalsIgnoreCase(candidate, command.filename);
      });
  if (current == filenames.end() || ++current == filenames.end())
    current = filenames.begin();
  pushEvent({EventKind::SelectionChanged, BlockHitAudio::WavError::None,
             *current, 0U});
}

void workerMain() noexcept {
  IDirectSound8 *device = nullptr;
  IDirectSoundBuffer *buffer = nullptr;
  BlockHitAudio::WavCache cache;
  float volumePercent = BlockHitAudio::kDefaultVolumePercent;

  for (;;) {
    Command command;
    {
      std::unique_lock<std::mutex> lock(g_commandMutex);
      g_commandReady.wait(lock,
                          [] { return g_stopping || !g_commands.empty(); });
      if (g_stopping) break;
      command = std::move(g_commands.front());
      g_commands.pop_front();
    }

    try {
      switch (command.kind) {
    case CommandKind::Load: {
      g_ready.store(false, std::memory_order_release);
      if (buffer) buffer->Stop();
      releaseBuffer(buffer);
      CreateDirectoryA(command.directory.c_str(), nullptr);
      const std::string path = command.directory + "\\" + command.filename;
      FileReadResult file = readFile(path);
      const BlockHitAudio::WavError error =
          file.error == BlockHitAudio::WavError::None
              ? cache.reload(command.filename, std::move(file.bytes))
              : file.error;
      if (command.loadGeneration !=
          g_requestedLoadGeneration.load(std::memory_order_acquire)) {
        cache.clear();
        break;
      }
      if (error != BlockHitAudio::WavError::None) {
        pushEvent({EventKind::LoadFailed, error, command.filename, 0U});
        break;
      }
      if (!initializeDevice(device)) {
        cache.clear();
        if (command.loadGeneration ==
            g_requestedLoadGeneration.load(std::memory_order_acquire)) {
          pushEvent({EventKind::BackendFailed, BlockHitAudio::WavError::None,
                     command.filename, 0U});
        }
        break;
      }
      volumePercent = BlockHitAudio::sanitizeVolumePercent(command.volumePercent);
      if (!createBuffer(device, cache, volumePercent, buffer)) {
        cache.clear();
        if (command.loadGeneration ==
            g_requestedLoadGeneration.load(std::memory_order_acquire)) {
          pushEvent({EventKind::BackendFailed, BlockHitAudio::WavError::None,
                     command.filename, 0U});
        }
        break;
      }
      if (command.loadGeneration !=
          g_requestedLoadGeneration.load(std::memory_order_acquire)) {
        releaseBuffer(buffer);
        cache.clear();
        break;
      }
      g_ready.store(true, std::memory_order_release);
      pushEvent({EventKind::Loaded, BlockHitAudio::WavError::None,
                 command.filename, cache.info().durationMs});
      break;
    }
    case CommandKind::Play:
      if (buffer && volumePercent > 0.0f) {
        buffer->Stop();
        if (FAILED(buffer->SetCurrentPosition(0U)) ||
            FAILED(buffer->Play(0U, 0U, 0U))) {
          g_ready.store(false, std::memory_order_release);
          pushEvent({EventKind::PlaybackFailed,
                     BlockHitAudio::WavError::None, cache.filename(), 0U});
        }
      }
      break;
    case CommandKind::Stop:
      if (buffer) buffer->Stop();
      break;
    case CommandKind::Volume:
      volumePercent =
          BlockHitAudio::sanitizeVolumePercent(command.volumePercent);
      if (buffer &&
          FAILED(buffer->SetVolume(directSoundVolume(volumePercent)))) {
        g_ready.store(false, std::memory_order_release);
        pushEvent({EventKind::PlaybackFailed,
                   BlockHitAudio::WavError::None, cache.filename(), 0U});
      }
      break;
    case CommandKind::SelectNext:
      selectNextFile(command);
      break;
      }
    } catch (...) {
      g_ready.store(false, std::memory_order_release);
      if (buffer) buffer->Stop();
      releaseBuffer(buffer);
      cache.clear();
      try {
        pushEvent({EventKind::BackendFailed, BlockHitAudio::WavError::None,
                   command.filename, 0U});
      } catch (...) {
      }
    }
  }

  g_ready.store(false, std::memory_order_release);
  if (buffer) buffer->Stop();
  releaseBuffer(buffer);
  releaseDevice(device);
}

bool ensureWorker() {
  std::lock_guard<std::mutex> lock(g_commandMutex);
  if (g_workerStarted) return true;
  if (g_shutdownPermanent) return false;
  g_stopping = false;
  try {
    g_worker = std::thread(workerMain);
  } catch (...) {
    return false;
  }
  g_workerStarted = true;
  return true;
}

void enqueue(Command command) {
  if (!ensureWorker()) {
    try {
      pushEvent({EventKind::BackendFailed, BlockHitAudio::WavError::None,
                 command.filename, 0U});
    } catch (...) {
    }
    return;
  }
  {
    std::lock_guard<std::mutex> lock(g_commandMutex);
    if (g_stopping || g_shutdownPermanent) return;
    if (command.kind == CommandKind::Stop ||
        command.kind == CommandKind::Load) {
      g_commands.erase(
          std::remove_if(g_commands.begin(), g_commands.end(),
                         [](const Command &queued) {
                           return queued.kind == CommandKind::Play;
                         }),
          g_commands.end());
    }
    if (command.kind == CommandKind::Volume) {
      g_commands.erase(
          std::remove_if(g_commands.begin(), g_commands.end(),
                         [](const Command &queued) {
                           return queued.kind == CommandKind::Volume;
                         }),
          g_commands.end());
    }
    if (g_commands.size() >= kMaximumCommands) {
      if (command.kind == CommandKind::Play) return;
      g_commands.pop_front();
    }
    g_commands.push_back(std::move(command));
  }
  g_commandReady.notify_one();
}

} // namespace

void requestLoad(const std::string &soundsDirectory,
                 const std::string &filename, float volumePercent) {
  // Do not let a just-changed selection preview or trigger the old buffer
  // while its replacement is still queued for validation.
  g_ready.store(false, std::memory_order_release);
  Command command;
  command.kind = CommandKind::Load;
  command.directory = soundsDirectory;
  command.filename = filename;
  command.volumePercent =
      BlockHitAudio::sanitizeVolumePercent(volumePercent);
  command.loadGeneration =
      g_requestedLoadGeneration.fetch_add(1U, std::memory_order_acq_rel) + 1U;
  enqueue(std::move(command));
}

void requestPlay() { enqueue({CommandKind::Play}); }

void requestStop() { enqueue({CommandKind::Stop}); }

void requestVolume(float volumePercent) {
  Command command;
  command.kind = CommandKind::Volume;
  command.volumePercent =
      BlockHitAudio::sanitizeVolumePercent(volumePercent);
  enqueue(std::move(command));
}

void requestSelectNext(const std::string &soundsDirectory,
                       const std::string &currentFilename) {
  enqueue({CommandKind::SelectNext, soundsDirectory, currentFilename});
}

bool isReady() noexcept { return g_ready.load(std::memory_order_acquire); }

bool pollEvent(Event &event) {
  std::lock_guard<std::mutex> lock(g_eventMutex);
  if (g_events.empty()) return false;
  event = std::move(g_events.front());
  g_events.pop_front();
  return true;
}

void shutdown() {
  {
    std::lock_guard<std::mutex> lock(g_commandMutex);
    g_shutdownPermanent = true;
    if (!g_workerStarted) {
      g_ready.store(false, std::memory_order_release);
      return;
    }
    g_stopping = true;
    g_commands.clear();
  }
  g_commandReady.notify_one();
  if (g_worker.joinable()) g_worker.join();
  {
    std::lock_guard<std::mutex> lock(g_commandMutex);
    g_workerStarted = false;
  }
  {
    std::lock_guard<std::mutex> lock(g_eventMutex);
    g_events.clear();
  }
}

} // namespace BlockHitAudioBackend

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "MediaSession.h"

#include "../Utils/Logger.h"

#include <Windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "../Utils/stb_image.h"

namespace OVson::Media {
namespace {

namespace WinrtControl = winrt::Windows::Media::Control;
namespace WinrtStreams = winrt::Windows::Storage::Streams;

std::mutex g_mutex;
Snapshot g_snapshot;
Artwork g_artwork;
bool g_artworkPending = false;
std::uint64_t g_artworkVersion = 0;

std::thread g_worker;
std::atomic<bool> g_running{false};
std::condition_variable g_cv;

enum class Command { Next, Previous, PlayPause, Shuffle, Repeat, Seek };
struct CommandRequest {
  Command command = Command::PlayPause;
  std::int64_t positionMs = 0;
};
std::mutex g_commandMutex;
std::deque<CommandRequest> g_commands;

std::string g_artworkKey;

std::string toUtf8(const winrt::hstring &value) {
  if (value.empty()) return {};
  return winrt::to_string(value);
}

std::int64_t toMs(const winrt::Windows::Foundation::TimeSpan &span) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(span).count();
}

bool decodeArtwork(const WinrtStreams::IRandomAccessStreamReference &reference,
                   Artwork &out) {
  if (!reference) return false;
  try {
    const auto stream = reference.OpenReadAsync().get();
    if (!stream) return false;
    const std::uint32_t size = static_cast<std::uint32_t>(stream.Size());
    if (size == 0 || size > 8u * 1024u * 1024u) return false;

    WinrtStreams::Buffer buffer(size);
    stream.ReadAsync(buffer, size, WinrtStreams::InputStreamOptions::None).get();

    const auto reader = WinrtStreams::DataReader::FromBuffer(buffer);
    std::vector<unsigned char> encoded(buffer.Length());
    reader.ReadBytes(winrt::array_view<uint8_t>(encoded));

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char *pixels =
        stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()),
                              &width, &height, &channels, 4);
    if (!pixels) return false;

    out.width = width;
    out.height = height;
    out.rgba.assign(pixels, pixels + static_cast<std::size_t>(width) * height * 4);
    stbi_image_free(pixels);
    return true;
  } catch (...) {
    return false;
  }
}

void runCommand(const WinrtControl::GlobalSystemMediaTransportControlsSession &session,
                 const CommandRequest &request) {
  try {
    switch (request.command) {
    case Command::Next: session.TrySkipNextAsync().get(); break;
    case Command::Previous: session.TrySkipPreviousAsync().get(); break;
    case Command::PlayPause: session.TryTogglePlayPauseAsync().get(); break;
    case Command::Shuffle: {
      const auto info = session.GetPlaybackInfo();
      if (info && !info.Controls().IsShuffleEnabled()) break;
      bool shuffled = false;
      if (info && info.IsShuffleActive()) shuffled = info.IsShuffleActive().Value();
      session.TryChangeShuffleActiveAsync(!shuffled).get();
      break;
    }
    case Command::Repeat: {
      const auto info = session.GetPlaybackInfo();
      if (info && !info.Controls().IsRepeatEnabled()) break;
      auto mode = winrt::Windows::Media::MediaPlaybackAutoRepeatMode::None;
      if (info && info.AutoRepeatMode()) mode = info.AutoRepeatMode().Value();
      const auto nextMode =
          mode == winrt::Windows::Media::MediaPlaybackAutoRepeatMode::None
              ? winrt::Windows::Media::MediaPlaybackAutoRepeatMode::List
          : mode == winrt::Windows::Media::MediaPlaybackAutoRepeatMode::List
              ? winrt::Windows::Media::MediaPlaybackAutoRepeatMode::Track
              : winrt::Windows::Media::MediaPlaybackAutoRepeatMode::None;
      session.TryChangeAutoRepeatModeAsync(nextMode).get();
      break;
    }
    case Command::Seek: {
      const auto info = session.GetPlaybackInfo();
      if (info && !info.Controls().IsPlaybackPositionEnabled()) break;
      const auto timeline = session.GetTimelineProperties();
      if (!timeline) break;
      const std::int64_t startMs = toMs(timeline.StartTime());
      const std::int64_t endMs = toMs(timeline.EndTime());
      const std::int64_t durationMs = endMs > startMs ? endMs - startMs : 0;
      const std::int64_t relativeMs =
          request.positionMs < 0
              ? 0
              : (request.positionMs > durationMs ? durationMs
                                                 : request.positionMs);
      session.TryChangePlaybackPositionAsync((startMs + relativeMs) * 10000LL)
          .get();
      break;
    }
    }
  } catch (...) {
  }
}

void poll(const WinrtControl::GlobalSystemMediaTransportControlsSessionManager &manager) {
  Snapshot next;
  WinrtControl::GlobalSystemMediaTransportControlsSession session{nullptr};
  try {
    session = manager.GetCurrentSession();
  } catch (...) {
    session = nullptr;
  }

  if (!session) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshot = Snapshot{};
    g_artworkKey.clear();
    return;
  }

  {
    std::deque<CommandRequest> pending;
    {
      std::lock_guard<std::mutex> lock(g_commandMutex);
      pending.swap(g_commands);
    }
    for (const CommandRequest &request : pending) runCommand(session, request);
  }

  try {
    const auto info = session.GetPlaybackInfo();
    if (info) {
      next.playing = info.PlaybackStatus() ==
                     WinrtControl::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
      if (info.IsShuffleActive()) next.shuffle = info.IsShuffleActive().Value();
      if (info.AutoRepeatMode()) {
        const auto mode = info.AutoRepeatMode().Value();
        next.repeatMode =
            mode == winrt::Windows::Media::MediaPlaybackAutoRepeatMode::Track ? 1
            : mode == winrt::Windows::Media::MediaPlaybackAutoRepeatMode::List ? 2
                                                                              : 0;
      }
      const auto controls = info.Controls();
      next.canNext = controls.IsNextEnabled();
      next.canPrevious = controls.IsPreviousEnabled();
      next.canPlayPause = controls.IsPlayEnabled() || controls.IsPauseEnabled();
      next.canShuffle = controls.IsShuffleEnabled();
      next.canRepeat = controls.IsRepeatEnabled();
      next.canSeek = controls.IsPlaybackPositionEnabled();
    }

    const auto properties = session.TryGetMediaPropertiesAsync().get();
    if (properties) {
      next.title = toUtf8(properties.Title());
      next.artist = toUtf8(properties.Artist());
      if (next.artist.empty()) next.artist = toUtf8(properties.AlbumArtist());
    }

    const auto timeline = session.GetTimelineProperties();
    if (timeline) {
      const std::int64_t startMs = toMs(timeline.StartTime());
      const std::int64_t endMs = toMs(timeline.EndTime());
      next.durationMs = endMs > startMs ? endMs - startMs : 0;
      std::int64_t posMs = toMs(timeline.Position()) - startMs;
      if (posMs < 0) posMs = 0;

      if (next.playing) {
        try {
          const auto lastUpdated = timeline.LastUpdatedTime();
          const auto nowWinrt = winrt::clock::now();
          if (lastUpdated.time_since_epoch().count() > 0 && nowWinrt > lastUpdated) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                nowWinrt - lastUpdated).count();
            posMs += elapsed;
          }
        } catch (...) {
        }
      }
      if (next.durationMs > 0 && posMs > next.durationMs) posMs = next.durationMs;
      next.positionMs = posMs;
    }

    next.sourceApp = toUtf8(session.SourceAppUserModelId());

    {
      static std::string s_loggedApp;
      if (next.sourceApp != s_loggedApp) {
        s_loggedApp = next.sourceApp;
        Logger::info("[Media] source=%s next=%d prev=%d playpause=%d "
                     "shuffle=%d repeat=%d seek=%d",
                     next.sourceApp.c_str(), next.canNext ? 1 : 0,
                     next.canPrevious ? 1 : 0, next.canPlayPause ? 1 : 0,
                     next.canShuffle ? 1 : 0, next.canRepeat ? 1 : 0,
                     next.canSeek ? 1 : 0);
      }
    }
    next.valid = !next.title.empty() || !next.artist.empty();
    next.sampledAtMs = GetTickCount64();

    const std::string key = next.title + "\x1f" + next.artist;
    bool needsArtwork = false;
    {
      std::lock_guard<std::mutex> lock(g_mutex);
      needsArtwork = next.valid && key != g_artworkKey;
    }
    if (needsArtwork) {
      Artwork art;
      const auto properties2 = session.TryGetMediaPropertiesAsync().get();
      if (properties2 && decodeArtwork(properties2.Thumbnail(), art)) {
        std::lock_guard<std::mutex> lock(g_mutex);
        art.version = ++g_artworkVersion;
        g_artwork = std::move(art);
        g_artworkPending = true;
        g_artworkKey = key;
      } else {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_artworkKey = key;
      }
    }
  } catch (...) {
    next = Snapshot{};
  }

  {
    std::lock_guard<std::mutex> lock(g_mutex);
    next.artworkVersion = g_artworkVersion;
    g_snapshot = next;
  }
}

void workerMain() {
  try {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
  } catch (...) {
    Logger::error("[Media] WinRT apartment could not be initialised");
    return;
  }

  WinrtControl::GlobalSystemMediaTransportControlsSessionManager manager{nullptr};
  try {
    manager = WinrtControl::GlobalSystemMediaTransportControlsSessionManager::
        RequestAsync().get();
  } catch (...) {
    Logger::error("[Media] no media session manager on this system");
    winrt::uninit_apartment();
    return;
  }

  Logger::info("[Media] session manager ready");
  while (g_running.load(std::memory_order_acquire)) {
    poll(manager);
    std::unique_lock<std::mutex> lock(g_commandMutex);
    g_cv.wait_for(lock, std::chrono::milliseconds(250), [&]() {
      return !g_running.load(std::memory_order_acquire) || !g_commands.empty();
    });
  }
  winrt::uninit_apartment();
}

} // namespace

void start() {
  if (g_running.exchange(true)) return;
  g_worker = std::thread(workerMain);
}

void shutdown() {
  if (!g_running.exchange(false)) return;
  g_cv.notify_all();
  if (g_worker.joinable()) g_worker.join();
  std::lock_guard<std::mutex> lock(g_mutex);
  g_snapshot = Snapshot{};
  g_artwork = Artwork{};
  g_artworkPending = false;
  g_artworkKey.clear();
}

Snapshot snapshot() {
  std::lock_guard<std::mutex> lock(g_mutex);
  return g_snapshot;
}

bool takeArtwork(Artwork &out) {
  std::lock_guard<std::mutex> lock(g_mutex);
  if (!g_artworkPending) return false;
  out = std::move(g_artwork);
  g_artwork = Artwork{};
  g_artworkPending = false;
  return true;
}

namespace {
void queue(Command command, std::int64_t positionMs = 0) {
  {
    std::lock_guard<std::mutex> lock(g_commandMutex);
    if (command == Command::Seek) {
      bool replaced = false;
      for (auto &req : g_commands) {
        if (req.command == Command::Seek) {
          req.positionMs = positionMs;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        g_commands.push_back(CommandRequest{command, positionMs});
      }
    } else {
      if (g_commands.size() < 16) {
        g_commands.push_back(CommandRequest{command, positionMs});
      }
    }
  }
  g_cv.notify_one();
}
} // namespace

void next() { queue(Command::Next); }
void previous() { queue(Command::Previous); }
void togglePlayPause() { queue(Command::PlayPause); }
void toggleShuffle() { queue(Command::Shuffle); }
void cycleRepeat() { queue(Command::Repeat); }
void seekTo(std::int64_t positionMs) { queue(Command::Seek, positionMs); }

} // namespace OVson::Media

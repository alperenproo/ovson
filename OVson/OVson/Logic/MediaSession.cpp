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
#include <deque>
#include <mutex>
#include <thread>

// The implementation is compiled once, in Render/TextureLoader.cpp. Including
// the header without the define here just pulls in the declarations.
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

// Transport requests are queued rather than executed inline: the caller is the
// render thread, and every one of these is an async WinRT operation.
enum class Command { Next, Previous, PlayPause, Shuffle, Repeat, Seek };
struct CommandRequest {
  Command command = Command::PlayPause;
  std::int64_t positionMs = 0;
};
std::mutex g_commandMutex;
std::deque<CommandRequest> g_commands;

// What the artwork was last decoded for. Album art is re-read only when the
// track actually changes, which is a few times an hour, not a few times a
// second.
std::string g_artworkKey;

std::string toUtf8(const winrt::hstring &value) {
  if (value.empty()) return {};
  return winrt::to_string(value);
}

std::int64_t toMs(const winrt::Windows::Foundation::TimeSpan &span) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(span).count();
}

// Reads the thumbnail stream into memory and decodes it here, on the worker.
// Doing this on the render thread would stall a frame every time the song
// changed -- a visible hitch, and exactly the mistake this design exists to
// avoid.
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
      // Second gate. The card already hides this button when the session does
      // not advertise shuffle, but a stale snapshot could let one click
      // through, and the source app answers an unsupported command with a
      // popup of its own -- which looks like our bug, not theirs.
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
      // WinRT takes 100ns units here, while the renderer and public API use ms.
      session.TryChangePlaybackPositionAsync((startMs + relativeMs) * 10000LL)
          .get();
      break;
    }
    }
  } catch (...) {
    // A player that refuses a transport request is normal -- not every source
    // supports every control. Nothing to report and nothing to retry.
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
      next.positionMs = toMs(timeline.Position()) - startMs;
      next.durationMs = toMs(timeline.EndTime()) - startMs;
      if (next.positionMs < 0) next.positionMs = 0;
      if (next.durationMs < 0) next.durationMs = 0;
    }

    next.sourceApp = toUtf8(session.SourceAppUserModelId());

    // One line per source change, so the question "does this player actually
    // support shuffle and repeat?" is answered by the log instead of by
    // guesswork. Spotify is the interesting case: it accepts play/pause/next/
    // previous over SMTC but has never implemented the shuffle and repeat
    // requests, and what it reports here is the evidence either way.
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

    // Artwork is keyed on the track, not polled. Same song, same picture.
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
        // Remember the attempt either way, or a track with no cover art is
        // re-decoded twice a second forever.
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
  // Multithreaded apartment: this runs on our own thread inside a hooked JVM
  // process, and an STA would need a message pump we have no business
  // installing here. That choice is also why this polls rather than
  // subscribing to MediaPropertiesChanged -- WinRT events need an apartment
  // that can dispatch them. Twice a second on a worker thread costs nothing
  // the player can feel; the thing that would cost frames is touching any of
  // this from the render thread, which nothing here does.
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
    for (int i = 0; i < 5 && g_running.load(std::memory_order_acquire); ++i)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
  std::lock_guard<std::mutex> lock(g_commandMutex);
  if (g_commands.size() >= 8) return;  // a stuck worker must not grow a backlog
  g_commands.push_back(CommandRequest{command, positionMs});
}
} // namespace

void next() { queue(Command::Next); }
void previous() { queue(Command::Previous); }
void togglePlayPause() { queue(Command::PlayPause); }
void toggleShuffle() { queue(Command::Shuffle); }
void cycleRepeat() { queue(Command::Repeat); }
void seekTo(std::int64_t positionMs) { queue(Command::Seek, positionMs); }

} // namespace OVson::Media

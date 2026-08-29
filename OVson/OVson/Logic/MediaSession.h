#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Reads whatever is currently playing on this machine through Windows' own
// media session registry -- the same channel the keyboard media keys and the
// volume flyout use.
//
// Deliberately NOT the Spotify Web API. As of the February 2026 changes a
// Development Mode app is capped at a handful of users and requires the app
// owner to keep an active Premium subscription, and Extended Quota Mode is
// reserved for "established, scalable" platform partners -- a publicly
// distributed Minecraft utility is not getting through that. This route needs
// no account, no OAuth, no key, no quota, works without Premium, and covers
// the browser and every other player as well as the Spotify desktop app.
//
// The trade is that it controls whichever app currently owns the media
// session. Spotify-specific things -- searching, playlists, "play this track"
// -- are not available and are not what the overlay needs.
namespace OVson::Media {

// What the renderer is allowed to see. Published from the worker under a lock;
// the render thread only ever copies this and never touches WinRT.
struct Snapshot {
  bool valid = false;   // a session exists at all
  bool playing = false;
  bool shuffle = false;
  int repeatMode = 0;   // 0 none, 1 track, 2 list
  bool canNext = false;
  bool canPrevious = false;
  bool canPlayPause = false;
  // Reported by the session itself. Spotify and most browsers advertise
  // transport but not shuffle/repeat, and sending a command a session never
  // claimed makes the source app pop its own error toast.
  bool canShuffle = false;
  bool canRepeat = false;
  bool canSeek = false;
  std::string title;
  std::string artist;
  std::string sourceApp;      // e.g. "Spotify.exe"
  std::int64_t positionMs = 0;
  std::int64_t durationMs = 0;
  // GetTickCount64() at the moment position was sampled, so the renderer can
  // advance the progress bar smoothly between polls instead of stepping.
  std::uint64_t sampledAtMs = 0;
  std::uint64_t artworkVersion = 0;
};

// Decoded album art. Handed over once per track: the decode happens on the
// worker, and only the glTexImage2D upload is left for the render thread,
// because that is the one part that has to hold the GL context.
struct Artwork {
  std::uint64_t version = 0;
  int width = 0;
  int height = 0;
  std::vector<unsigned char> rgba;
};

void start();
void shutdown();

[[nodiscard]] Snapshot snapshot();

// True exactly once per new artwork; moves the pixels out.
bool takeArtwork(Artwork &out);

// Transport. Queued onto the worker -- calling these from the render thread
// must never block on an async WinRT operation.
void next();
void previous();
void togglePlayPause();
void toggleShuffle();
void cycleRepeat();
void seekTo(std::int64_t positionMs);

} // namespace OVson::Media

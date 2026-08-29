#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace BlockHitAudio {

inline constexpr bool kDefaultFeatureEnabled = false;
inline constexpr bool kDefaultDebugLoggingEnabled = false;
inline constexpr float kDefaultVolumePercent = 22.0f;
inline constexpr std::string_view kDefaultCustomFilename = "block-hit.wav";
inline constexpr std::size_t kMaximumWavFileBytes = 8U * 1024U * 1024U;
inline constexpr std::uint64_t kMaximumWavDurationMs = 15000U;

enum class SoundSource { Default, Custom };
enum class PlaybackTarget { None, DefaultMinecraft, CustomWav };

enum class WavError {
  None,
  MissingFile,
  UnsafeFilename,
  Empty,
  Oversized,
  Truncated,
  InvalidContainer,
  MissingFormatChunk,
  MissingDataChunk,
  UnsupportedEncoding,
  UnsupportedChannels,
  UnsupportedSampleRate,
  UnsupportedBitDepth,
  InvalidFormat,
  TooLong,
};

struct WavInfo {
  std::uint16_t channels = 0;
  std::uint32_t sampleRate = 0;
  std::uint16_t bitsPerSample = 0;
  std::uint16_t blockAlign = 0;
  std::uint32_t bytesPerSecond = 0;
  std::size_t dataOffset = 0;
  std::size_t dataSize = 0;
  std::uint64_t durationMs = 0;
};

float sanitizeVolumePercent(float value) noexcept;
SoundSource parseSoundSource(std::string_view value) noexcept;
const char *soundSourceName(SoundSource source) noexcept;
bool isSafeWavFilename(std::string_view filename) noexcept;
WavError validatePcmWav(std::span<const std::uint8_t> bytes,
                        WavInfo &info) noexcept;
const char *wavErrorMessage(WavError error) noexcept;

PlaybackTarget choosePlayback(bool featureEnabled, bool preview,
                              SoundSource source, bool customReady,
                              float volumePercent) noexcept;

// Owns the validated bytes so the backend can create and recreate its audio
// buffer without touching disk at hit time.
class WavCache {
public:
  WavError reload(std::string filename,
                  std::optional<std::vector<std::uint8_t>> bytes) noexcept;
  void clear() noexcept;

  bool ready() const noexcept { return ready_; }
  const std::string &filename() const noexcept { return filename_; }
  const std::vector<std::uint8_t> &bytes() const noexcept { return bytes_; }
  const WavInfo &info() const noexcept { return info_; }

private:
  bool ready_ = false;
  std::string filename_;
  std::vector<std::uint8_t> bytes_;
  WavInfo info_{};
};

} // namespace BlockHitAudio

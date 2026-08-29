#include "BlockHitAudio.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace BlockHitAudio {
namespace {

constexpr std::uint32_t kMinimumSampleRate = 8000U;
constexpr std::uint32_t kMaximumSampleRate = 192000U;

std::uint16_t readU16(std::span<const std::uint8_t> bytes,
                      std::size_t offset) noexcept {
  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(bytes[offset + 1]) << 8U;
}

std::uint32_t readU32(std::span<const std::uint8_t> bytes,
                      std::size_t offset) noexcept {
  return static_cast<std::uint32_t>(bytes[offset]) |
         static_cast<std::uint32_t>(bytes[offset + 1]) << 8U |
         static_cast<std::uint32_t>(bytes[offset + 2]) << 16U |
         static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

bool hasTag(std::span<const std::uint8_t> bytes, std::size_t offset,
            const char (&tag)[5]) noexcept {
  return offset <= bytes.size() && bytes.size() - offset >= 4U &&
         std::memcmp(bytes.data() + offset, tag, 4U) == 0;
}

char asciiLower(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A'))
                                      : value;
}

} // namespace

float sanitizeVolumePercent(float value) noexcept {
  if (!std::isfinite(value)) return kDefaultVolumePercent;
  return std::clamp(value, 0.0f, 100.0f);
}

SoundSource parseSoundSource(std::string_view value) noexcept {
  if (value.size() == 6U && asciiLower(value[0]) == 'c' &&
      asciiLower(value[1]) == 'u' && asciiLower(value[2]) == 's' &&
      asciiLower(value[3]) == 't' && asciiLower(value[4]) == 'o' &&
      asciiLower(value[5]) == 'm') {
    return SoundSource::Custom;
  }
  return SoundSource::Default;
}

const char *soundSourceName(SoundSource source) noexcept {
  return source == SoundSource::Custom ? "Custom" : "Default";
}

bool isSafeWavFilename(std::string_view filename) noexcept {
  if (filename.empty() || filename.size() > 128U || filename == "." ||
      filename == "..") {
    return false;
  }
  for (const char value : filename) {
    const unsigned char byte = static_cast<unsigned char>(value);
    if (byte < 32U || value == '/' || value == '\\' || value == ':' ||
        value == '"' || value == '<' || value == '>' || value == '|' ||
        value == '*' || value == '?') {
      return false;
    }
  }
  constexpr std::string_view extension = ".wav";
  if (filename.size() <= extension.size()) return false;
  const std::size_t start = filename.size() - extension.size();
  for (std::size_t index = 0; index < extension.size(); ++index) {
    if (asciiLower(filename[start + index]) != extension[index]) return false;
  }
  return true;
}

WavError validatePcmWav(std::span<const std::uint8_t> bytes,
                        WavInfo &info) noexcept {
  info = {};
  if (bytes.empty()) return WavError::Empty;
  if (bytes.size() > kMaximumWavFileBytes) return WavError::Oversized;
  if (bytes.size() < 12U) return WavError::Truncated;
  if (!hasTag(bytes, 0U, "RIFF") || !hasTag(bytes, 8U, "WAVE"))
    return WavError::InvalidContainer;

  const std::uint64_t declaredEnd = 8ULL + readU32(bytes, 4U);
  if (declaredEnd < 12ULL || declaredEnd > bytes.size())
    return WavError::Truncated;
  const std::size_t riffEnd = static_cast<std::size_t>(declaredEnd);

  bool haveFormat = false;
  bool haveData = false;
  std::uint16_t encoding = 0;
  std::size_t offset = 12U;
  while (offset < riffEnd) {
    if (riffEnd - offset < 8U) return WavError::Truncated;
    const std::uint32_t chunkSize = readU32(bytes, offset + 4U);
    const std::size_t chunkData = offset + 8U;
    if (static_cast<std::uint64_t>(chunkData) + chunkSize > riffEnd)
      return WavError::Truncated;

    if (!haveFormat && hasTag(bytes, offset, "fmt ")) {
      if (chunkSize < 16U) return WavError::Truncated;
      encoding = readU16(bytes, chunkData);
      info.channels = readU16(bytes, chunkData + 2U);
      info.sampleRate = readU32(bytes, chunkData + 4U);
      info.bytesPerSecond = readU32(bytes, chunkData + 8U);
      info.blockAlign = readU16(bytes, chunkData + 12U);
      info.bitsPerSample = readU16(bytes, chunkData + 14U);
      haveFormat = true;
    } else if (!haveData && hasTag(bytes, offset, "data")) {
      if (chunkSize == 0U) return WavError::Empty;
      info.dataOffset = chunkData;
      info.dataSize = chunkSize;
      haveData = true;
    }

    const std::uint64_t next = static_cast<std::uint64_t>(chunkData) +
                               chunkSize + (chunkSize & 1U);
    if (next > riffEnd) return WavError::Truncated;
    offset = static_cast<std::size_t>(next);
  }

  if (!haveFormat) return WavError::MissingFormatChunk;
  if (!haveData) return WavError::MissingDataChunk;
  if (encoding != 1U) return WavError::UnsupportedEncoding;
  if (info.channels != 1U && info.channels != 2U)
    return WavError::UnsupportedChannels;
  if (info.sampleRate < kMinimumSampleRate ||
      info.sampleRate > kMaximumSampleRate)
    return WavError::UnsupportedSampleRate;
  if (info.bitsPerSample != 16U) return WavError::UnsupportedBitDepth;

  const std::uint32_t expectedAlign =
      static_cast<std::uint32_t>(info.channels) * 2U;
  const std::uint64_t expectedRate =
      static_cast<std::uint64_t>(info.sampleRate) * expectedAlign;
  if (info.blockAlign != expectedAlign || info.bytesPerSecond != expectedRate ||
      info.dataSize % info.blockAlign != 0U) {
    return WavError::InvalidFormat;
  }

  info.durationMs =
      (static_cast<std::uint64_t>(info.dataSize) * 1000ULL +
       info.bytesPerSecond - 1ULL) /
      info.bytesPerSecond;
  if (info.durationMs > kMaximumWavDurationMs) return WavError::TooLong;
  return WavError::None;
}

const char *wavErrorMessage(WavError error) noexcept {
  switch (error) {
  case WavError::None: return "loaded";
  case WavError::MissingFile: return "file is missing or unreadable";
  case WavError::UnsafeFilename: return "filename must be a local .wav name";
  case WavError::Empty: return "file or audio data is empty";
  case WavError::Oversized: return "file exceeds the 8 MiB limit";
  case WavError::Truncated: return "WAV is truncated";
  case WavError::InvalidContainer: return "file is not a RIFF/WAVE file";
  case WavError::MissingFormatChunk: return "WAV has no format chunk";
  case WavError::MissingDataChunk: return "WAV has no data chunk";
  case WavError::UnsupportedEncoding: return "WAV is not uncompressed PCM";
  case WavError::UnsupportedChannels: return "only mono or stereo is supported";
  case WavError::UnsupportedSampleRate: return "sample rate is unsupported";
  case WavError::UnsupportedBitDepth: return "only 16-bit PCM is supported";
  case WavError::InvalidFormat: return "WAV format fields are inconsistent";
  case WavError::TooLong: return "audio exceeds the 15 second limit";
  default: return "unknown WAV error";
  }
}

PlaybackTarget choosePlayback(bool featureEnabled, bool preview,
                              SoundSource source, bool customReady,
                              float volumePercent) noexcept {
  if ((!featureEnabled && !preview) ||
      sanitizeVolumePercent(volumePercent) <= 0.0f) {
    return PlaybackTarget::None;
  }
  if (source == SoundSource::Custom && customReady)
    return PlaybackTarget::CustomWav;
  return PlaybackTarget::DefaultMinecraft;
}

WavError WavCache::reload(
    std::string filename,
    std::optional<std::vector<std::uint8_t>> bytes) noexcept {
  clear();
  filename_ = std::move(filename);
  if (!isSafeWavFilename(filename_)) return WavError::UnsafeFilename;
  if (!bytes) return WavError::MissingFile;

  WavInfo candidate;
  const WavError error = validatePcmWav(*bytes, candidate);
  if (error != WavError::None) return error;
  bytes_ = std::move(*bytes);
  info_ = candidate;
  ready_ = true;
  return WavError::None;
}

void WavCache::clear() noexcept {
  ready_ = false;
  filename_.clear();
  bytes_.clear();
  info_ = {};
}

} // namespace BlockHitAudio

#pragma once

#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Library/Synth/Core/SynthStorage.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#include <string_view>

namespace synth::sampler {

inline constexpr std::size_t kMaxSampleFrames = 44100 * 5;
inline constexpr std::uint32_t kMinSampleRate = 8000;
inline constexpr std::uint32_t kMaxSampleRate = 96000;

inline std::shared_ptr<audio::SampleBuffer> AllocateFrames(std::size_t frameCount)
{
  if (frameCount == 0 || frameCount > kMaxSampleFrames)
  {
    return {};
  }
  auto frames = memory::MakeExternalArray<std::int16_t>(frameCount);
  if (!frames)
  {
    return {};
  }
  std::shared_ptr<audio::SampleBuffer> buffer(new (std::nothrow) audio::SampleBuffer());
  if (!buffer) return {};
  buffer->frames = std::move(frames);
  buffer->frameCount = frameCount;
  return buffer;
}

inline std::uint16_t ReadU16(std::istream& file)
{
  std::array<unsigned char, 2> bytes{};
  file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  return static_cast<std::uint16_t>(bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8));
}

inline std::uint32_t ReadU32(std::istream& file)
{
  std::array<unsigned char, 4> bytes{};
  file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
  return static_cast<std::uint32_t>(bytes[0]) |
      (static_cast<std::uint32_t>(bytes[1]) << 8) |
      (static_cast<std::uint32_t>(bytes[2]) << 16) |
      (static_cast<std::uint32_t>(bytes[3]) << 24);
}

struct WavInfo {
  std::uint32_t sampleRate = 0;
  std::uint32_t frameCount = 0;
  std::uint64_t dataOffset = 0;
};

inline bool InspectMono16Wav(std::ifstream& file, WavInfo& info)
{
  file.seekg(0, std::ios::end);
  const std::streamoff length = file.tellg();
  if (length < 12)
  {
    return false;
  }
  file.seekg(0);
  std::array<char, 4> id{};
  file.read(id.data(), id.size());
  if (std::string_view(id.data(), id.size()) != "RIFF")
  {
    return false;
  }
  const std::uint64_t riffEnd = static_cast<std::uint64_t>(ReadU32(file)) + 8;
  file.read(id.data(), id.size());
  if (!file || riffEnd < 12 || riffEnd != static_cast<std::uint64_t>(length) ||
      std::string_view(id.data(), id.size()) != "WAVE")
  {
    return false;
  }

  WavInfo parsed;
  bool foundFormat = false;
  bool foundData = false;
  for (std::uint64_t offset = 12; offset < riffEnd;)
  {
    if (riffEnd - offset < 8)
    {
      return false;
    }
    file.seekg(static_cast<std::streamoff>(offset));
    file.read(id.data(), id.size());
    const std::uint32_t size = ReadU32(file);
    const std::uint64_t dataOffset = offset + 8;
    const std::uint64_t next = dataOffset + size + (size & 1u);
    if (!file || next > riffEnd)
    {
      return false;
    }
    const std::string_view chunk(id.data(), id.size());
    if (chunk == "fmt ")
    {
      if (foundFormat || size < 16)
      {
        return false;
      }
      const std::uint16_t format = ReadU16(file);
      const std::uint16_t channels = ReadU16(file);
      parsed.sampleRate = ReadU32(file);
      const std::uint32_t byteRate = ReadU32(file);
      const std::uint16_t alignment = ReadU16(file);
      const std::uint16_t bits = ReadU16(file);
      if (!file || format != 1 || channels != 1 || bits != 16 || alignment != 2 ||
          parsed.sampleRate < kMinSampleRate || parsed.sampleRate > kMaxSampleRate ||
          byteRate != parsed.sampleRate * 2)
      {
        return false;
      }
      foundFormat = true;
    }
    else if (chunk == "data")
    {
      if (foundData || size == 0 || (size & 1u) != 0 || size / 2 > kMaxSampleFrames)
      {
        return false;
      }
      parsed.dataOffset = dataOffset;
      parsed.frameCount = size / 2;
      foundData = true;
    }
    offset = next;
  }
  if (!foundFormat || !foundData)
  {
    return false;
  }
  info = parsed;
  return true;
}

inline bool ReadMono16Wav(const std::string& path, audio::SharedSampleBuffer& frames, std::uint32_t& sampleRate)
{
  std::ifstream file(path, std::ios::binary);
  WavInfo info;
  if (!file || !InspectMono16Wav(file, info))
  {
    return false;
  }
  auto loaded = AllocateFrames(info.frameCount);
  if (!loaded)
  {
    return false;
  }
  file.seekg(static_cast<std::streamoff>(info.dataOffset));
  const auto byteCount = static_cast<std::streamsize>(info.frameCount * sizeof(std::int16_t));
  file.read(reinterpret_cast<char*>(loaded->frames.get()), byteCount);
  if (!file || file.gcount() != byteCount)
  {
    return false;
  }
  frames = std::move(loaded);
  sampleRate = info.sampleRate;
  return true;
}

inline void WriteU16(std::ostream& file, std::uint16_t value)
{
  const char bytes[] = {static_cast<char>(value), static_cast<char>(value >> 8)};
  file.write(bytes, sizeof(bytes));
}

inline void WriteU32(std::ostream& file, std::uint32_t value)
{
  const char bytes[] = {static_cast<char>(value), static_cast<char>(value >> 8),
                        static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
  file.write(bytes, sizeof(bytes));
}

inline bool WriteMono16Wav(const std::string& path, const std::int16_t* frames,
                           std::size_t frameCount, std::uint32_t sampleRate)
{
  if (!frames || frameCount == 0 || frameCount > kMaxSampleFrames ||
      sampleRate < kMinSampleRate || sampleRate > kMaxSampleRate)
  {
    return false;
  }
  const std::string temporary = path + ".tmp";
  const std::string backup = path + ".bak";
  // A previous interrupted replacement must remain recoverable.
  if (std::ifstream(backup, std::ios::binary).good())
  {
    return false;
  }
  const auto failed = [] { storage::NotifyWriteFailure(); return false; };
  std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
  const auto bytes = static_cast<std::uint32_t>(frameCount * sizeof(std::int16_t));
  file.write("RIFF", 4);
  WriteU32(file, 36 + bytes);
  file.write("WAVEfmt ", 8);
  WriteU32(file, 16);
  WriteU16(file, 1);
  WriteU16(file, 1);
  WriteU32(file, sampleRate);
  WriteU32(file, sampleRate * 2);
  WriteU16(file, 2);
  WriteU16(file, 16);
  file.write("data", 4);
  WriteU32(file, bytes);
  file.write(reinterpret_cast<const char*>(frames), bytes);
  file.flush();
  file.close();
  if (!file)
  {
    std::remove(temporary.c_str());
    return failed();
  }

  const bool hadOriginal = std::ifstream(path, std::ios::binary).good();
  if (hadOriginal && std::rename(path.c_str(), backup.c_str()) != 0)
  {
    std::remove(temporary.c_str());
    return failed();
  }
  if (std::rename(temporary.c_str(), path.c_str()) != 0)
  {
    if (hadOriginal)
    {
      // If rollback itself fails, leave the .bak intact for recovery.
      (void)std::rename(backup.c_str(), path.c_str());
    }
    std::remove(temporary.c_str());
    return failed();
  }
  if (hadOriginal)
  {
    (void)std::remove(backup.c_str());
  }
  return true;
}

}  // namespace synth::sampler

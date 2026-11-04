#pragma once

#include "Library/Memory/Memory.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#include <string_view>

namespace synth::sampler {

// Five minutes. Samples up to RamSampleFrameLimit() are read into RAM, the longer ones play from the
// card: with PSRAM (where they are kept, read once per voice per block, in order) two seconds,
// 176 KB, drum hits, one-shots and short loops; without it 186 ms, 16 KB of internal RAM.
inline constexpr std::size_t kMaxSampleFrames = 44100 * 300;
inline constexpr std::size_t kMaxRamSampleFrames = 8192;
inline constexpr std::size_t kMaxSlowRamSampleFrames = 44100 * 2;
inline std::size_t RamSampleFrameLimit() { return memory::HasSlow() ? kMaxSlowRamSampleFrames : kMaxRamSampleFrames; }
// What samples leave in PSRAM for patterns and snapshots (a sample that does not fit streams).
inline constexpr std::size_t kSampleSlowReserveBytes = 256 * 1024;
inline constexpr std::uint32_t kMinSampleRate = 8000;
inline constexpr std::uint32_t kMaxSampleRate = 96000;

inline std::shared_ptr<audio::SampleBuffer> AllocateFrames(std::size_t frameCount)
{
  if (frameCount == 0 || frameCount > kMaxSampleFrames)
  {
    return {};
  }
  // In PSRAM; without it in internal RAM, keeping room for file IO, the event queues and the UI.
  constexpr std::size_t kFastReserveBytes = 32 * 1024;
  if (!memory::SlowRoom(frameCount * sizeof(std::int16_t), kSampleSlowReserveBytes, kFastReserveBytes))
  {
    return {};
  }
  std::unique_ptr<std::int16_t[]> frames(new (memory::slow) std::int16_t[frameCount]);
  if (!frames)
  {
    return {};
  }
  auto buffer = std::make_shared<audio::SampleBuffer>();
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
  auto loaded = info.frameCount <= RamSampleFrameLimit() ? AllocateFrames(info.frameCount) : nullptr;
  if (!loaded)
  {
    auto streamed = std::make_shared<audio::SampleBuffer>();
    streamed->frameCount = info.frameCount;
    streamed->stream = audio::SampleStream::Open(path, info.dataOffset, info.frameCount);
    if (!streamed->stream)
    {
      return false;
    }
    frames = std::move(streamed);
    sampleRate = info.sampleRate;
    return true;
  }
  file.seekg(static_cast<std::streamoff>(info.dataOffset));
  // Through a piece of internal RAM: the card's DMA cannot reach PSRAM, and read straight into it the
  // driver would fetch one sector per command.
  constexpr std::size_t kChunkBytes = 4096;
  std::unique_ptr<char[]> chunk(new (std::nothrow) char[kChunkBytes]);
  if (!chunk)
  {
    return false;
  }
  char* destination = reinterpret_cast<char*>(loaded->frames.get());
  for (std::size_t left = info.frameCount * sizeof(std::int16_t); left > 0;)
  {
    const auto count = static_cast<std::streamsize>(std::min(left, kChunkBytes));
    file.read(chunk.get(), count);
    if (!file || file.gcount() != count)
    {
      return false;
    }
    std::copy(chunk.get(), chunk.get() + count, destination);
    destination += count;
    left -= static_cast<std::size_t>(count);
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

inline constexpr std::size_t kWavHeaderBytes = 44;

inline void WriteMono16WavHeader(std::ostream& file, std::size_t frameCount, std::uint32_t sampleRate)
{
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
}

// Writes the file atomically; `source(chunk, offset, count)` delivers the frames piece by piece, so a
// file much longer than RAM can be written.
template <typename Source>
inline bool WriteMono16WavFrom(const std::string& path, std::size_t frameCount, std::uint32_t sampleRate, Source&& source)
{
  if (frameCount == 0 || frameCount > kMaxSampleFrames ||
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
  std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
  WriteMono16WavHeader(file, frameCount, sampleRate);
  std::array<std::int16_t, 512> chunk{};
  for (std::size_t offset = 0; offset < frameCount && file;)
  {
    const std::size_t count = std::min(chunk.size(), frameCount - offset);
    if (!source(chunk.data(), offset, count))
    {
      file.setstate(std::ios::failbit);
      break;
    }
    file.write(reinterpret_cast<const char*>(chunk.data()), static_cast<std::streamsize>(count * sizeof(std::int16_t)));
    offset += count;
  }
  file.flush();
  file.close();
  if (!file)
  {
    std::remove(temporary.c_str());
    return false;
  }

  const bool hadOriginal = std::ifstream(path, std::ios::binary).good();
  if (hadOriginal && std::rename(path.c_str(), backup.c_str()) != 0)
  {
    std::remove(temporary.c_str());
    return false;
  }
  if (std::rename(temporary.c_str(), path.c_str()) != 0)
  {
    if (hadOriginal)
    {
      // If rollback itself fails, leave the .bak intact for recovery.
      (void)std::rename(backup.c_str(), path.c_str());
    }
    std::remove(temporary.c_str());
    return false;
  }
  if (hadOriginal)
  {
    (void)std::remove(backup.c_str());
  }
  return true;
}

inline bool WriteMono16Wav(const std::string& path, const std::int16_t* frames,
                           std::size_t frameCount, std::uint32_t sampleRate)
{
  return frames != nullptr && WriteMono16WavFrom(path, frameCount, sampleRate, [frames](std::int16_t* chunk, std::size_t offset, std::size_t count) {
    std::copy_n(frames + offset, count, chunk);
    return true;
  });
}

}  // namespace synth::sampler

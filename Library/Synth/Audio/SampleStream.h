#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace synth::audio {

// A sample too long for internal RAM plays from its file on the card. A low-priority worker keeps a
// small cache of blocks filled: the blocks each voice is on and the next ones, and the first blocks
// after the start point, so a note begins without waiting for the card. The cache exists only while
// such a sample is loaded (16 KB for one, 32 KB at most) and the render thread never blocks on it:
// a block that is late plays as the last frame held, a short gap and nothing worse.
inline constexpr std::size_t kStreamBlockFrames = 1024;  // 2 KB, 23 ms at 44.1 kHz
inline constexpr std::size_t kMaxStreamReaders = 16;     // every sampler voice of every track

// What a voice remembers between frames.
struct StreamCursor {
  const std::int16_t* block = nullptr;
  std::uint32_t blockIndex = 0;
  std::int16_t last = 0;
};

class SampleStream {
public:
  // Null without memory for the cache or with every stream slot in use.
  static std::shared_ptr<SampleStream> Open(const std::string& path, std::uint64_t dataOffset, std::size_t frameCount);
  ~SampleStream();
  SampleStream(const SampleStream&) = delete;
  SampleStream& operator=(const SampleStream&) = delete;

  std::size_t FrameCount() const { return frameCount_; }

  // Render thread: never blocks.
  std::int16_t Read(StreamCursor& cursor, std::size_t frame) const
  {
    const auto index = static_cast<std::uint32_t>(frame / kStreamBlockFrames);
    if ((cursor.block == nullptr || cursor.blockIndex != index) && !FindBlock(cursor, index)) return cursor.last;
    cursor.last = cursor.block[frame % kStreamBlockFrames];
    return cursor.last;
  }
  // Render thread, once per audio block: where notes start and where each voice is (null: nowhere).
  void PublishStart(std::size_t frame) const;
  static void PublishReader(std::size_t reader, const SampleStream* stream, std::size_t frame);

  // Reads frames right now, for a waveform or a copy; false when the card fails.
  bool ReadFrames(std::size_t frame, std::int16_t* destination, std::size_t count) const;

private:
  SampleStream(std::size_t slot, std::uint32_t id, std::size_t frameCount) : slot_(slot), id_(id), frameCount_(frameCount) {}
  bool FindBlock(StreamCursor& cursor, std::uint32_t index) const;

  std::size_t slot_;
  std::uint32_t id_;
  std::size_t frameCount_;
};

// The card is back: streams that gave up on a failed read try again.
void RetrySampleStreams();

// While it lives the file is not open for streaming and no stream reads it, so it can be renamed,
// replaced or deleted (FAT refuses that for an open file).
class SampleFileGuard {
public:
  explicit SampleFileGuard(const std::string& path);
  ~SampleFileGuard();
  SampleFileGuard(const SampleFileGuard&) = delete;
  SampleFileGuard& operator=(const SampleFileGuard&) = delete;

  // The file now has this name: streams follow it.
  void MovedTo(const std::string& path);

private:
  std::string path_;
};

}  // namespace synth::audio

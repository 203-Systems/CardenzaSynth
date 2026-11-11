#pragma once

#include <atomic>
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
inline constexpr std::size_t kMaxStreamReaders = 40;     // every sampler voice of every track (6 x 6)

// A block's key in 32 bits: the stream's id above, the block's index + 1 below. 32-bit keys because a
// 64-bit atomic is a library call on the board.
inline constexpr unsigned kKeyBlockBits = 15;
inline constexpr std::uint32_t kKeyBlockMask = (1u << kKeyBlockBits) - 1;
inline constexpr std::uint32_t kMaxStreamId = (1u << (32 - kKeyBlockBits)) - 1;

inline std::uint32_t BlockKey(std::uint32_t id, std::uint32_t block) { return (id << kKeyBlockBits) | (block + 1); }
inline std::uint32_t KeyStream(std::uint32_t key) { return key >> kKeyBlockBits; }
inline std::uint32_t KeyBlock(std::uint32_t key) { return (key & kKeyBlockMask) - 1; }

// What a voice remembers between frames.
struct StreamCursor {
  const std::int16_t* block = nullptr;
  // The cache's word for that block. It is read on every access: a block can be taken back for another
  // stream while a voice is inside it, and its memory then holds somebody else's audio - a burst of
  // noise a third stream is enough to provoke, because the cache is shared between all of them.
  const std::atomic<std::uint32_t>* key = nullptr;
  std::uint32_t blockIndex = 0;
  std::int16_t last = 0;
  std::uint16_t retryIn = 0;  // the block is not there yet: reads before the cache is looked through again
};

// A block that is not there yet is looked for again after this many reads, not on every one: the
// cache is looked through with 64-bit atomics, library calls on this chip, and doing that for every
// frame took a whole block's time with a few voices waiting for the card (the stress test,
// 2026-11-04). About one audio block (a voice reads two frames per sample).
inline constexpr std::uint16_t kStreamMissRetryReads = 256;

// Blocks the cache took back while a voice was inside one: each is a moment of another stream's audio,
// so it belongs at zero. Watched from the web page, where a shared cache is easiest to overrun.
extern std::atomic<std::uint32_t> g_streamBlockLosses;

// What the cache is doing right now, for the log the firmware keeps on the card: how many samples
// stream, how many blocks the heap let the cache have, and how many were taken back under a voice.
struct StreamStats {
  std::size_t streams = 0;
  std::size_t blocks = 0;
  std::uint32_t losses = 0;
};
StreamStats SampleStreamStats();

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
    const std::uint32_t key = BlockKey(id_, index);
    const bool had = cursor.block != nullptr && cursor.blockIndex == index;
    if (had && (cursor.key == nullptr || cursor.key->load(std::memory_order_acquire) != key))
      g_streamBlockLosses.fetch_add(1, std::memory_order_relaxed);
    if (!had || cursor.key == nullptr || cursor.key->load(std::memory_order_acquire) != key)
    {
      if (cursor.block == nullptr && cursor.blockIndex == index && cursor.retryIn > 0)
      {
        --cursor.retryIn;
        return cursor.last;
      }
      if (!FindBlock(cursor, index))
      {
        cursor.retryIn = kStreamMissRetryReads;
        return cursor.last;
      }
    }
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
// A voice is playing from the card right now.
bool SampleStreamsReading();

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

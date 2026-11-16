#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace synth::audio {

// A sample too long for RAM plays from its file on the card. A worker task keeps a cache of blocks
// filled: about half a second ahead of every voice at the voice's own speed, and the first two
// blocks of every sample's start, so a note begins without waiting for the card. Blocks are 8 KB and
// lie on the file's sector grid, so each is one card command. The cache is in PSRAM (64 blocks,
// 512 KB at most; four voices take about 300 KB) and the card reads into one 8 KB block of internal
// RAM first (its DMA reaches nothing else); none of it exists while no such sample is loaded. The
// render thread never waits for it: a block that is late plays as the last frame held.
inline constexpr std::size_t kStreamBlockFrames = 4096;  // 8 KB, 93 ms at 44.1 kHz
inline constexpr std::size_t kMaxStreamReaders = 16;     // every sampler voice of every track

namespace detail {
// The render pass under way (BeginStreamReads). A voice looks its block up again in every pass: that
// is what lets the worker reuse a block once the pass that may still read it is over, without an
// atomic per sample.
inline std::uint32_t g_streamReadPass = 1;
}  // namespace detail

// What a voice remembers between frames.
struct StreamCursor {
  const std::int16_t* block = nullptr;
  std::uint32_t blockIndex = ~0u;
  std::uint32_t pass = 0;         // the render pass `block` was found in
  std::uint32_t lateIndex = ~0u;  // the last block this voice found missing (counted once)
  std::int16_t last = 0;
  std::uint16_t retryIn = 0;      // the block is not there yet: reads before the cache is looked through again
};

// A block that is not there yet is looked for again after this many reads, not on every one: looking
// through the cache is 64 loads, and a voice reads two frames per sample. About one audio block.
inline constexpr std::uint16_t kStreamMissRetryReads = 256;

class SampleStream {
public:
  // Null without memory for the cache or with every stream slot in use. Streams of the same file
  // (two tracks with one sample) share its blocks and its file slot.
  static std::shared_ptr<SampleStream> Open(const std::string& path, std::uint64_t dataOffset, std::size_t frameCount);
  ~SampleStream();
  SampleStream(const SampleStream&) = delete;
  SampleStream& operator=(const SampleStream&) = delete;

  std::size_t FrameCount() const { return frameCount_; }

  // Render thread, inside a render pass (StreamReadPass; the audio runtime holds one around every
  // block it renders): never blocks.
  std::int16_t Read(StreamCursor& cursor, std::size_t frame) const
  {
    const std::size_t at = frame + leadFrames_;
    const auto index = static_cast<std::uint32_t>(at / kStreamBlockFrames);
    if (cursor.blockIndex != index || cursor.pass != detail::g_streamReadPass || cursor.block == nullptr)
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
    cursor.last = cursor.block[at % kStreamBlockFrames];
    return cursor.last;
  }
  // Render thread, once per audio block: where notes start, where they end and whether they loop
  // there (the look-ahead wraps), and where each voice is and how fast it moves (frames of the sample
  // per output frame: the look-ahead covers the same time at any speed). Null: the reader plays
  // nothing. A stream played from several places (one sample on two tracks shares its stream) takes
  // a region from each, up to four; a region not published for a few blocks is dropped.
  void PublishRegion(std::size_t start, std::size_t end, bool loop) const;
  void PublishStart(std::size_t frame) const { PublishRegion(frame, frameCount_, false); }
  static void PublishReader(std::size_t reader, const SampleStream* stream, std::size_t frame, float rate = 1.0f);

  // Reads frames right now, for a waveform or a copy; false when the card fails.
  bool ReadFrames(std::size_t frame, std::int16_t* destination, std::size_t count) const;

private:
  SampleStream(std::size_t slot, std::uint32_t id, std::uint32_t fileId, std::size_t frameCount, std::uint32_t leadFrames)
      : slot_(slot), id_(id), fileId_(fileId), frameCount_(frameCount), leadFrames_(leadFrames)
  {
  }
  bool FindBlock(StreamCursor& cursor, std::uint32_t index) const;

  std::size_t slot_;
  std::uint32_t id_;
  std::uint32_t fileId_;
  std::size_t frameCount_;
  std::uint32_t leadFrames_;  // what of the file comes before frame 0 in its first block (the header), in frames
};

// A render pass: every Read happens between these, on the thread that renders. The worker reuses a
// block only once the pass that may still have been reading it is over.
void BeginStreamReads();
void EndStreamReads();
struct StreamReadPass {
  StreamReadPass() { BeginStreamReads(); }
  ~StreamReadPass() { EndStreamReads(); }
  StreamReadPass(const StreamReadPass&) = delete;
  StreamReadPass& operator=(const StreamReadPass&) = delete;
};

// The card is back: streams that gave up on failed reads try again.
void RetrySampleStreams();
// A voice is playing from the card right now.
bool SampleStreamsReading();

// What streaming has done so far, for the log and the stress test.
struct StreamCounters {
  std::uint32_t reads = 0;              // blocks read from the card
  std::uint32_t lateBlocks = 0;         // a voice reached a block that was not there yet (it held its last frame)
  std::uint32_t opens = 0;              // files opened: more files playing than the two file slots means reopening
  std::uint32_t failures = 0;           // reads and opens that failed
  std::uint32_t slowestReadMicros = 0;  // since the last reset
  std::uint32_t cacheBlocks = 0;        // the cache now, 8 KB each
};
StreamCounters SampleStreamCounters(bool resetSlowest = false);

// While it lives the file is not open for streaming and no stream reads it, so it can be renamed,
// replaced or deleted (FAT refuses that for an open file). Streams of other files go on.
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

#if !defined(ESP_PLATFORM)
namespace testing {
// A slow card for host tests: every block read and every file open takes at least this long.
void SetCardDelays(std::uint32_t readMicros, std::uint32_t openMicros);
// At most this many blocks in the cache (0: as on the board).
void LimitCacheBlocks(std::size_t blocks);
}  // namespace testing
#endif

}  // namespace synth::audio

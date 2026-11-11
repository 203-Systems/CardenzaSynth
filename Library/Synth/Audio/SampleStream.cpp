#include "Library/Synth/Audio/SampleStream.h"

#include "Library/Memory/Memory.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <new>

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#elif !defined(__EMSCRIPTEN__)
#include <condition_variable>
#include <thread>
#endif

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace synth::audio {

// Blocks the cache took back while a voice was inside one (SampleStream.h): a moment of another
// stream's audio, so it belongs at zero. Declared here, outside the pool's own namespace, so the
// render path and the web page can read it.
std::atomic<std::uint32_t> g_streamBlockLosses{0};

namespace {

constexpr std::size_t kMaxStreams = 8;       // four tracks, plus a project being loaded over them
constexpr std::size_t kMaxOpenFiles = 3;     // the card driver has five file slots for everything
// Twenty blocks a stream is 460 ms of look-ahead: an 8 KB block is 93 ms, so twelve cover it and the
// cache still fits four streams in 384 KB of PSRAM.
constexpr std::size_t kBlocksPerStream = 12;
// The card still reads through internal RAM (the driver's DMA cannot address PSRAM), so one scratch
// block there takes each read and the worker copies it across: two kilobytes, whatever the cache size.
#if defined(ESP_PLATFORM)
constexpr std::size_t kMaxBlocks = 80;       // 640 KB of PSRAM at most, six streams deep
#else
constexpr std::size_t kMaxBlocks = 40;
#endif
constexpr std::size_t kMinBlocks = 4;
// Blocks fetched in one round of the worker. A round is a few milliseconds of the card; the UI shares
// this core and needs the rest of the time.
#if defined(ESP_PLATFORM)
constexpr std::size_t kReadsPerPump = 3;
#else
constexpr std::size_t kReadsPerPump = 64;  // no threads to come back in: finish the job here
#endif
// PSRAM left for patterns, projects and the rest of the app before the cache stops growing.
constexpr std::size_t kSlowReserveBytes = 512 * 1024;

struct Block {
  std::atomic<std::uint32_t> key{0};  // BlockKey, 0 = empty
  std::uint32_t lastUse = 0;
  std::int16_t* frames = nullptr;     // PSRAM: kStreamBlockFrames of them
};

struct StreamState {
  std::atomic<std::uint32_t> id{0};  // 0 = free
  std::atomic<bool> released{false};
  std::atomic<std::uint32_t> startBlock{0};
  std::string path;
  std::uint64_t dataOffset = 0;
  std::size_t frameCount = 0;
  std::FILE* file = nullptr;
  std::uint32_t lastUse = 0;
  bool failed = false;
};

// A block's key in 32 bits: the stream's id above, the block's index + 1 below (BlockKey and
// friends are in the header - a voice checks its block against them on every sample it reads).
// Deliberately never destroyed: the worker runs for as long as the program.
struct Pool {
  std::recursive_mutex io;
  std::array<StreamState, kMaxStreams> streams{};
  std::array<std::atomic<Block*>, kMaxBlocks> blocks{};
  std::atomic<std::size_t> blockCount{0};
  std::array<std::atomic<std::uint32_t>, kMaxStreamReaders> readers{};
  // The one block the card reads into: internal RAM, because its driver's DMA cannot address PSRAM.
  // Each filled block is copied from here into its PSRAM home (2 KB a block, 43 times a second a
  // stream - nothing next to the render).
  std::array<std::int16_t, kStreamBlockFrames> scratch{};
  std::uint32_t nextId = 1;
  std::uint32_t clock = 0;
#if defined(ESP_PLATFORM)
  TaskHandle_t worker = nullptr;
#elif !defined(__EMSCRIPTEN__)
  std::mutex wakeMutex;
  std::condition_variable wake;
  bool woken = false;
  bool workerStarted = false;
#endif
};

Pool& ThePool()
{
  static Pool* pool = new Pool;
  return *pool;
}

void Wake(Pool& pool)
{
#if defined(ESP_PLATFORM)
  if (pool.worker != nullptr) xTaskNotifyGive(pool.worker);
#elif !defined(__EMSCRIPTEN__)
  {
    std::lock_guard<std::mutex> lock(pool.wakeMutex);
    pool.woken = true;
  }
  pool.wake.notify_one();
#else
  (void)pool;
#endif
}

bool SamePath(const std::string& left, const std::string& right)
{
  // Firmware storage is FAT: names differ only in case and still are one file.
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a, unsigned char b) {
    return std::tolower(a) == std::tolower(b);
  });
}

void CloseFile(StreamState& stream)
{
  if (stream.file == nullptr) return;
  std::fclose(stream.file);
  stream.file = nullptr;
}

bool EnsureOpen(Pool& pool, StreamState& stream)
{
  if (stream.file != nullptr) return true;
  if (stream.failed) return false;
  std::size_t open = 0;
  StreamState* oldest = nullptr;
  for (StreamState& other : pool.streams)
  {
    if (other.file == nullptr) continue;
    ++open;
    if (oldest == nullptr || other.lastUse < oldest->lastUse) oldest = &other;
  }
  if (open >= kMaxOpenFiles && oldest != nullptr) CloseFile(*oldest);
  stream.file = std::fopen(stream.path.c_str(), "rb");
  if (stream.file == nullptr)
  {
    stream.failed = true;
    return false;
  }
  // Read through its descriptor (ReadBytes): a stdio buffer would only cost memory, and none may hold
  // data the descriptor's position has moved past.
  std::setvbuf(stream.file, nullptr, _IONBF, 0);
  return true;
}

// Through the file's descriptor: newlib reads an unbuffered FILE a byte at a time, every byte a VFS and
// FATFS call. A 2 KB block took 33 ms on the board and the stream worker, above the UI on its core,
// kept 76% of it while a sample played from the card (the stress test, 2026-11-04); read() takes the
// block in one FATFS call, its whole sectors in one card command: 1.8 ms.
bool ReadBytes(std::FILE* file, long offset, void* destination, std::size_t bytes)
{
#if defined(_WIN32)
  const int descriptor = _fileno(file);
  if (_lseek(descriptor, offset, SEEK_SET) != offset) return false;
#else
  const int descriptor = fileno(file);
  if (::lseek(descriptor, offset, SEEK_SET) != offset) return false;
#endif
  auto* cursor = static_cast<char*>(destination);
  while (bytes > 0)
  {
#if defined(_WIN32)
    const int got = _read(descriptor, cursor, static_cast<unsigned>(bytes));
#else
    const ssize_t got = ::read(descriptor, cursor, bytes);
#endif
    if (got <= 0) return false;
    cursor += got;
    bytes -= static_cast<std::size_t>(got);
  }
  return true;
}

bool ReadFromFile(Pool& pool, StreamState& stream, std::size_t frame, std::int16_t* destination, std::size_t count)
{
  if (!EnsureOpen(pool, stream)) return false;
  stream.lastUse = ++pool.clock;
  const auto offset = static_cast<long>(stream.dataOffset + frame * sizeof(std::int16_t));
  if (!ReadBytes(stream.file, offset, destination, count * sizeof(std::int16_t)))
  {
    // A read that fails once is not a stream that is gone: the block stays empty and the next round
    // asks the card again. Giving up for good here is what turned one bad read into a track that
    // never made another sound until it was reloaded.
    CloseFile(stream);
    return false;
  }
  return true;
}

void FreeBlock(Block* block)
{
  if (block == nullptr) return;
  memory::FreeSlowArray(block->frames);
  delete block;
}

void GrowCache(Pool& pool, std::size_t target)
{
  std::size_t count = pool.blockCount.load(std::memory_order_relaxed);
  target = std::min(target, kMaxBlocks);
  while (count < target)
  {
    // PSRAM, where the look-ahead can be long: the block's frames are the only thing worth reserving
    // there, and each one is 2 KB.
    if (!memory::SlowRoom(kStreamBlockFrames * sizeof(std::int16_t), kSlowReserveBytes)) break;
    Block* block = new (std::nothrow) Block;
    if (block == nullptr) break;
    block->frames = static_cast<std::int16_t*>(memory::AllocateSlowArray(kStreamBlockFrames * sizeof(std::int16_t)));
    if (block->frames == nullptr)
    {
      delete block;
      break;
    }
    std::fill(block->frames, block->frames + kStreamBlockFrames, std::int16_t{0});
    pool.blocks[count].store(block, std::memory_order_relaxed);
    pool.blockCount.store(++count, std::memory_order_release);
  }
}

// One round of the worker: retire released streams, then bring in what the voices need next. Only a
// few blocks a round: the worker sits above the UI on this core, and five streams' look-ahead is
// nearly half a megabyte - fetched in one go that is half a second with the screen frozen and the
// keys dead. Returns whether more is waiting, so the caller can come back after a tick.
bool Pump(Pool& pool)
{
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  std::size_t live = 0;
  for (StreamState& stream : pool.streams)
  {
    const std::uint32_t id = stream.id.load(std::memory_order_acquire);
    if (id == 0) continue;
    if (!stream.released.load(std::memory_order_acquire))
    {
      ++live;
      continue;
    }
    CloseFile(stream);
    const std::size_t count = pool.blockCount.load(std::memory_order_relaxed);
    for (std::size_t index = 0; index < count; ++index)
    {
      Block* block = pool.blocks[index].load(std::memory_order_relaxed);
      if (KeyStream(block->key.load(std::memory_order_relaxed)) == id) block->key.store(0, std::memory_order_release);
    }
    stream.path.clear();
    stream.failed = false;
    stream.released.store(false, std::memory_order_relaxed);
    stream.id.store(0, std::memory_order_release);
  }
  if (live == 0)
  {
    // Nothing plays from the card any more: the memory goes back.
    const std::size_t count = pool.blockCount.exchange(0, std::memory_order_acq_rel);
    for (std::size_t index = 0; index < count; ++index) FreeBlock(pool.blocks[index].exchange(nullptr, std::memory_order_acq_rel));
    return false;
  }
  GrowCache(pool, live * kBlocksPerStream);

  auto stateOf = [&](std::uint32_t id) -> StreamState* {
    for (StreamState& stream : pool.streams)
      if (stream.id.load(std::memory_order_relaxed) == id && !stream.released.load(std::memory_order_relaxed)) return &stream;
    return nullptr;
  };
  // What is needed, most urgent first: each voice's block and the next, the start of every sample,
  // then one more ahead for each voice. A wanted block is never evicted.
  // Sized for every stream's whole budget plus its start: the look-ahead below asks for that many.
  std::array<std::uint32_t, kMaxStreams * (kBlocksPerStream + 2)> needs{};
  std::size_t needCount = 0;
  auto need = [&](std::uint32_t id, std::uint32_t block) {
    const StreamState* stream = stateOf(id);
    if (stream == nullptr || block * kStreamBlockFrames >= stream->frameCount) return;
    const std::uint32_t key = BlockKey(id, block);
    if (std::find(needs.begin(), needs.begin() + needCount, key) == needs.begin() + needCount && needCount < needs.size()) needs[needCount++] = key;
  };
  // What is needed, most urgent first: for every voice, the block it is in and the ones behind it.
  //
  // A stream reads forward from each of its voices, and the look-ahead is shared between them - one
  // voice gets the whole window, four get a quarter each. Reading forward from the earliest voice
  // alone left a chop's other slices waiting for blocks nobody had asked for (they held their last
  // frame: a slice that never sounds), and asking for the window *plus* the sample's start asked for
  // fourteen blocks where the cache holds twelve, so the cache churned: filled, evicted, filled.
  for (const StreamState& stream : pool.streams)
  {
    const std::uint32_t id = stream.id.load(std::memory_order_relaxed);
    if (id == 0 || stream.released.load(std::memory_order_relaxed)) continue;
    const std::uint32_t start = stream.startBlock.load(std::memory_order_relaxed);
    std::array<std::uint32_t, kMaxStreamReaders> heads{};
    std::size_t voices = 0;
    for (const auto& reader : pool.readers)
    {
      const std::uint32_t key = reader.load(std::memory_order_acquire);
      if (key == 0 || KeyStream(key) != id) continue;
      if (voices < heads.size()) heads[voices++] = KeyBlock(key);
    }
    if (voices == 0)
    {
      // Open but not playing: nothing has a position yet, so the start is what will be wanted.
      for (std::uint32_t ahead = 0; ahead < kBlocksPerStream; ++ahead) need(id, start + ahead);
      continue;
    }
    // Two blocks a voice at the very least: a slice needs its block before the next arrives, and six
    // voices sharing twelve leaves no room for a deeper window than that.
    const std::size_t depth = (std::max)(std::size_t{2}, kBlocksPerStream / voices);
    for (std::size_t voice = 0; voice < voices; ++voice)
      for (std::uint32_t ahead = 0; ahead < depth; ++ahead) need(id, heads[voice] + ahead);
  }

  const std::size_t count = pool.blockCount.load(std::memory_order_relaxed);
  auto isNeeded = [&](std::uint32_t key) { return std::find(needs.begin(), needs.begin() + needCount, key) != needs.begin() + needCount; };
  std::size_t reads = 0;
  for (std::size_t index = 0; index < needCount; ++index)
  {
    const std::uint32_t key = needs[index];
    Block* victim = nullptr;
    bool cached = false;
    for (std::size_t blockIndex = 0; blockIndex < count; ++blockIndex)
    {
      Block* block = pool.blocks[blockIndex].load(std::memory_order_relaxed);
      const std::uint32_t held = block->key.load(std::memory_order_relaxed);
      if (held == key)
      {
        block->lastUse = ++pool.clock;
        cached = true;
        break;
      }
      if (isNeeded(held)) continue;
      if (victim == nullptr || held == 0 || (victim->key.load(std::memory_order_relaxed) != 0 && block->lastUse < victim->lastUse)) victim = block;
    }
    if (cached) continue;
    if (victim == nullptr) break;  // every block is in use: the rest waits
    StreamState* stream = stateOf(KeyStream(key));
    if (stream == nullptr || stream->failed) continue;
    const auto blockNumber = static_cast<std::size_t>(KeyBlock(key));
    const std::size_t firstFrame = blockNumber * kStreamBlockFrames;
    const std::size_t frames = std::min(kStreamBlockFrames, stream->frameCount - firstFrame);
    victim->key.store(0, std::memory_order_release);
    // The card fills the scratch block in internal RAM, then it is copied to the block's PSRAM home.
    if (!ReadFromFile(pool, *stream, firstFrame, pool.scratch.data(), frames)) continue;
    std::copy(pool.scratch.begin(), pool.scratch.begin() + static_cast<std::ptrdiff_t>(frames), victim->frames);
    std::fill(victim->frames + frames, victim->frames + kStreamBlockFrames, std::int16_t{0});
    victim->lastUse = ++pool.clock;
    victim->key.store(key, std::memory_order_release);
    if (++reads >= kReadsPerPump) return index + 1 < needCount;
  }
  return false;
}

#if defined(ESP_PLATFORM)
void WorkerTask(void* context)
{
  Pool& pool = *static_cast<Pool*>(context);
  for (;;)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // A tick between rounds: three blocks are 24 KB, a few milliseconds of the card, and the UI on
    // this core needs the rest of the time.
    while (Pump(pool)) vTaskDelay(1);
  }
}
#endif

void EnsureWorker(Pool& pool)
{
#if defined(ESP_PLATFORM)
  // Above the UI, which also runs on this core: voices cannot wait for a frame to finish drawing.
  if (pool.worker == nullptr) xTaskCreatePinnedToCore(WorkerTask, "sample_stream", 6144, &pool, 6, &pool.worker, 0);
#elif !defined(__EMSCRIPTEN__)
  if (pool.workerStarted) return;
  pool.workerStarted = true;
  std::thread([&pool] {
    for (;;)
    {
      {
        std::unique_lock<std::mutex> lock(pool.wakeMutex);
        pool.wake.wait(lock, [&pool] { return pool.woken; });
        pool.woken = false;
      }
      (void)Pump(pool);
    }
  }).detach();
#else
  (void)pool;
#endif
}

}  // namespace

std::shared_ptr<SampleStream> SampleStream::Open(const std::string& path, std::uint64_t dataOffset, std::size_t frameCount)
{
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
#if defined(__EMSCRIPTEN__)
  (void)Pump(pool);
#endif
  std::size_t live = 1;
  StreamState* free = nullptr;
  std::size_t slot = 0;
  for (std::size_t index = 0; index < pool.streams.size(); ++index)
  {
    StreamState& stream = pool.streams[index];
    if (stream.id.load(std::memory_order_relaxed) != 0) ++live;
    else if (free == nullptr)
    {
      free = &stream;
      slot = index;
    }
  }
  // Longer than a key can number: not from the card (no sample here is that long).
  if (free == nullptr || frameCount == 0 || frameCount / kStreamBlockFrames + 1 > kKeyBlockMask) return nullptr;
  GrowCache(pool, live * kBlocksPerStream);
  if (pool.blockCount.load(std::memory_order_relaxed) < kMinBlocks) return nullptr;
  EnsureWorker(pool);

  // Ids wrap within what a key holds, never 0 and never one a stream still has.
  std::uint32_t id = 0;
  do
  {
    id = pool.nextId;
    pool.nextId = pool.nextId >= kMaxStreamId ? 1 : pool.nextId + 1;
  } while (std::any_of(pool.streams.begin(), pool.streams.end(), [id](const StreamState& state) { return state.id.load(std::memory_order_relaxed) == id; }));
  free->path = path;
  free->dataOffset = dataOffset;
  free->frameCount = frameCount;
  free->failed = false;
  free->startBlock.store(0, std::memory_order_relaxed);
  std::shared_ptr<SampleStream> stream(new (std::nothrow) SampleStream(slot, id, frameCount));
  if (!stream) return nullptr;
  free->id.store(id, std::memory_order_release);
  Wake(pool);
#if defined(__EMSCRIPTEN__)
  (void)Pump(pool);
#endif
  return stream;
}

SampleStream::~SampleStream()
{
  // Possibly on the render thread: the worker does the closing.
  Pool& pool = ThePool();
  pool.streams[slot_].released.store(true, std::memory_order_release);
  Wake(pool);
#if defined(__EMSCRIPTEN__)
  (void)Pump(pool);
#endif
}

bool SampleStream::FindBlock(StreamCursor& cursor, std::uint32_t index) const
{
  Pool& pool = ThePool();
  const std::uint32_t key = BlockKey(id_, index);
  cursor.block = nullptr;
  cursor.key = nullptr;
  cursor.blockIndex = index;
#if defined(__EMSCRIPTEN__)
  // No threads in the browser, and its files are in memory: read on the spot.
  for (int attempt = 0; attempt < 2; ++attempt)
  {
#endif
    const std::size_t count = pool.blockCount.load(std::memory_order_acquire);
    for (std::size_t blockIndex = 0; blockIndex < count; ++blockIndex)
    {
      const Block* block = pool.blocks[blockIndex].load(std::memory_order_acquire);
      if (block != nullptr && block->key.load(std::memory_order_acquire) == key)
      {
        cursor.block = block->frames;
        cursor.key = &block->key;
        return true;
      }
    }
#if defined(__EMSCRIPTEN__)
    PublishReader(0, this, index * kStreamBlockFrames);
    (void)Pump(pool);
  }
#endif
  return false;
}

void SampleStream::PublishStart(std::size_t frame) const
{
  Pool& pool = ThePool();
  const auto block = static_cast<std::uint32_t>(frame / kStreamBlockFrames);
  if (pool.streams[slot_].startBlock.exchange(block, std::memory_order_relaxed) != block) Wake(pool);
}

void SampleStream::PublishReader(std::size_t reader, const SampleStream* stream, std::size_t frame)
{
  Pool& pool = ThePool();
  if (reader >= pool.readers.size()) return;
  const std::uint32_t key = stream != nullptr ? BlockKey(stream->id_, static_cast<std::uint32_t>(frame / kStreamBlockFrames)) : 0;
  if (pool.readers[reader].exchange(key, std::memory_order_acq_rel) != key && key != 0) Wake(pool);
}

bool SampleStream::ReadFrames(std::size_t frame, std::int16_t* destination, std::size_t count) const
{
  if (frame + count > frameCount_) return false;
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  return ReadFromFile(pool, pool.streams[slot_], frame, destination, count);
}

bool SampleStreamsReading()
{
  const Pool& pool = ThePool();
  return std::any_of(pool.readers.begin(), pool.readers.end(), [](const std::atomic<std::uint32_t>& reader) { return reader.load(std::memory_order_relaxed) != 0; });
}

StreamStats SampleStreamStats()
{
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  StreamStats stats;
  for (const StreamState& stream : pool.streams)
    if (stream.id.load(std::memory_order_relaxed) != 0 && !stream.released.load(std::memory_order_relaxed)) ++stats.streams;
  stats.blocks = pool.blockCount.load(std::memory_order_relaxed);
  stats.losses = g_streamBlockLosses.load(std::memory_order_relaxed);
  return stats;
}

void RetrySampleStreams()
{
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  for (StreamState& stream : pool.streams) stream.failed = false;
  Wake(pool);
}

SampleFileGuard::SampleFileGuard(const std::string& path) : path_(path)
{
  Pool& pool = ThePool();
  pool.io.lock();
  for (StreamState& stream : pool.streams)
    if (stream.id.load(std::memory_order_relaxed) != 0 && SamePath(stream.path, path_)) CloseFile(stream);
}

SampleFileGuard::~SampleFileGuard()
{
  ThePool().io.unlock();
}

void SampleFileGuard::MovedTo(const std::string& path)
{
  for (StreamState& stream : ThePool().streams)
    if (stream.id.load(std::memory_order_relaxed) != 0 && SamePath(stream.path, path_)) stream.path = path;
  path_ = path;
}

}  // namespace synth::audio

#include "Library/Synth/Audio/SampleStream.h"

#include "Library/Memory/Memory.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <vector>

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

namespace {

// How it works.
//
// A file is cut into blocks of 8 KB on its own byte grid (block 0 starts with the WAV header), so a
// block is whole sectors, inside one cluster, and the card reads it with one command: blocks cut at
// the data chunk's offset (44) made every read three commands, two of them for a partial sector.
// The card costs about a millisecond and a half per command whatever its size; 8 KB at 1x is 93 ms
// of sound, so four voices read 43 times a second.
//
// What to read is planned from where the voices are (published once per audio block) and how fast
// they move: each voice's window is the block it is in and enough after it for half a second at its
// speed, wrapping at the loop end; each stream also keeps the first two blocks of its start resident,
// so a new note sounds at once (186 ms) while its window is read. The windows are fitted to the
// cache. A voice is served when fewer than half its window is there, which batches the reads of one
// file: with more files playing than the two file slots, every switch can mean reopening one (a
// directory lookup and a walk along its cluster chain). The most urgent file comes first; a visit
// reads at most eight blocks before it plans again.
//
// A block a voice may be reading is never overwritten. The voices look their block up again in
// every render pass (Read compares the pass number, a plain load); the worker takes a block away by
// zeroing its key, and writes into it only after the pass that may still have been reading it is
// over (the pass counter is odd during a pass; seq_cst fences on both sides decide which saw the
// other). No atomic per sample, and nothing the render thread does waits.
//
// Memory: the blocks are in PSRAM, read by the voices in order like a sample kept in RAM; the card
// reads into one staging block of internal RAM (its DMA reaches nothing else; into PSRAM its driver
// fetches a sector per command) and the worker copies it across. The worker's plan is in PSRAM too.
// All of it exists only while a streamed sample is loaded. A board without PSRAM reads straight
// into four blocks of internal RAM.

constexpr std::size_t kMaxStreams = 8;      // four tracks, plus a project being loaded over them
constexpr std::size_t kMaxFiles = kMaxStreams;
constexpr std::size_t kMaxOpenFiles = 2;    // the card driver has five file slots for everything
constexpr std::size_t kBlockBytes = kStreamBlockFrames * sizeof(std::int16_t);
constexpr std::size_t kMaxSlots = 64;       // 512 KB of PSRAM
constexpr std::size_t kMaxDirectSlots = 4;  // without PSRAM: 32 KB of internal RAM, as much as before
constexpr std::size_t kMinSlots = 4;
constexpr std::size_t kSlackSlots = 4;      // kept above what the plan asks for, against churn
// PSRAM the cache leaves to patterns, snapshots and samples (they keep 256 KB each for themselves).
constexpr std::size_t kSlowReserveBytes = 512 * 1024;
constexpr std::size_t kSlowFirstReserveBytes = 256 * 1024;  // for the first blocks, which a sample needs to load
constexpr std::size_t kStagingFloorBytes = 32 * 1024;
constexpr std::size_t kDirectFloorBytes = 40 * 1024;
constexpr std::uint32_t kAheadFrames = 22050;  // each voice's look-ahead: half a second of output
constexpr std::size_t kMaxAhead = 12;          // blocks after the voice's own
constexpr std::uint32_t kStartDeadlineFrames = 4410;
constexpr std::size_t kMaxReadsPerVisit = 8;
constexpr std::uint32_t kUrgentFrames = 8820;  // what is due within 0.2 s does not wait for a tick
constexpr std::uint32_t kRateOne = 1u << 16;
constexpr std::uint32_t kIdleSlotMs = 2000;
constexpr std::uint8_t kFailuresBeforeSilence = 3;
constexpr std::uint32_t kRetryMs = 250;
constexpr std::uint32_t kGraceWaitMs = 100;
// A stream may be played from several places at once: the same sample loaded on two tracks shares one
// stream (a project load prepares it once), each track with its own trim and loop. Each publishes its
// region every audio block; a region nobody has published for a few blocks is gone.
constexpr std::size_t kMaxRegions = 4;
constexpr std::uint32_t kRegionLoop = 1u << 31;  // in a region's end word
constexpr std::uint32_t kRegionLivePasses = 16;   // in pass counts: 8 render passes, 46 ms
constexpr std::size_t kMaxNeeds = kMaxStreamReaders * (kMaxAhead + 1) + kMaxStreams * kMaxRegions * 2;
constexpr std::uint32_t kNever = std::numeric_limits<std::uint32_t>::max();

// A block's key in 32 bits: its file's id above, the block's index + 1 below. Atomics of 64 bits are
// library calls on this chip (with PSRAM even a load is one), behind a single lock with interrupts off
// that both cores share: lookups with 64-bit keys kept the worker busy for seconds, above the UI on
// its core (the stress test, 2026-11-04). 32-bit loads and stores are plain instructions. Readers
// are published the same way with the stream's id.
constexpr unsigned kKeyBlockBits = 15;  // 32767 blocks, 50 minutes at 44.1 kHz
constexpr std::uint32_t kKeyBlockMask = (1u << kKeyBlockBits) - 1;
constexpr std::uint32_t kMaxId = (1u << (32 - kKeyBlockBits)) - 1;

std::uint32_t BlockKey(std::uint32_t id, std::uint32_t block) { return (id << kKeyBlockBits) | (block + 1); }
std::uint32_t KeyId(std::uint32_t key) { return key >> kKeyBlockBits; }
std::uint32_t KeyBlock(std::uint32_t key) { return (key & kKeyBlockMask) - 1; }

// What the render thread touches: static, so it never allocates and never takes a lock.
struct RegionShared {
  std::atomic<std::uint32_t> start{0};
  std::atomic<std::uint32_t> end{0};    // with kRegionLoop; 0 = unused
  std::atomic<std::uint32_t> stamp{0};  // the pass it was last published in
};

struct StreamShared {
  std::atomic<std::uint32_t> id{0};  // 0 = free
  std::atomic<bool> released{false};
  std::array<RegionShared, kMaxRegions> regions{};
};

struct Shared {
  std::array<std::atomic<std::uint32_t>, kMaxSlots> keys{};  // BlockKey, 0 = nothing (yet)
  std::array<std::int16_t*, kMaxSlots> frames{};             // null: the slot has no memory
  std::array<std::atomic<std::uint32_t>, kMaxStreamReaders> readers{};
  std::array<std::atomic<std::uint32_t>, kMaxStreamReaders> rates{};  // 16.16
  std::array<StreamShared, kMaxStreams> streams{};
  std::atomic<std::uint32_t> pass{0};  // odd during a render pass
  std::uint32_t passDepth = 0;         // the render thread's
  // Counters, each with one writer (the render thread for `late`, the worker for the rest).
  std::atomic<std::uint32_t> late{0};
  std::atomic<std::uint32_t> reads{0};
  std::atomic<std::uint32_t> opens{0};
  std::atomic<std::uint32_t> failures{0};
  std::atomic<std::uint32_t> slowestMicros{0};
  std::atomic<std::uint32_t> blocks{0};
#if defined(ESP_PLATFORM)
  std::atomic<TaskHandle_t> worker{nullptr};
#endif
};
Shared g_shared;

#if !defined(ESP_PLATFORM)
std::atomic<std::uint32_t> g_testReadMicros{0};
std::atomic<std::uint32_t> g_testOpenMicros{0};
std::atomic<std::size_t> g_testSlotLimit{0};
#endif

void Bump(std::atomic<std::uint32_t>& counter)
{
  counter.store(counter.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
}

struct FileState {
  std::uint32_t id = 0;  // 0 = free
  std::uint32_t streams = 0;
  std::string path;
  std::uint64_t dataOffset = 0;
  std::uint64_t dataEnd = 0;
  std::uint64_t base = 0;  // where block 0 starts in the file
  std::size_t frameCount = 0;
  std::uint32_t leadFrames = 0;
  std::uint32_t blockCount = 0;
  std::FILE* file = nullptr;
  std::uint32_t lastUse = 0;
  std::uint32_t retryAt = 0;
  std::uint8_t failures = 0;
  bool failed = false;     // silent until RetrySampleStreams
  bool shareable = true;   // false once a guard has had its path: what is there now may be another file
};

struct SlotInfo {
  std::uint32_t lastUseMs = 0;
  std::uint32_t retiredPass = 0;
  bool retiring = false;  // key taken away; free once GraceOver(retiredPass)
};

struct Need {
  std::uint32_t key = 0;
  std::uint32_t deadline = kNever;  // output frames until a voice gets there
  std::uint8_t file = 0;
  bool due = false;
  bool present = false;
};

struct Window {
  std::array<std::uint32_t, kMaxAhead + 1> keys{};
  std::uint8_t count = 0;
  std::uint8_t low = 0;  // fewer blocks after the voice's own than this: the voice is due
};

// The worker's (PSRAM, while something streams).
struct Work {
  std::array<SlotInfo, kMaxSlots> slots{};
  std::array<Need, kMaxNeeds> needs{};
  std::size_t needCount = 0;
  std::array<Window, kMaxStreamReaders> windows{};
  std::size_t windowCount = 0;
  std::array<std::uint32_t, kMaxFiles> urgency{};  // the soonest deadline of a missing block, per file
  std::size_t desired = 0;
};

struct VisitRead {
  std::uint8_t slot = 0;
  std::uint8_t file = 0;
  std::uint32_t fileId = 0;
  std::uint32_t key = 0;
  std::uint32_t deadline = 0;
};

// Deliberately never destroyed: the worker runs for as long as the program.
struct Pool {
  std::recursive_mutex io;
  std::array<FileState, kMaxFiles> files{};
  std::array<std::uint8_t, kMaxStreams> streamFile{};
  std::vector<std::string> guarded;
  std::int16_t* staging = nullptr;
  Work* work = nullptr;
  std::size_t slotCount = 0;
  bool staged = true;  // blocks in PSRAM, read through the staging block
  std::uint32_t nextFileId = 1;
  std::uint32_t nextStreamId = 1;
  std::uint32_t clock = 0;
#if !defined(ESP_PLATFORM) && !defined(__EMSCRIPTEN__)
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

std::uint32_t NowMs()
{
  return static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::uint32_t NowMicros()
{
  return static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool Reached(std::uint32_t now, std::uint32_t at) { return static_cast<std::int32_t>(now - at) >= 0; }

void Wake()
{
#if defined(ESP_PLATFORM)
  if (TaskHandle_t worker = g_shared.worker.load(std::memory_order_acquire)) xTaskNotifyGive(worker);
#elif !defined(__EMSCRIPTEN__)
  Pool& pool = ThePool();
  {
    std::lock_guard<std::mutex> lock(pool.wakeMutex);
    pool.woken = true;
  }
  pool.wake.notify_one();
#endif
}

void Nap()
{
#if defined(ESP_PLATFORM)
  vTaskDelay(1);
#elif !defined(__EMSCRIPTEN__)
  std::this_thread::sleep_for(std::chrono::microseconds(300));
#endif
}

#if !defined(ESP_PLATFORM)
void TestDelay(const std::atomic<std::uint32_t>& micros)
{
#if !defined(__EMSCRIPTEN__)
  if (const std::uint32_t wait = micros.load(std::memory_order_relaxed)) std::this_thread::sleep_for(std::chrono::microseconds(wait));
#else
  (void)micros;
#endif
}
#endif

bool SamePath(const std::string& left, const std::string& right)
{
  // Firmware storage is FAT: names differ only in case and still are one file.
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](unsigned char a, unsigned char b) {
    return std::tolower(a) == std::tolower(b);
  });
}

bool Guarded(const Pool& pool, const std::string& path)
{
  return std::any_of(pool.guarded.begin(), pool.guarded.end(), [&](const std::string& guarded) { return SamePath(guarded, path); });
}

std::uint32_t BlockOf(const FileState& file, std::size_t frame)
{
  return static_cast<std::uint32_t>((frame + file.leadFrames) / kStreamBlockFrames);
}

// ---------------------------------------------------------------------------------------------------
// Taking blocks away

// After keys were zeroed: the render pass under way now, if there is one, may still read those blocks.
std::uint32_t RetireStamp()
{
  std::atomic_thread_fence(std::memory_order_seq_cst);
#if defined(__EMSCRIPTEN__)
  // One thread: every voice looks its block up again, and no pass is reading meanwhile.
  ++detail::g_streamReadPass;
#endif
  return g_shared.pass.load(std::memory_order_acquire);
}

bool GraceOver(std::uint32_t stamp)
{
#if defined(__EMSCRIPTEN__)
  (void)stamp;
  return true;
#else
  return (stamp & 1u) == 0 || g_shared.pass.load(std::memory_order_acquire) != stamp;
#endif
}

void RetireSlots(Pool& pool, const std::uint8_t* slots, std::size_t count)
{
  if (count == 0 || pool.work == nullptr) return;
  for (std::size_t index = 0; index < count; ++index) g_shared.keys[slots[index]].store(0, std::memory_order_relaxed);
  const std::uint32_t stamp = RetireStamp();
  for (std::size_t index = 0; index < count; ++index)
  {
    SlotInfo& info = pool.work->slots[slots[index]];
    info.retiring = true;
    info.retiredPass = stamp;
  }
}

// A slot with memory, no key and no pass that may still read what it held.
bool SlotFree(Pool& pool, std::size_t slot)
{
  if (g_shared.frames[slot] == nullptr || g_shared.keys[slot].load(std::memory_order_relaxed) != 0) return false;
  SlotInfo& info = pool.work->slots[slot];
  if (info.retiring && !GraceOver(info.retiredPass)) return false;
  info.retiring = false;
  return true;
}

// ---------------------------------------------------------------------------------------------------
// Memory

std::int16_t* AllocateBlock(const Pool& pool)
{
  if (pool.staged)
  {
    const std::size_t reserve = pool.slotCount < kMinSlots ? kSlowFirstReserveBytes : kSlowReserveBytes;
    if (!memory::SlowRoom(kBlockBytes, reserve)) return nullptr;
    return static_cast<std::int16_t*>(memory::AllocateSlowArray(kBlockBytes));
  }
  return static_cast<std::int16_t*>(memory::AllocateDma(kBlockBytes, kDirectFloorBytes));
}

void FreeBlock(const Pool& pool, std::int16_t* frames)
{
  if (pool.staged) memory::FreeSlowArray(frames);
  else memory::FreeDma(frames);
}

std::size_t SlotLimit(const Pool& pool)
{
  std::size_t limit = pool.staged ? kMaxSlots : kMaxDirectSlots;
#if !defined(ESP_PLATFORM)
  if (const std::size_t test = g_testSlotLimit.load(std::memory_order_relaxed)) limit = std::min(limit, test);
#endif
  return limit;
}

// The staging block and the worker's plan. Decided once whether the blocks go to PSRAM.
bool EnsureWork(Pool& pool)
{
  if (pool.work == nullptr)
  {
#if defined(ESP_PLATFORM)
    pool.staged = memory::HasSlow();
#else
    pool.staged = true;  // as on the board, so the tests run the copy
#endif
    pool.work = new (memory::slow) Work;
    if (pool.work == nullptr) return false;
  }
  if (pool.staged && pool.staging == nullptr)
    pool.staging = static_cast<std::int16_t*>(memory::AllocateDma(kBlockBytes, kStagingFloorBytes));
  return !pool.staged || pool.staging != nullptr;
}

void GrowCache(Pool& pool, std::size_t target)
{
  target = std::min(target, SlotLimit(pool));
  for (std::size_t slot = 0; slot < kMaxSlots && pool.slotCount < target; ++slot)
  {
    if (g_shared.frames[slot] != nullptr) continue;
    std::int16_t* frames = AllocateBlock(pool);
    if (frames == nullptr) break;
    pool.work->slots[slot] = SlotInfo{};
    g_shared.keys[slot].store(0, std::memory_order_relaxed);
    g_shared.frames[slot] = frames;
    ++pool.slotCount;
  }
  g_shared.blocks.store(static_cast<std::uint32_t>(pool.slotCount), std::memory_order_relaxed);
}

void FreeSlot(Pool& pool, std::size_t slot)
{
  FreeBlock(pool, g_shared.frames[slot]);
  g_shared.frames[slot] = nullptr;
  --pool.slotCount;
}

// Back to what the plan asks for and a little more: free slots first, then blocks nobody has
// needed for a while (taken away now, freed in a later round).
void ShrinkCache(Pool& pool, const std::array<bool, kMaxSlots>& kept, std::uint32_t now)
{
  const std::size_t keep = std::max(pool.work->desired + kSlackSlots, kMinSlots);
  for (std::size_t slot = 0; slot < kMaxSlots && pool.slotCount > keep; ++slot)
    if (!kept[slot] && SlotFree(pool, slot)) FreeSlot(pool, slot);
  std::array<std::uint8_t, kMaxSlots> idle{};
  std::size_t idleCount = 0;
  std::size_t over = pool.slotCount > keep ? pool.slotCount - keep : 0;
  for (std::size_t slot = 0; slot < kMaxSlots && idleCount < over; ++slot)
  {
    if (kept[slot] || g_shared.keys[slot].load(std::memory_order_relaxed) == 0) continue;
    if (now - pool.work->slots[slot].lastUseMs >= kIdleSlotMs) idle[idleCount++] = static_cast<std::uint8_t>(slot);
  }
  RetireSlots(pool, idle.data(), idleCount);
  g_shared.blocks.store(static_cast<std::uint32_t>(pool.slotCount), std::memory_order_relaxed);
}

// Nothing is loaded from the card any more: every block, the staging block and the plan go back.
bool ReleaseCache(Pool& pool)
{
  if (pool.work == nullptr) return true;
  std::array<std::uint8_t, kMaxSlots> keyed{};
  std::size_t keyedCount = 0;
  for (std::size_t slot = 0; slot < kMaxSlots; ++slot)
    if (g_shared.keys[slot].load(std::memory_order_relaxed) != 0) keyed[keyedCount++] = static_cast<std::uint8_t>(slot);
  RetireSlots(pool, keyed.data(), keyedCount);
  for (std::size_t slot = 0; slot < kMaxSlots; ++slot)
    if (g_shared.frames[slot] != nullptr && !SlotFree(pool, slot)) return false;
  for (std::size_t slot = 0; slot < kMaxSlots; ++slot)
    if (g_shared.frames[slot] != nullptr) FreeSlot(pool, slot);
  memory::FreeDma(pool.staging);
  pool.staging = nullptr;
  delete pool.work;
  pool.work = nullptr;
  g_shared.blocks.store(0, std::memory_order_relaxed);
  return true;
}

// ---------------------------------------------------------------------------------------------------
// Files

void CloseFile(FileState& file)
{
  if (file.file == nullptr) return;
  std::fclose(file.file);
  file.file = nullptr;
}

void Failed(FileState& file, const char* what)
{
  CloseFile(file);
  Bump(g_shared.failures);
  ++file.failures;
  if (file.failures >= kFailuresBeforeSilence)
  {
    // Card out or the file gone: silent until the card comes back, no retries holding up the others.
    file.failed = true;
    std::printf("stream: %s failed %u times, silent until the card is back: %s\n", what, static_cast<unsigned>(file.failures), file.path.c_str());
    return;
  }
  file.retryAt = NowMs() + kRetryMs * file.failures;
  std::printf("stream: %s failed, again in %u ms: %s\n", what, static_cast<unsigned>(kRetryMs * file.failures), file.path.c_str());
}

bool Servable(const Pool& pool, const FileState& file, std::uint32_t now)
{
  return file.id != 0 && !file.failed && Reached(now, file.retryAt) && !Guarded(pool, file.path);
}

bool EnsureOpen(Pool& pool, std::size_t index)
{
  FileState& file = pool.files[index];
  file.lastUse = ++pool.clock;
  if (file.file != nullptr) return true;
  // Out of file slots: give up the one needed latest (by the plan: no block missing at all, else the
  // latest deadline), the longest unused among equals.
  std::size_t open = 0;
  FileState* drop = nullptr;
  std::uint32_t dropUrgency = 0;
  for (std::size_t other = 0; other < pool.files.size(); ++other)
  {
    FileState& candidate = pool.files[other];
    if (candidate.file == nullptr) continue;
    ++open;
    const std::uint32_t urgency = pool.work != nullptr ? pool.work->urgency[other] : kNever;
    if (drop == nullptr || urgency > dropUrgency || (urgency == dropUrgency && candidate.lastUse < drop->lastUse))
    {
      drop = &candidate;
      dropUrgency = urgency;
    }
  }
  if (open >= kMaxOpenFiles && drop != nullptr) CloseFile(*drop);
#if !defined(ESP_PLATFORM)
  TestDelay(g_testOpenMicros);
#endif
  file.file = std::fopen(file.path.c_str(), "rb");
  if (file.file == nullptr)
  {
    // Every file slot of the card in use elsewhere is no failure of this file: later.
    if (errno == ENFILE || errno == EMFILE) file.retryAt = NowMs() + kRetryMs;
    else Failed(file, "open");
    return false;
  }
  Bump(g_shared.opens);
  // Read through its descriptor (ReadBytes): a stdio buffer would only cost memory, and none may hold
  // data the descriptor's position has moved past.
  std::setvbuf(file.file, nullptr, _IONBF, 0);
  return true;
}

// Through the file's descriptor: newlib reads an unbuffered FILE a byte at a time, every byte a VFS and
// FATFS call. A 2 KB block took 33 ms on the board and the stream worker, above the UI on its core,
// kept 76% of it while a sample played from the card (the stress test, 2026-11-04); read() takes the
// block in one FATFS call, its whole sectors in one card command.
bool ReadBytes(std::FILE* file, std::uint64_t offset, void* destination, std::size_t bytes)
{
#if defined(_WIN32)
  const int descriptor = _fileno(file);
  if (_lseeki64(descriptor, static_cast<__int64>(offset), SEEK_SET) != static_cast<__int64>(offset)) return false;
#else
  const int descriptor = fileno(file);
  if (::lseek(descriptor, static_cast<off_t>(offset), SEEK_SET) != static_cast<off_t>(offset)) return false;
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

template <typename Taken>
std::uint32_t NewId(std::uint32_t& next, Taken&& taken)
{
  // Ids wrap within what a key holds, never 0 and never one still in use.
  std::uint32_t id = 0;
  do
  {
    id = next;
    next = next >= kMaxId ? 1 : next + 1;
  } while (taken(id));
  return id;
}

// Streams whose last owner let go, and files no stream reads any more (their blocks are taken away).
void RetireReleased(Pool& pool)
{
  for (std::size_t slot = 0; slot < kMaxStreams; ++slot)
  {
    StreamShared& stream = g_shared.streams[slot];
    if (stream.id.load(std::memory_order_acquire) == 0 || !stream.released.load(std::memory_order_acquire)) continue;
    FileState& file = pool.files[pool.streamFile[slot]];
    if (file.streams > 0) --file.streams;
    stream.released.store(false, std::memory_order_relaxed);
    stream.id.store(0, std::memory_order_release);
  }
  std::array<std::uint8_t, kMaxSlots> dead{};
  std::size_t deadCount = 0;
  for (FileState& file : pool.files)
  {
    if (file.id == 0 || file.streams != 0) continue;
    CloseFile(file);
    for (std::size_t slot = 0; slot < kMaxSlots && pool.work != nullptr; ++slot)
    {
      const std::uint32_t key = g_shared.keys[slot].load(std::memory_order_relaxed);
      if (key != 0 && KeyId(key) == file.id) dead[deadCount++] = static_cast<std::uint8_t>(slot);
    }
    file = FileState{};
  }
  RetireSlots(pool, dead.data(), deadCount);
}

bool AnyLiveStream()
{
  return std::any_of(g_shared.streams.begin(), g_shared.streams.end(), [](const StreamShared& stream) {
    return stream.id.load(std::memory_order_acquire) != 0 && !stream.released.load(std::memory_order_acquire);
  });
}

// ---------------------------------------------------------------------------------------------------
// The plan

struct RegionPlan {
  std::uint32_t first = 0;  // blocks
  std::uint32_t last = 0;
  bool loop = false;
};

struct StreamPlan {
  bool live = false;
  std::uint8_t file = 0;
  std::array<RegionPlan, kMaxRegions> regions{};
  std::size_t regionCount = 0;
};

// The regions published lately (and the latest one in any case: nothing is published while the output
// is held), in blocks.
void PlanRegions(const StreamShared& shared, const FileState& file, StreamPlan& plan)
{
  const std::uint32_t pass = g_shared.pass.load(std::memory_order_relaxed);
  std::uint32_t newest = 0;
  bool any = false;
  for (const RegionShared& region : shared.regions)
    if (region.end.load(std::memory_order_relaxed) != 0)
    {
      const std::uint32_t stamp = region.stamp.load(std::memory_order_relaxed);
      if (!any || static_cast<std::int32_t>(stamp - newest) > 0) newest = stamp;
      any = true;
    }
  plan.regionCount = 0;
  for (const RegionShared& region : shared.regions)
  {
    const std::uint32_t endWord = region.end.load(std::memory_order_relaxed);
    if (endWord == 0) continue;
    const std::uint32_t stamp = region.stamp.load(std::memory_order_relaxed);
    if (stamp != newest && pass - stamp > kRegionLivePasses) continue;
    const std::size_t start = std::min<std::size_t>(region.start.load(std::memory_order_relaxed), file.frameCount - 1);
    std::size_t end = endWord & ~kRegionLoop;
    end = end > file.frameCount ? file.frameCount : std::max(end, start + 1);
    plan.regions[plan.regionCount++] = RegionPlan{BlockOf(file, start), BlockOf(file, end - 1), (endWord & kRegionLoop) != 0};
  }
}

// Where a voice in `block` goes after `block`: on, back to its loop's start, or nowhere (its one-shot
// ends). The region it is in tells, a loop first (a voice in a loop stays in it).
bool NextBlock(const StreamPlan& stream, const FileState& file, std::uint32_t& block)
{
  const RegionPlan* inside = nullptr;
  for (std::size_t index = 0; index < stream.regionCount; ++index)
  {
    const RegionPlan& region = stream.regions[index];
    if (block < region.first || block > region.last) continue;
    if (inside == nullptr || (region.loop && !inside->loop) || (region.loop == inside->loop && region.last > inside->last)) inside = &region;
  }
  if (inside != nullptr && block == inside->last)
  {
    if (!inside->loop) return false;
    block = inside->first;
    return true;
  }
  return ++block < file.blockCount;
}

struct VoicePlan {
  std::uint8_t stream = 0;
  std::uint32_t block = 0;
  std::uint32_t rate = kRateOne;
  std::size_t depth = 0;
};

// Output frames until a voice that is somewhere in block `from` reaches the start of `ahead` blocks
// on, assuming it is at the end of its block already.
std::uint32_t Deadline(std::size_t ahead, std::uint32_t rate)
{
  if (ahead <= 1) return 0;
  const std::uint64_t frames = (static_cast<std::uint64_t>(ahead - 1) * kStreamBlockFrames << 16) / rate;
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(frames, kNever - 1));
}

void AddNeed(Work& work, std::uint8_t file, std::uint32_t fileId, std::uint32_t block, std::uint32_t deadline, bool due)
{
  if (work.needCount >= work.needs.size()) return;
  Need& need = work.needs[work.needCount++];
  need = Need{};
  need.key = BlockKey(fileId, block);
  need.deadline = deadline;
  need.file = file;
  need.due = due;
}

Need* FindNeed(Work& work, std::uint32_t key)
{
  const auto end = work.needs.begin() + static_cast<std::ptrdiff_t>(work.needCount);
  const auto found = std::lower_bound(work.needs.begin(), end, key, [](const Need& need, std::uint32_t value) { return need.key < value; });
  return found != end && found->key == key ? &*found : nullptr;
}

// What every voice and every start needs, fitted to the cache (which grows toward it first); which
// blocks are there; who is due.
void BuildPlan(Pool& pool, Work& work, std::uint32_t now, std::array<bool, kMaxSlots>& kept)
{
  std::array<StreamPlan, kMaxStreams> streams{};
  std::size_t starts = 0;
  for (std::size_t slot = 0; slot < kMaxStreams; ++slot)
  {
    const StreamShared& shared = g_shared.streams[slot];
    if (shared.id.load(std::memory_order_acquire) == 0 || shared.released.load(std::memory_order_acquire)) continue;
    const std::uint8_t fileIndex = pool.streamFile[slot];
    const FileState& file = pool.files[fileIndex];
    if (file.id == 0) continue;
    StreamPlan& plan = streams[slot];
    plan.live = true;
    plan.file = fileIndex;
    PlanRegions(shared, file, plan);
    starts += 2 * plan.regionCount;
  }

  std::array<VoicePlan, kMaxStreamReaders> voices{};
  std::size_t voiceCount = 0;
  std::size_t desired = starts;
  for (std::size_t reader = 0; reader < kMaxStreamReaders; ++reader)
  {
    const std::uint32_t key = g_shared.readers[reader].load(std::memory_order_acquire);
    if (key == 0) continue;
    std::size_t slot = 0;
    while (slot < kMaxStreams && !(streams[slot].live && g_shared.streams[slot].id.load(std::memory_order_relaxed) == KeyId(key))) ++slot;
    if (slot == kMaxStreams) continue;
    VoicePlan& voice = voices[voiceCount++];
    voice.stream = static_cast<std::uint8_t>(slot);
    voice.block = KeyBlock(key);
    voice.rate = std::max<std::uint32_t>(1, g_shared.rates[reader].load(std::memory_order_relaxed));
    // Half a second at its speed, so a voice an octave up reads twice as far.
    const std::uint64_t span = static_cast<std::uint64_t>(kStreamBlockFrames) << 16;
    voice.depth = std::clamp<std::size_t>(static_cast<std::size_t>((static_cast<std::uint64_t>(kAheadFrames) * voice.rate + span - 1) / span), 2, kMaxAhead);
    desired += voice.depth + 1;
  }
  work.desired = std::min(desired, SlotLimit(pool));
  GrowCache(pool, work.desired);

  // Fitted to the cache: every window as deep as the cache allows, one block after the voice's own
  // at the least.
  std::size_t cap = kMaxAhead;
  const auto total = [&](std::size_t limit) {
    std::size_t sum = starts;
    for (std::size_t index = 0; index < voiceCount; ++index) sum += std::min(voices[index].depth, limit) + 1;
    return sum;
  };
  while (cap > 1 && total(cap) > pool.slotCount) --cap;

  work.needCount = 0;
  for (std::size_t slot = 0; slot < kMaxStreams; ++slot)
  {
    const StreamPlan& stream = streams[slot];
    if (!stream.live) continue;
    const FileState& file = pool.files[stream.file];
    for (std::size_t index = 0; index < stream.regionCount; ++index)
    {
      const RegionPlan& region = stream.regions[index];
      AddNeed(work, stream.file, file.id, region.first, kStartDeadlineFrames, true);
      if (region.first < region.last) AddNeed(work, stream.file, file.id, region.first + 1, kStartDeadlineFrames + kStreamBlockFrames, true);
    }
  }
  work.windowCount = voiceCount;
  for (std::size_t index = 0; index < voiceCount; ++index)
  {
    const VoicePlan& voice = voices[index];
    const StreamPlan& stream = streams[voice.stream];
    const FileState& file = pool.files[stream.file];
    Window& window = work.windows[index];
    window = Window{};
    const std::size_t depth = std::min(voice.depth, cap);
    window.low = static_cast<std::uint8_t>((depth + 1) / 2);
    std::uint32_t block = voice.block;
    for (std::size_t ahead = 0; ahead <= depth && block < file.blockCount; ++ahead)
    {
      AddNeed(work, stream.file, file.id, block, Deadline(ahead, voice.rate), false);
      window.keys[window.count++] = BlockKey(file.id, block);
      if (!NextBlock(stream, file, block)) break;
    }
  }

  // Each block once, at its most urgent.
  const auto begin = work.needs.begin();
  std::sort(begin, begin + static_cast<std::ptrdiff_t>(work.needCount), [](const Need& a, const Need& b) { return a.key < b.key; });
  std::size_t unique = 0;
  for (std::size_t index = 0; index < work.needCount; ++index)
  {
    if (unique > 0 && work.needs[unique - 1].key == work.needs[index].key)
    {
      Need& first = work.needs[unique - 1];
      first.deadline = std::min(first.deadline, work.needs[index].deadline);
      first.due = first.due || work.needs[index].due;
      continue;
    }
    work.needs[unique++] = work.needs[index];
  }
  work.needCount = unique;

  kept.fill(false);
  for (std::size_t slot = 0; slot < kMaxSlots; ++slot)
  {
    const std::uint32_t key = g_shared.keys[slot].load(std::memory_order_relaxed);
    if (key == 0) continue;
    if (Need* need = FindNeed(work, key))
    {
      need->present = true;
      kept[slot] = true;
      work.slots[slot].lastUseMs = now;
    }
  }

  // A voice is due when its own block is missing or fewer than half the blocks after it are there.
  for (std::size_t index = 0; index < work.windowCount; ++index)
  {
    const Window& window = work.windows[index];
    if (window.count == 0) continue;
    const auto present = [&](std::uint32_t key) {
      const Need* need = FindNeed(work, key);
      return need != nullptr && need->present;
    };
    std::size_t ahead = 0;
    while (1 + ahead < window.count && present(window.keys[1 + ahead])) ++ahead;
    const bool whole = 1 + ahead == window.count;
    if (present(window.keys[0]) && (whole || ahead >= window.low)) continue;
    for (std::size_t at = 0; at < window.count; ++at)
      if (Need* need = FindNeed(work, window.keys[at])) need->due = true;
  }

  work.urgency.fill(kNever);
  for (std::size_t index = 0; index < work.needCount; ++index)
  {
    const Need& need = work.needs[index];
    if (!need.present) work.urgency[need.file] = std::min(work.urgency[need.file], need.deadline);
  }
}

// ---------------------------------------------------------------------------------------------------
// The worker

enum class PumpResult { Idle, Again, Later };

// One block into its slot, under the lock; the slot is the worker's until its key is set.
void ReadBlock(Pool& pool, const VisitRead& read)
{
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  FileState& file = pool.files[read.file];
  if (file.id != read.fileId || !Servable(pool, file, NowMs())) return;
  std::int16_t* frames = g_shared.frames[read.slot];
  if (frames == nullptr || g_shared.keys[read.slot].load(std::memory_order_relaxed) != 0) return;
  if (!EnsureOpen(pool, read.file)) return;
  const std::uint64_t offset = file.base + static_cast<std::uint64_t>(KeyBlock(read.key)) * kBlockBytes;
  if (offset >= file.dataEnd) return;
  const auto bytes = static_cast<std::size_t>(std::min<std::uint64_t>(kBlockBytes, file.dataEnd - offset));
  std::int16_t* target = pool.staged ? pool.staging : frames;
  const std::uint32_t started = NowMicros();
#if !defined(ESP_PLATFORM)
  TestDelay(g_testReadMicros);
#endif
  if (!ReadBytes(file.file, offset, target, bytes))
  {
    Failed(file, "read");
    return;
  }
  const std::uint32_t took = NowMicros() - started;
  file.failures = 0;
  if (pool.staged) std::memcpy(frames, target, bytes);
  if (bytes < kBlockBytes) std::memset(reinterpret_cast<char*>(frames) + bytes, 0, kBlockBytes - bytes);
  g_shared.keys[read.slot].store(read.key, std::memory_order_release);
  pool.work->slots[read.slot].lastUseMs = NowMs();
  Bump(g_shared.reads);
  if (took > g_shared.slowestMicros.load(std::memory_order_relaxed)) g_shared.slowestMicros.store(took, std::memory_order_relaxed);
}

// One round: retire what was let go, plan, and read for the most urgent file that is due.
PumpResult Pump(Pool& pool, bool& urgentLeft)
{
  urgentLeft = false;
  std::array<VisitRead, kMaxReadsPerVisit> visit{};
  std::size_t visitCount = 0;
  std::uint32_t graceStamp = 0;
  bool needGrace = false;
  {
    std::lock_guard<std::recursive_mutex> lock(pool.io);
    RetireReleased(pool);
    if (!AnyLiveStream()) return ReleaseCache(pool) ? PumpResult::Idle : PumpResult::Later;
    if (!EnsureWork(pool)) return PumpResult::Later;
    Work& work = *pool.work;
    const std::uint32_t now = NowMs();
    std::array<bool, kMaxSlots> kept{};
    BuildPlan(pool, work, now, kept);
    ShrinkCache(pool, kept, now);

    // The file with the soonest deadline among what is due and can be read now.
    std::uint32_t best = kNever;
    std::size_t bestFile = kMaxFiles;
    bool blocked = false;
    for (std::size_t index = 0; index < work.needCount; ++index)
    {
      const Need& need = work.needs[index];
      if (need.present || !need.due) continue;
      if (!Servable(pool, pool.files[need.file], now))
      {
        blocked = true;
        continue;
      }
      if (need.deadline < best || bestFile == kMaxFiles)
      {
        best = need.deadline;
        bestFile = need.file;
      }
    }
    if (bestFile == kMaxFiles) return blocked ? PumpResult::Later : PumpResult::Idle;
    for (std::size_t index = 0; index < work.needCount; ++index)
    {
      const Need& need = work.needs[index];
      if (!need.present && need.due && need.file != bestFile && need.deadline < kUrgentFrames) urgentLeft = true;
    }

    // Its missing blocks, the most urgent first, as many as there are slots to put them in.
    std::array<std::uint16_t, kMaxNeeds> wanted{};
    std::size_t wantedCount = 0;
    for (std::size_t index = 0; index < work.needCount; ++index)
      if (!work.needs[index].present && work.needs[index].file == bestFile) wanted[wantedCount++] = static_cast<std::uint16_t>(index);
    std::sort(wanted.begin(), wanted.begin() + static_cast<std::ptrdiff_t>(wantedCount),
              [&](std::uint16_t a, std::uint16_t b) { return work.needs[a].deadline < work.needs[b].deadline; });
    wantedCount = std::min(wantedCount, kMaxReadsPerVisit);

    std::array<std::uint8_t, kMaxReadsPerVisit> victims{};
    std::size_t victimCount = 0;
    for (std::size_t slot = 0; slot < kMaxSlots && victimCount < wantedCount; ++slot)
      if (SlotFree(pool, slot)) victims[victimCount++] = static_cast<std::uint8_t>(slot);
    if (victimCount < wantedCount)
    {
      // Then blocks no voice and no start needs, the longest unused first: taken away now, written
      // once the pass that may still read them is over.
      std::array<std::uint8_t, kMaxSlots> spare{};
      std::size_t spareCount = 0;
      for (std::size_t slot = 0; slot < kMaxSlots; ++slot)
        if (!kept[slot] && g_shared.frames[slot] != nullptr && g_shared.keys[slot].load(std::memory_order_relaxed) != 0)
          spare[spareCount++] = static_cast<std::uint8_t>(slot);
      std::sort(spare.begin(), spare.begin() + static_cast<std::ptrdiff_t>(spareCount),
                [&](std::uint8_t a, std::uint8_t b) { return work.slots[a].lastUseMs < work.slots[b].lastUseMs; });
      const std::size_t taken = std::min(spareCount, wantedCount - victimCount);
      RetireSlots(pool, spare.data(), taken);
      for (std::size_t index = 0; index < taken; ++index)
      {
        victims[victimCount++] = spare[index];
        graceStamp = work.slots[spare[index]].retiredPass;
        needGrace = true;
      }
    }
    if (victimCount == 0) return PumpResult::Later;  // every block is wanted: the rest waits
    wantedCount = victimCount;
    const FileState& file = pool.files[bestFile];
    for (std::size_t index = 0; index < wantedCount; ++index)
    {
      const Need& need = work.needs[wanted[index]];
      visit[index] = VisitRead{victims[index], need.file, file.id, need.key, need.deadline};
    }
    visitCount = wantedCount;
    // In the file's order: the card is read forward, and FAT seeks forward from where it is.
    std::sort(visit.begin(), visit.begin() + static_cast<std::ptrdiff_t>(visitCount),
              [](const VisitRead& a, const VisitRead& b) { return a.key < b.key; });
  }

  if (needGrace)
  {
    const std::uint32_t started = NowMs();
    while (!GraceOver(graceStamp))
    {
      if (NowMs() - started > kGraceWaitMs) return PumpResult::Later;  // the render is stuck: nothing to gain
      Nap();
    }
    std::lock_guard<std::recursive_mutex> lock(pool.io);
    for (std::size_t index = 0; index < visitCount; ++index)
      if (pool.work != nullptr) (void)SlotFree(pool, visit[index].slot);
  }
  for (std::size_t index = 0; index < visitCount; ++index) ReadBlock(pool, visit[index]);
  return PumpResult::Again;
}

#if defined(ESP_PLATFORM)
void WorkerTask(void* context)
{
  Pool& pool = *static_cast<Pool*>(context);
  for (;;)
  {
    bool urgent = false;
    const PumpResult result = Pump(pool, urgent);
    if (result == PumpResult::Again)
    {
      // Between visits a tick for the tasks below, unless a voice is about to run dry.
      if (!urgent) vTaskDelay(1);
      continue;
    }
    (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(result == PumpResult::Later ? 50 : 250));
  }
}
#endif

void EnsureWorker(Pool& pool)
{
#if defined(ESP_PLATFORM)
  // On the UI's core and at its priority, below the second audio core's helper (6): a voice has half a
  // second of blocks ahead and can wait for a frame, the helper cannot wait for a card read. The card
  // reads themselves leave the core to others while the DMA runs.
  if (g_shared.worker.load(std::memory_order_acquire) != nullptr) return;
  TaskHandle_t worker = nullptr;
  if (xTaskCreatePinnedToCore(WorkerTask, "sample_stream", 6144, &pool, 5, &worker, 0) == pdPASS)
    g_shared.worker.store(worker, std::memory_order_release);
#elif !defined(__EMSCRIPTEN__)
  if (pool.workerStarted) return;
  pool.workerStarted = true;
  std::thread([&pool] {
    for (;;)
    {
      bool urgent = false;
      const PumpResult result = Pump(pool, urgent);
      if (result == PumpResult::Again)
      {
        if (!urgent) std::this_thread::sleep_for(std::chrono::microseconds(200));
        continue;
      }
      std::unique_lock<std::mutex> lock(pool.wakeMutex);
      pool.wake.wait_for(lock, std::chrono::milliseconds(result == PumpResult::Later ? 20 : 250), [&pool] { return pool.woken; });
      pool.woken = false;
    }
  }).detach();
#else
  (void)pool;
#endif
}

#if defined(__EMSCRIPTEN__)
// No threads in the browser, and its files are in memory: the rounds run on the spot.
void PumpNow(Pool& pool)
{
  for (int round = 0; round < 8; ++round)
  {
    bool urgent = false;
    if (Pump(pool, urgent) != PumpResult::Again) return;
  }
}
#endif

}  // namespace

// ---------------------------------------------------------------------------------------------------

std::shared_ptr<SampleStream> SampleStream::Open(const std::string& path, std::uint64_t dataOffset, std::size_t frameCount)
{
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  RetireReleased(pool);
  if (frameCount == 0) return nullptr;
  // Blocks on the file's own grid: block 0 starts at the file's start and holds the header. (A data
  // chunk at an odd offset, which RIFF does not allow, gets its own grid.)
  const std::uint64_t base = dataOffset % 2 == 0 ? 0 : dataOffset;
  const auto lead = static_cast<std::uint32_t>((dataOffset - base) / sizeof(std::int16_t));
  const std::uint64_t blocks = (lead + static_cast<std::uint64_t>(frameCount) + kStreamBlockFrames - 1) / kStreamBlockFrames;
  if (blocks >= kKeyBlockMask) return nullptr;  // longer than a key can number (no sample here is that long)

  std::size_t slot = 0;
  std::size_t live = 1;
  bool foundSlot = false;
  for (std::size_t index = 0; index < kMaxStreams; ++index)
  {
    if (g_shared.streams[index].id.load(std::memory_order_relaxed) != 0) ++live;
    else if (!foundSlot)
    {
      slot = index;
      foundSlot = true;
    }
  }
  if (!foundSlot) return nullptr;
  // The worker exists before any memory does: it gives back what a failed open took.
  EnsureWorker(pool);
  const auto fail = [] {
    Wake();
    return std::shared_ptr<SampleStream>();
  };
  // Room for every stream's start and a voice.
  if (!EnsureWork(pool)) return fail();
  GrowCache(pool, std::max(kMinSlots, 2 * live + 2));
  if (pool.slotCount < kMinSlots) return fail();

  // The file: one that streams already (its blocks are shared), else a new one.
  std::size_t fileIndex = kMaxFiles;
  for (std::size_t index = 0; index < kMaxFiles && fileIndex == kMaxFiles; ++index)
  {
    const FileState& file = pool.files[index];
    if (file.id != 0 && file.shareable && !file.failed && file.dataOffset == dataOffset && file.frameCount == frameCount && SamePath(file.path, path) &&
        !Guarded(pool, path))
      fileIndex = index;
  }
  if (fileIndex == kMaxFiles)
  {
    for (std::size_t index = 0; index < kMaxFiles && fileIndex == kMaxFiles; ++index)
      if (pool.files[index].id == 0) fileIndex = index;
    if (fileIndex == kMaxFiles) return fail();
    FileState& file = pool.files[fileIndex];
    file = FileState{};
    file.id = NewId(pool.nextFileId, [&](std::uint32_t id) {
      return std::any_of(pool.files.begin(), pool.files.end(), [id](const FileState& other) { return other.id == id; }) ||
             std::any_of(g_shared.keys.begin(), g_shared.keys.end(), [id](const std::atomic<std::uint32_t>& key) {
               const std::uint32_t value = key.load(std::memory_order_relaxed);
               return value != 0 && KeyId(value) == id;
             });
    });
    file.path = path;
    file.dataOffset = dataOffset;
    file.dataEnd = dataOffset + static_cast<std::uint64_t>(frameCount) * sizeof(std::int16_t);
    file.base = base;
    file.frameCount = frameCount;
    file.leadFrames = lead;
    file.blockCount = static_cast<std::uint32_t>(blocks);
  }
  FileState& file = pool.files[fileIndex];
  const std::uint32_t id = NewId(pool.nextStreamId, [](std::uint32_t candidate) {
    return std::any_of(g_shared.streams.begin(), g_shared.streams.end(),
                       [candidate](const StreamShared& stream) { return stream.id.load(std::memory_order_relaxed) == candidate; });
  });
  std::shared_ptr<SampleStream> stream(new (std::nothrow) SampleStream(slot, id, file.id, frameCount, lead));
  if (!stream)
  {
    if (file.streams == 0) file = FileState{};
    return fail();
  }
  ++file.streams;
  pool.streamFile[slot] = static_cast<std::uint8_t>(fileIndex);
  StreamShared& shared = g_shared.streams[slot];
  // The whole file until someone says otherwise: its start is read at once.
  for (RegionShared& region : shared.regions) region.end.store(0, std::memory_order_relaxed);
  shared.regions[0].start.store(0, std::memory_order_relaxed);
  shared.regions[0].stamp.store(g_shared.pass.load(std::memory_order_relaxed), std::memory_order_relaxed);
  shared.regions[0].end.store(static_cast<std::uint32_t>(frameCount), std::memory_order_relaxed);
  shared.released.store(false, std::memory_order_relaxed);
  shared.id.store(id, std::memory_order_release);
  Wake();
#if defined(__EMSCRIPTEN__)
  PumpNow(pool);
#endif
  return stream;
}

SampleStream::~SampleStream()
{
  // Possibly on the render thread: the worker does the closing.
  g_shared.streams[slot_].released.store(true, std::memory_order_release);
  Wake();
#if defined(__EMSCRIPTEN__)
  PumpNow(ThePool());
#endif
}

bool SampleStream::FindBlock(StreamCursor& cursor, std::uint32_t index) const
{
  const std::uint32_t key = BlockKey(fileId_, index);
  cursor.blockIndex = index;
#if defined(__EMSCRIPTEN__)
  for (int attempt = 0; attempt < 2; ++attempt)
  {
#endif
    cursor.pass = detail::g_streamReadPass;
#if defined(__SANITIZE_THREAD__)
    // The thread sanitizer does not follow fences: the same order, said by every load.
    constexpr std::memory_order kKeyOrder = std::memory_order_acquire;
#else
    constexpr std::memory_order kKeyOrder = std::memory_order_relaxed;
#endif
    for (std::size_t slot = 0; slot < kMaxSlots; ++slot)
    {
      if (g_shared.keys[slot].load(kKeyOrder) != key) continue;
      std::atomic_thread_fence(std::memory_order_acquire);
      cursor.block = g_shared.frames[slot];
      return true;
    }
#if defined(__EMSCRIPTEN__)
    if (attempt == 0) PumpNow(ThePool());
  }
#endif
  cursor.block = nullptr;
  if (cursor.lateIndex != index)
  {
    cursor.lateIndex = index;
    Bump(g_shared.late);
  }
  return false;
}

void SampleStream::PublishRegion(std::size_t start, std::size_t end, bool loop) const
{
  StreamShared& shared = g_shared.streams[slot_];
  const auto start32 = static_cast<std::uint32_t>(std::min<std::size_t>(start, frameCount_ - 1));
  const std::uint32_t end32 = static_cast<std::uint32_t>(std::clamp<std::size_t>(end, start32 + 1, frameCount_)) | (loop ? kRegionLoop : 0u);
  const std::uint32_t pass = g_shared.pass.load(std::memory_order_relaxed);
  // The render thread is the only writer. The same region again: still in use.
  for (RegionShared& region : shared.regions)
    if (region.end.load(std::memory_order_relaxed) == end32 && region.start.load(std::memory_order_relaxed) == start32)
    {
      region.stamp.store(pass, std::memory_order_relaxed);
      return;
    }
  // A new one (another track, or a trim that moved) takes the place of the one published longest ago.
  RegionShared* oldest = &shared.regions[0];
  for (RegionShared& region : shared.regions)
  {
    if (region.end.load(std::memory_order_relaxed) == 0)
    {
      oldest = &region;
      break;
    }
    if (static_cast<std::int32_t>(region.stamp.load(std::memory_order_relaxed) - oldest->stamp.load(std::memory_order_relaxed)) < 0) oldest = &region;
  }
  oldest->end.store(0, std::memory_order_relaxed);
  oldest->start.store(start32, std::memory_order_relaxed);
  oldest->stamp.store(pass, std::memory_order_relaxed);
  oldest->end.store(end32, std::memory_order_relaxed);
  Wake();
}

void SampleStream::PublishReader(std::size_t reader, const SampleStream* stream, std::size_t frame, float rate)
{
  if (reader >= kMaxStreamReaders) return;
  std::uint32_t key = 0;
  std::uint32_t rate16 = 0;
  if (stream != nullptr)
  {
    key = BlockKey(stream->id_, static_cast<std::uint32_t>((frame + stream->leadFrames_) / kStreamBlockFrames));
    rate16 = rate > 0.0f ? static_cast<std::uint32_t>(std::min(rate, 1024.0f) * 65536.0f) : kRateOne;
  }
  // The render thread is the only writer: no read-modify-write (a library call on this chip).
  g_shared.rates[reader].store(rate16, std::memory_order_relaxed);
  if (g_shared.readers[reader].load(std::memory_order_relaxed) == key) return;
  g_shared.readers[reader].store(key, std::memory_order_release);
  if (key != 0) Wake();
}

bool SampleStream::ReadFrames(std::size_t frame, std::int16_t* destination, std::size_t count) const
{
  if (frame + count > frameCount_) return false;
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  const std::size_t index = pool.streamFile[slot_];
  FileState& file = pool.files[index];
  if (file.id != fileId_ || !EnsureOpen(pool, index)) return false;
  const bool read = ReadBytes(file.file, file.dataOffset + static_cast<std::uint64_t>(frame) * sizeof(std::int16_t), destination, count * sizeof(std::int16_t));
  if (!read) Failed(file, "read");
  // Inside a guard of its own file (a copy of a sample onto itself): the guard's caller needs it closed.
  else if (Guarded(pool, file.path)) CloseFile(file);
  return read;
}

void BeginStreamReads()
{
  if (g_shared.passDepth++ != 0) return;
  ++detail::g_streamReadPass;
  g_shared.pass.store(g_shared.pass.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);
  // Against the worker's fence after it takes keys away: either this pass sees the key gone, or the
  // worker sees this pass under way and waits for its end.
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

void EndStreamReads()
{
  if (g_shared.passDepth == 0 || --g_shared.passDepth != 0) return;
  g_shared.pass.store(g_shared.pass.load(std::memory_order_relaxed) + 1, std::memory_order_release);
}

bool SampleStreamsReading()
{
  return std::any_of(g_shared.readers.begin(), g_shared.readers.end(),
                     [](const std::atomic<std::uint32_t>& reader) { return reader.load(std::memory_order_relaxed) != 0; });
}

StreamCounters SampleStreamCounters(bool resetSlowest)
{
  StreamCounters counters;
  counters.reads = g_shared.reads.load(std::memory_order_relaxed);
  counters.lateBlocks = g_shared.late.load(std::memory_order_relaxed);
  counters.opens = g_shared.opens.load(std::memory_order_relaxed);
  counters.failures = g_shared.failures.load(std::memory_order_relaxed);
  counters.slowestReadMicros = resetSlowest ? g_shared.slowestMicros.exchange(0, std::memory_order_relaxed)
                                            : g_shared.slowestMicros.load(std::memory_order_relaxed);
  counters.cacheBlocks = g_shared.blocks.load(std::memory_order_relaxed);
  return counters;
}

void RetrySampleStreams()
{
  Pool& pool = ThePool();
  {
    std::lock_guard<std::recursive_mutex> lock(pool.io);
    for (FileState& file : pool.files)
    {
      file.failed = false;
      file.failures = 0;
      file.retryAt = 0;
    }
  }
  Wake();
}

SampleFileGuard::SampleFileGuard(const std::string& path) : path_(path)
{
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  pool.guarded.push_back(path_);
  for (FileState& file : pool.files)
    if (file.id != 0 && SamePath(file.path, path_)) CloseFile(file);
}

SampleFileGuard::~SampleFileGuard()
{
  Pool& pool = ThePool();
  {
    std::lock_guard<std::recursive_mutex> lock(pool.io);
    const auto found = std::find_if(pool.guarded.begin(), pool.guarded.end(), [&](const std::string& guarded) { return SamePath(guarded, path_); });
    if (found != pool.guarded.end()) pool.guarded.erase(found);
    // What is there now may be another file: a sample opened from it from now on reads it afresh.
    for (FileState& file : pool.files)
      if (file.id != 0 && SamePath(file.path, path_)) file.shareable = false;
  }
  Wake();
}

void SampleFileGuard::MovedTo(const std::string& path)
{
  Pool& pool = ThePool();
  std::lock_guard<std::recursive_mutex> lock(pool.io);
  for (FileState& file : pool.files)
    if (file.id != 0 && SamePath(file.path, path_)) file.path = path;
  const auto found = std::find_if(pool.guarded.begin(), pool.guarded.end(), [&](const std::string& guarded) { return SamePath(guarded, path_); });
  if (found != pool.guarded.end()) *found = path;
  path_ = path;
}

#if !defined(ESP_PLATFORM)
namespace testing {
void SetCardDelays(std::uint32_t readMicros, std::uint32_t openMicros)
{
  g_testReadMicros.store(readMicros, std::memory_order_relaxed);
  g_testOpenMicros.store(openMicros, std::memory_order_relaxed);
}

void LimitCacheBlocks(std::size_t blocks)
{
  g_testSlotLimit.store(blocks, std::memory_order_relaxed);
  Wake();
}
}  // namespace testing
#endif

}  // namespace synth::audio

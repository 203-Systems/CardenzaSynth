// Playing a sample from a file: what a voice reads is what the file holds, the look-ahead keeps several
// voices on several files fed from a slow card, a block is never reused under a voice, the cache goes
// away with the last stream, and the file can be renamed and deleted while it is loaded.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "Library/Synth/Audio/SampleStream.h"

using synth::audio::kMaxStreamReaders;
using synth::audio::kStreamBlockFrames;
using synth::audio::SampleFileGuard;
using synth::audio::SampleStream;
using synth::audio::SampleStreamCounters;
using synth::audio::StreamCounters;
using synth::audio::StreamCursor;
using synth::audio::StreamReadPass;

#define CHECK(condition)                                                                         \
  do                                                                                             \
  {                                                                                              \
    if (!(condition))                                                                            \
    {                                                                                            \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);         \
      std::abort();                                                                              \
    }                                                                                            \
  } while (false)

namespace {

constexpr std::size_t kSampleRate = 44100;
constexpr std::size_t kPassFrames = 256;  // one audio block, as the runtime renders it
constexpr auto kPassTime = std::chrono::microseconds(kPassFrames * 1000000 / kSampleRate);

// Every file holds its own values, so a block of another file (or of another place) shows.
std::int16_t Expected(int file, std::size_t frame) { return static_cast<std::int16_t>((frame * 7 + static_cast<std::size_t>(file) * 4099) % 30011); }

std::string WriteSample(const std::filesystem::path& path, int file, std::size_t frames, std::size_t dataOffset = 44)
{
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  const std::vector<char> header(dataOffset, 0);
  out.write(header.data(), static_cast<std::streamsize>(header.size()));
  std::vector<std::int16_t> values(frames);
  for (std::size_t frame = 0; frame < frames; ++frame) values[frame] = Expected(file, frame);
  out.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(frames * sizeof(std::int16_t)));
  return path.string();
}

// Like a render pass that comes back while the block is not there yet.
std::int16_t ReadWhenReady(const SampleStream& stream, StreamCursor& cursor, std::size_t frame)
{
  for (int attempt = 0; attempt < 2000; ++attempt)
  {
    std::int16_t value = 0;
    bool there = false;
    {
      const StreamReadPass pass;
      SampleStream::PublishReader(0, &stream, frame);
      cursor.retryIn = 0;
      value = stream.Read(cursor, frame);
      there = cursor.block != nullptr;
    }
    if (there) return value;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(false && "the block never arrived");
  return 0;
}

void ClearReaders()
{
  for (std::size_t reader = 0; reader < kMaxStreamReaders; ++reader) SampleStream::PublishReader(reader, nullptr, 0);
}

// Waits for a stream's start blocks, as a note waits for nothing: they are there before it plays.
void WaitForStart(const SampleStream& stream, std::size_t frame)
{
  for (int attempt = 0; attempt < 2000; ++attempt)
  {
    StreamCursor probe;
    bool there = false;
    {
      const StreamReadPass pass;
      (void)stream.Read(probe, frame);
      there = probe.block != nullptr;
    }
    if (there) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(false && "the start never arrived");
}

struct Voice {
  std::shared_ptr<SampleStream> stream;
  int file = 0;
  std::size_t reader = 0;
  double rate = 1.0;
  std::size_t start = 0;
  std::size_t end = 0;  // 0: the whole file
  bool loop = false;
  double startsAt = 0.0;  // seconds into the run
  std::size_t from = 0;    // where it starts, if not at `start`
  bool silent = false;     // never says where it is (it reads where no plan protects anything)
  std::size_t backJumps = 0;  // jumps back into blocks just played, in the middle of a pass
  std::uint32_t renderMicros = 0;  // how long its half block takes, as on a busy board
  double pos = 0.0;
  bool started = false;
  bool done = false;
  StreamCursor cursor;
};

struct PlayResult {
  std::size_t frames = 0;
  std::size_t wrong = 0;  // a value that is not the file's at that frame: a block reused under a voice
  std::size_t gaps = 0;   // frames held because the block was late
};

// The audio runtime in real time: once per audio block, each voice publishes where it is and how fast
// it goes, then reads its frames in a render pass.
// A voice's `renderMicros` stretch its part of the pass, as rendering does on a busy board (a pass
// there takes milliseconds: the window in which a block taken away could still be read).
PlayResult Play(std::vector<Voice>& voices, double seconds, std::uint32_t seed = 1)
{
  PlayResult result;
  std::mt19937 random(seed);
  const auto passes = static_cast<std::size_t>(seconds * kSampleRate / kPassFrames);
  auto next = std::chrono::steady_clock::now();
  for (std::size_t pass = 0; pass < passes; ++pass)
  {
    const double now = static_cast<double>(pass * kPassFrames) / kSampleRate;
    {
      const StreamReadPass readPass;
      for (Voice& voice : voices)
      {
        const std::size_t end = voice.end != 0 ? voice.end : voice.stream->FrameCount();
        if (!voice.started && now >= voice.startsAt)
        {
          voice.started = true;
          voice.pos = static_cast<double>(voice.from != 0 ? voice.from : voice.start);
        }
        voice.stream->PublishRegion(voice.start, end, voice.loop);
        if (!voice.started || voice.done)
        {
          SampleStream::PublishReader(voice.reader, nullptr, 0);
          continue;
        }
        // Now and then, in the middle of the pass (after it said where it is), a jump back into a
        // block just left behind.
        const std::size_t jumpAt = voice.backJumps != 0 && random() % 24 == 0 ? kPassFrames / 2 : kPassFrames;
        if (voice.silent) SampleStream::PublishReader(voice.reader, nullptr, 0);
        else SampleStream::PublishReader(voice.reader, voice.stream.get(), static_cast<std::size_t>(voice.pos), static_cast<float>(voice.rate));
        for (std::size_t frame = 0; frame < kPassFrames; ++frame)
        {
          if (frame == jumpAt)
          {
            const double back = static_cast<double>((1 + random() % 2) * kStreamBlockFrames);
            voice.pos = std::max(static_cast<double>(voice.start), voice.pos - back);
          }
          if (voice.renderMicros != 0 && frame % (kPassFrames / 2) == kPassFrames / 4)
            std::this_thread::sleep_for(std::chrono::microseconds(voice.renderMicros));
          auto at = static_cast<std::size_t>(voice.pos);
          if (at >= end)
          {
            if (!voice.loop)
            {
              voice.done = true;
              break;
            }
            voice.pos = static_cast<double>(voice.start) + std::fmod(voice.pos - static_cast<double>(voice.start), static_cast<double>(end - voice.start));
            at = static_cast<std::size_t>(voice.pos);
          }
          const std::int16_t value = voice.stream->Read(voice.cursor, at);
          ++result.frames;
          if (voice.cursor.block == nullptr) ++result.gaps;
          else if (value != Expected(voice.file, at)) ++result.wrong;
          voice.pos += voice.rate;
        }
      }
    }
    next += kPassTime;
    std::this_thread::sleep_until(next);
  }
  ClearReaders();
  return result;
}

StreamCounters Since(const StreamCounters& before)
{
  StreamCounters now = SampleStreamCounters();
  now.reads -= before.reads;
  now.lateBlocks -= before.lateBlocks;
  now.opens -= before.opens;
  now.failures -= before.failures;
  return now;
}

void WaitForEmptyCache()
{
  for (int attempt = 0; attempt < 2000 && SampleStreamCounters().cacheBlocks != 0; ++attempt)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  CHECK(SampleStreamCounters().cacheBlocks == 0);
}

void Report(const char* name, const PlayResult& result, const StreamCounters& counters)
{
  std::cout << name << ": " << result.frames << " frames, " << result.wrong << " wrong, " << result.gaps << " held; " << counters.reads
            << " blocks read, " << counters.lateBlocks << " late, " << counters.opens << " opens, cache " << counters.cacheBlocks << " blocks" << std::endl;
}

void BasicReads(const std::filesystem::path& root)
{
  const std::size_t frames = 50 * kStreamBlockFrames + 100;
  const std::string path = WriteSample(root / "long.wav", 0, frames);
  auto stream = SampleStream::Open(path, 44, frames);
  CHECK(stream && stream->FrameCount() == frames);
  // Every block arrives, in order and in the middle, up to the partial last block.
  StreamCursor cursor;
  for (std::size_t frame = 0; frame < frames; frame += 137) CHECK(ReadWhenReady(*stream, cursor, frame) == Expected(0, frame));
  CHECK(ReadWhenReady(*stream, cursor, frames - 1) == Expected(0, frames - 1));
  // A jump back (a loop, a new note) finds the block again.
  CHECK(ReadWhenReady(*stream, cursor, 5) == Expected(0, 5));

  // A late block is no failure: the voice holds its last frame.
  {
    ClearReaders();
    StreamCursor late;
    late.last = 77;
    const StreamReadPass pass;
    const std::int16_t value = stream->Read(late, 40 * kStreamBlockFrames + 3);
    CHECK(value == 77 || late.block != nullptr);
  }

  // The start point stays cached, so a note can begin at once.
  stream->PublishStart(30 * kStreamBlockFrames);
  WaitForStart(*stream, 30 * kStreamBlockFrames);
  StreamCursor start;
  CHECK(ReadWhenReady(*stream, start, 30 * kStreamBlockFrames) == Expected(0, 30 * kStreamBlockFrames));

  // Direct reads for waveforms and copies.
  std::vector<std::int16_t> direct(600);
  CHECK(stream->ReadFrames(2000, direct.data(), direct.size()) && direct[599] == Expected(0, 2599));
  CHECK(!stream->ReadFrames(frames - 10, direct.data(), 11));

  // Under a guard the file can be renamed; the stream follows.
  const std::string moved = (root / "moved.wav").string();
  {
    SampleFileGuard guard(path);
    std::filesystem::rename(path, moved);
    guard.MovedTo(moved);
  }
  CHECK(stream->ReadFrames(3000, direct.data(), 10) && direct[0] == Expected(0, 3000));
  // Several streams can run; a slot is free again after release.
  std::vector<std::shared_ptr<SampleStream>> many;
  for (int index = 0; index < 7; ++index)
  {
    many.push_back(SampleStream::Open(moved, 44, frames));
    CHECK(many.back());
  }
  CHECK(!SampleStream::Open(moved, 44, frames));  // eight in all
  many.clear();
  CHECK(SampleStream::Open(moved, 44, frames));

  // Deleted while loaded: silence, no crash.
  {
    SampleFileGuard guard(moved);
    std::filesystem::remove(moved);
  }
  CHECK(!stream->ReadFrames(4000, direct.data(), 10));
  StreamCursor gone;
  for (int attempt = 0; attempt < 20; ++attempt)
  {
    {
      const StreamReadPass pass;
      SampleStream::PublishReader(0, stream.get(), 45 * kStreamBlockFrames);
      (void)stream->Read(gone, 45 * kStreamBlockFrames);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ClearReaders();
}

// The data chunk anywhere: at an odd offset (its own grid) and at one that is not on a sector.
void Offsets(const std::filesystem::path& root)
{
  for (const std::size_t offset : {std::size_t{45}, std::size_t{1000}, std::size_t{8192}})
  {
    const std::size_t frames = 3 * kStreamBlockFrames + 17;
    const std::string path = WriteSample(root / ("offset-" + std::to_string(offset) + ".wav"), 3, frames, offset);
    auto stream = SampleStream::Open(path, offset, frames);
    CHECK(stream);
    StreamCursor cursor;
    for (const std::size_t frame : {std::size_t{0}, std::size_t{1}, kStreamBlockFrames - 600, kStreamBlockFrames - 1, kStreamBlockFrames,
                                    2 * kStreamBlockFrames + 5, frames - 1})
      CHECK(ReadWhenReady(*stream, cursor, frame) == Expected(3, frame));
    std::vector<std::int16_t> direct(5);
    CHECK(stream->ReadFrames(kStreamBlockFrames - 2, direct.data(), direct.size()) && direct[4] == Expected(3, kStreamBlockFrames + 2));
  }
  ClearReaders();
}

// What broke before: four 24-second samples, one per track, each started once (a one-step note at
// the start of a long pattern), on a card that takes 1.5 ms a read and 6 ms to open a file (with
// four files and two file slots, files are reopened). Then six voices: two on one file at different
// speeds, a second note on another file while its first still sounds (the first one outlives its
// note: it is let go and still reads), and the rest. Nothing may be late.
void FourLoops(const std::filesystem::path& root)
{
  const std::size_t frames = 24 * kSampleRate;
  std::vector<std::string> paths;
  for (int file = 0; file < 4; ++file) paths.push_back(WriteSample(root / ("loop-" + std::to_string(file) + ".wav"), 10 + file, frames));
  synth::audio::testing::SetCardDelays(1500, 6000);

  {
    std::vector<std::shared_ptr<SampleStream>> streams;
    for (const std::string& path : paths) streams.push_back(SampleStream::Open(path, 44, frames));
    for (const auto& stream : streams) CHECK(stream);
    for (const auto& stream : streams) WaitForStart(*stream, 0);
    const StreamCounters before = SampleStreamCounters();
    std::vector<Voice> voices(4);
    for (std::size_t index = 0; index < voices.size(); ++index)
    {
      voices[index].stream = streams[index];
      voices[index].file = 10 + static_cast<int>(index);
      voices[index].reader = index * 4;
    }
    const PlayResult result = Play(voices, 3.0);
    const StreamCounters counters = Since(before);
    Report("four loops", result, counters);
    CHECK(result.wrong == 0);
    CHECK(result.gaps == 0);
    CHECK(counters.lateBlocks == 0);
    CHECK(counters.cacheBlocks <= 64);
  }
  WaitForEmptyCache();

  {
    std::vector<std::shared_ptr<SampleStream>> streams;
    for (const std::string& path : paths) streams.push_back(SampleStream::Open(path, 44, frames));
    for (const auto& stream : streams) WaitForStart(*stream, 0);
    const StreamCounters before = SampleStreamCounters();
    std::vector<Voice> voices(6);
    const double fifth = std::pow(2.0, 7.0 / 12.0);
    const int files[] = {0, 0, 1, 1, 2, 3};
    const double rates[] = {1.0, fifth, 1.0, 1.0, 1.0, 1.0};
    const double starts[] = {0.0, 0.0, 0.0, 1.2, 0.0, 0.0};
    for (std::size_t index = 0; index < voices.size(); ++index)
    {
      voices[index].stream = streams[static_cast<std::size_t>(files[index])];
      voices[index].file = 10 + files[index];
      voices[index].reader = index;
      voices[index].rate = rates[index];
      voices[index].startsAt = starts[index];
    }
    const PlayResult result = Play(voices, 3.0, 2);
    const StreamCounters counters = Since(before);
    Report("six voices", result, counters);
    CHECK(result.wrong == 0);
    CHECK(result.gaps == 0);
    CHECK(counters.lateBlocks == 0);
  }
  WaitForEmptyCache();

  // A loop of a second and a half in the middle of a file: the look-ahead wraps to its start.
  {
    auto stream = SampleStream::Open(paths[2], 44, frames);
    CHECK(stream);
    const std::size_t start = kSampleRate, end = kSampleRate * 5 / 2;
    stream->PublishRegion(start, end, true);
    WaitForStart(*stream, start);
    const StreamCounters before = SampleStreamCounters();
    std::vector<Voice> voices(1);
    voices[0].stream = stream;
    voices[0].file = 12;
    voices[0].start = start;
    voices[0].end = end;
    voices[0].loop = true;
    voices[0].rate = 1.5;
    const PlayResult result = Play(voices, 2.5, 3);
    const StreamCounters counters = Since(before);
    Report("loop", result, counters);
    CHECK(result.wrong == 0);
    CHECK(result.gaps == 0);
  }
  WaitForEmptyCache();
  synth::audio::testing::SetCardDelays(0, 0);
}

// One file on two tracks: its blocks are read once, through one file slot.
void SharedFile(const std::filesystem::path& root)
{
  const std::size_t frames = 6 * kSampleRate;
  const std::string path = WriteSample(root / "shared.wav", 20, frames);
  auto first = SampleStream::Open(path, 44, frames);
  auto second = SampleStream::Open(path, 44, frames);
  CHECK(first && second);
  WaitForStart(*first, 0);
  const StreamCounters before = SampleStreamCounters();
  std::vector<Voice> voices(2);
  voices[0].stream = first;
  voices[1].stream = second;
  voices[0].file = voices[1].file = 20;
  voices[1].reader = 1;
  const PlayResult result = Play(voices, 1.0);
  const StreamCounters counters = Since(before);
  Report("shared file", result, counters);
  CHECK(result.wrong == 0 && result.gaps == 0);
  // One second and the look-ahead: about 17 blocks for one voice, not twice that.
  CHECK(counters.reads <= 20);
  CHECK(counters.opens <= 1);
}

// One stream on two tracks (a project load prepares a sample once for every track that has it), each
// with its own trim: one loops a second in the middle, the other plays from 3 s on. Both starts stay
// in, and the loop's look-ahead wraps at its own end, not the other track's.
void OneStreamTwoRegions(const std::filesystem::path& root)
{
  const std::size_t frames = 8 * kSampleRate;
  const std::string path = WriteSample(root / "two-regions.wav", 25, frames);
  synth::audio::testing::SetCardDelays(1500, 6000);
  {
    auto stream = SampleStream::Open(path, 44, frames);
    CHECK(stream);
    const StreamCounters before = SampleStreamCounters();
    std::vector<Voice> voices(2);
    for (Voice& voice : voices)
    {
      voice.stream = stream;
      voice.file = 25;
    }
    voices[0].start = kSampleRate;
    voices[0].end = 2 * kSampleRate;
    voices[0].loop = true;
    voices[0].startsAt = 0.3;
    voices[1].start = 3 * kSampleRate;
    voices[1].reader = 4;
    voices[1].startsAt = 0.6;
    const PlayResult result = Play(voices, 2.5, 4);
    const StreamCounters counters = Since(before);
    Report("one stream, two regions", result, counters);
    CHECK(result.wrong == 0);
    CHECK(result.gaps == 0);
  }
  synth::audio::testing::SetCardDelays(0, 0);
  WaitForEmptyCache();
}

// A guard holds its own file only: the others play on.
void GuardHoldsOneFile(const std::filesystem::path& root)
{
  const std::size_t frames = 6 * kSampleRate;
  const std::string held = WriteSample(root / "held.wav", 30, frames);
  const std::string other = WriteSample(root / "other.wav", 31, frames);
  auto heldStream = SampleStream::Open(held, 44, frames);
  auto otherStream = SampleStream::Open(other, 44, frames);
  CHECK(heldStream && otherStream);
  WaitForStart(*otherStream, 0);
  SampleFileGuard guard(held);
  const StreamCounters before = SampleStreamCounters();
  std::vector<Voice> voices(1);
  voices[0].stream = otherStream;
  voices[0].file = 31;
  const PlayResult result = Play(voices, 1.0);
  Report("guard", result, Since(before));
  CHECK(result.wrong == 0 && result.gaps == 0);
}

// A block is never written while a render pass may still read it. The cache holds exactly what a fast
// voice needs (its window and the sample's start), and a second voice reads one block behind it
// without saying so: each time the first one moves on, the only block the worker can take for it is
// the one the second is reading, in the middle of a pass that takes milliseconds (as on a busy
// board). The second voice may find its block gone (held frames), never another block's data. Then
// a storm of voices jumping back into what they just left, on a cache far too small.
void NoBlockReusedUnderAVoice(const std::filesystem::path& root)
{
  const std::size_t frames = 20 * kSampleRate;
  const std::string a = WriteSample(root / "storm-a.wav", 40, frames);
  const std::string b = WriteSample(root / "storm-b.wav", 41, frames);
  synth::audio::testing::SetCardDelays(300, 0);
  {
    synth::audio::testing::LimitCacheBlocks(6);
    auto stream = SampleStream::Open(a, 44, frames);
    CHECK(stream);
    WaitForStart(*stream, 0);
    const StreamCounters before = SampleStreamCounters();
    std::vector<Voice> voices(2);
    for (Voice& voice : voices)
    {
      voice.stream = stream;
      voice.file = 40;
      voice.rate = 3.0;
    }
    voices[0].from = 4 * kStreamBlockFrames;
    voices[1].from = 3 * kStreamBlockFrames;
    voices[1].reader = 1;
    voices[1].silent = true;
    voices[1].renderMicros = 1000;
    const PlayResult result = Play(voices, 2.0, 5);
    const StreamCounters counters = Since(before);
    Report("block taken under a voice", result, counters);
    CHECK(result.wrong == 0);
    CHECK(result.frames > result.gaps);  // the second voice did read blocks that were then taken
    CHECK(counters.cacheBlocks <= 6);
  }
  WaitForEmptyCache();
  {
    synth::audio::testing::LimitCacheBlocks(10);
    auto streamA = SampleStream::Open(a, 44, frames);
    auto streamB = SampleStream::Open(b, 44, frames);
    CHECK(streamA && streamB);
    const StreamCounters before = SampleStreamCounters();
    std::vector<Voice> voices(4);
    const double rates[] = {0.7, 1.3, 2.1, 3.0};
    for (std::size_t index = 0; index < voices.size(); ++index)
    {
      voices[index].stream = index % 2 == 0 ? streamA : streamB;
      voices[index].file = index % 2 == 0 ? 40 : 41;
      voices[index].reader = index;
      voices[index].rate = rates[index];
      voices[index].loop = true;
      voices[index].backJumps = 1;
      voices[index].renderMicros = 300;
    }
    const PlayResult result = Play(voices, 1.5, 7);
    const StreamCounters counters = Since(before);
    Report("eviction storm", result, counters);
    CHECK(result.wrong == 0);
    CHECK(counters.reads > 40);  // the cache was reused, over and over
    CHECK(counters.cacheBlocks <= 10);
  }
  synth::audio::testing::LimitCacheBlocks(0);
  synth::audio::testing::SetCardDelays(0, 0);
  WaitForEmptyCache();
}

}  // namespace

int main()
{
  const auto root = std::filesystem::temp_directory_path() / ("cardenza-stream-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);

  BasicReads(root);
  Offsets(root);
  WaitForEmptyCache();  // nothing loaded: no memory kept
  FourLoops(root);
  SharedFile(root);
  WaitForEmptyCache();
  OneStreamTwoRegions(root);
  GuardHoldsOneFile(root);
  WaitForEmptyCache();
  NoBlockReusedUnderAVoice(root);

  std::filesystem::remove_all(root);
  std::cout << "Sample stream passed\n";
  return 0;
}

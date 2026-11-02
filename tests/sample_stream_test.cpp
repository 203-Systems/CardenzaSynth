// Playing a sample from a file: what a voice reads is what the file holds, the cache stays small, and
// the file can be renamed and deleted while it is loaded.
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#include "Library/Synth/Audio/SampleStream.h"

using synth::audio::kStreamBlockFrames;
using synth::audio::SampleFileGuard;
using synth::audio::SampleStream;
using synth::audio::StreamCursor;

namespace {

std::int16_t Expected(std::size_t frame) { return static_cast<std::int16_t>(frame % 30011); }

std::string WriteRamp(const std::filesystem::path& path, std::size_t frames)
{
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  const char header[44]{};
  file.write(header, sizeof(header));
  for (std::size_t frame = 0; frame < frames; ++frame)
  {
    const std::int16_t value = Expected(frame);
    file.write(reinterpret_cast<const char*>(&value), sizeof(value));
  }
  return path.string();
}

// Like the render thread: asks for a frame, and comes back while it is not there yet.
std::int16_t ReadWhenReady(const SampleStream& stream, StreamCursor& cursor, std::size_t frame)
{
  for (int attempt = 0; attempt < 2000; ++attempt)
  {
    SampleStream::PublishReader(0, &stream, frame);
    const std::int16_t value = stream.Read(cursor, frame);
    if (cursor.block != nullptr && cursor.blockIndex == frame / kStreamBlockFrames) return value;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  assert(false && "the block never arrived");
  return 0;
}

}  // namespace

int main()
{
  const auto root = std::filesystem::temp_directory_path() / ("cardenza-stream-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  const std::size_t frames = 50 * kStreamBlockFrames + 100;
  const std::string path = WriteRamp(root / "long.wav", frames);

  {
    auto stream = SampleStream::Open(path, 44, frames);
    assert(stream && stream->FrameCount() == frames);
    // Every block arrives, in order and in the middle, up to the partial last block.
    StreamCursor cursor;
    for (std::size_t frame = 0; frame < frames; frame += 137) assert(ReadWhenReady(*stream, cursor, frame) == Expected(frame));
    assert(ReadWhenReady(*stream, cursor, frames - 1) == Expected(frames - 1));
    // A jump back (a loop, a new note) finds the block again.
    assert(ReadWhenReady(*stream, cursor, 5) == Expected(5));

    // A late block is no failure: the voice holds its last frame.
    StreamCursor late;
    late.last = 77;
    SampleStream::PublishReader(0, nullptr, 0);
    assert(stream->Read(late, 40 * kStreamBlockFrames + 3) == 77 || late.block != nullptr);

    // The start point stays cached, so a note can begin at once.
    stream->PublishStart(30 * kStreamBlockFrames);
    StreamCursor start;
    assert(ReadWhenReady(*stream, start, 30 * kStreamBlockFrames) == Expected(30 * kStreamBlockFrames));

    // Direct reads for waveforms and copies.
    std::vector<std::int16_t> direct(600);
    assert(stream->ReadFrames(2000, direct.data(), direct.size()) && direct[599] == Expected(2599));
    assert(!stream->ReadFrames(frames - 10, direct.data(), 11));

    // Under a guard the file can be renamed; the stream follows.
    const std::string moved = (root / "moved.wav").string();
    {
      SampleFileGuard guard(path);
      std::filesystem::rename(path, moved);
      guard.MovedTo(moved);
    }
    assert(stream->ReadFrames(3000, direct.data(), 10) && direct[0] == Expected(3000));
    // Several voices and streams can run; a slot is free again after release.
    std::vector<std::shared_ptr<SampleStream>> many;
    for (int index = 0; index < 7; ++index)
    {
      many.push_back(SampleStream::Open(moved, 44, frames));
      assert(many.back());
    }
    assert(!SampleStream::Open(moved, 44, frames));  // eight in all
    many.clear();
    std::shared_ptr<SampleStream> again;
    for (int attempt = 0; attempt < 2000 && !again; ++attempt)
    {
      again = SampleStream::Open(moved, 44, frames);
      if (!again) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(again);

    // Deleted while loaded: silence, no crash.
    {
      SampleFileGuard guard(moved);
      std::filesystem::remove(moved);
    }
    assert(!stream->ReadFrames(4000, direct.data(), 10));
    StreamCursor gone;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
      SampleStream::PublishReader(0, stream.get(), 45 * kStreamBlockFrames);
      (void)stream->Read(gone, 45 * kStreamBlockFrames);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    SampleStream::PublishReader(0, nullptr, 0);
  }

  std::filesystem::remove_all(root);
  std::cout << "Sample stream passed\n";
  return 0;
}

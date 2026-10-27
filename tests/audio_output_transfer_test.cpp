#include "Devices/AudioOutputTransfer.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

using device::audio::WriteAllPcm;
using device::audio::WriteAttempt;

void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

constexpr std::array<std::int16_t, 6> kSamples{{-300, 42, -1, 0, 32767, -32768}};

void CompleteAndPartialWrites()
{
  unsigned calls = 0;
  auto result = WriteAllPcm(kSamples.data(), kSamples.size(),
      [&](const void* source, std::size_t remaining) {
        ++calls;
        Require(source == kSamples.data() && remaining == sizeof(kSamples), "full write request changed");
        return WriteAttempt{remaining, true};
      });
  Require(result.complete && result.bytesWritten == sizeof(kSamples) &&
          result.shortWrites == 0 && calls == 1, "full write did not complete once");

  const std::array<std::size_t, 3> acceptedBytes{{4, 2, 6}};
  std::vector<std::int16_t> accepted;
  calls = 0;
  result = WriteAllPcm(kSamples.data(), kSamples.size(),
      [&](const void* source, std::size_t remaining) {
        Require(calls < acceptedBytes.size(), "partial write retried after completion");
        const auto* next = static_cast<const std::int16_t*>(source);
        Require(next == kSamples.data() + accepted.size(), "partial write repeated or skipped a prefix");
        Require(remaining == (kSamples.size() - accepted.size()) * sizeof(*next), "remaining byte count incorrect");
        const std::size_t count = acceptedBytes[calls++];
        accepted.insert(accepted.end(), next, next + count / sizeof(*next));
        return WriteAttempt{count, true};
      });
  Require(result.complete && result.bytesWritten == sizeof(kSamples) && result.shortWrites == 2 && calls == 3,
          "partial writes did not complete correctly");
  Require(accepted == std::vector<std::int16_t>(kSamples.begin(), kSamples.end()), "PCM order changed");
}

void TimeoutStopsOnZeroProgress()
{
  unsigned calls = 0;
  const auto result = WriteAllPcm(kSamples.data(), kSamples.size(),
      [&](const void* source, std::size_t remaining) {
        ++calls;
        Require(calls <= 2, "zero progress was retried");
        if (calls == 1) return WriteAttempt{2, true};
        Require(source == kSamples.data() + 1 && remaining == sizeof(kSamples) - 2,
                "timeout retry did not retain the exact tail");
        return WriteAttempt{0, true};
      });
  Require(!result.complete && result.bytesWritten == 2 && result.shortWrites == 2 && calls == 2,
          "timeout with zero progress did not stop");
}

void FatalStatuses()
{
  for (const std::size_t count : {std::size_t{0}, std::size_t{2}, sizeof(kSamples)})
  {
    unsigned calls = 0;
    const auto result = WriteAllPcm(kSamples.data(), kSamples.size(),
        [&](const void*, std::size_t) {
          ++calls;
          return WriteAttempt{count, false};
        });
    Require(!result.complete && result.bytesWritten == count && calls == 1,
            "fatal status was retried or reported complete");
    Require(result.shortWrites == (count < sizeof(kSamples) ? 1U : 0U), "fatal short write count incorrect");
  }
}

void MalformedByteCounts()
{
  for (const std::size_t count : {std::size_t{1}, sizeof(kSamples)})
  {
    unsigned calls = 0;
    const auto result = WriteAllPcm(kSamples.data(), kSamples.size(),
        [&](const void*, std::size_t) {
          ++calls;
          Require(calls <= 2, "malformed byte count was retried");
          return calls == 1 ? WriteAttempt{2, true} : WriteAttempt{count, true};
        });
    Require(!result.complete && result.bytesWritten == 2 && result.shortWrites == 1 && calls == 2,
            "malformed count advanced or discarded the valid prefix");
  }
}

void EmptyInvalidAndBoundedRequests()
{
  unsigned calls = 0;
  const auto neverWrite = [&](const void*, std::size_t) {
    ++calls;
    return WriteAttempt{};
  };
  auto result = WriteAllPcm(nullptr, 0, neverWrite);
  Require(result.complete && result.bytesWritten == 0 && result.shortWrites == 0 && calls == 0,
          "empty write invoked the writer");
  result = WriteAllPcm(nullptr, 1, neverWrite);
  Require(!result.complete && result.bytesWritten == 0 && calls == 0, "null PCM was passed to the writer");
  result = WriteAllPcm(kSamples.data(), (std::numeric_limits<std::size_t>::max)() / sizeof(std::int16_t) + 1,
                       neverWrite);
  Require(!result.complete && result.bytesWritten == 0 && calls == 0, "overflowing frame count was accepted");

  result = WriteAllPcm(kSamples.data(), kSamples.size(),
      [&](const void* source, std::size_t remaining) {
        Require(source == kSamples.data() + calls && remaining == (kSamples.size() - calls) * 2,
                "single-frame retry corrupted the tail");
        ++calls;
        Require(calls <= kSamples.size(), "retry count exceeded frame count");
        return WriteAttempt{sizeof(std::int16_t), true};
      });
  Require(result.complete && calls == kSamples.size() && result.shortWrites == kSamples.size() - 1,
          "smallest valid progress did not finish within the frame bound");
}

}  // namespace

int main()
{
  try
  {
    CompleteAndPartialWrites();
    TimeoutStopsOnZeroProgress();
    FatalStatuses();
    MalformedByteCounts();
    EmptyInvalidAndBoundedRequests();
    std::cout << "Audio output transfer tests passed\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << "Audio output transfer test failed: " << error.what() << '\n';
    return 1;
  }
}

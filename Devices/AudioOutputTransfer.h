#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace device::audio {

struct WriteAttempt {
  std::size_t bytesWritten = 0;
  bool retryable = false;
};

struct WriteResult {
  std::size_t bytesWritten = 0;
  unsigned shortWrites = 0;
  bool complete = false;
};

// The writer accepts a prefix of the remaining mono PCM. It owns the timeout/deadline;
// successful and recoverable statuses set retryable, fatal statuses clear it.
// Each retry consumes at least one frame, so there are at most `frames` calls.
template<class Writer>
WriteResult WriteAllPcm(const std::int16_t* samples, std::size_t frames, Writer&& writer)
{
  WriteResult result;
  if (frames == 0)
  {
    result.complete = true;
    return result;
  }
  if (samples == nullptr || frames > (std::numeric_limits<std::size_t>::max)() / sizeof(*samples))
  {
    return result;
  }

  const std::size_t totalBytes = frames * sizeof(*samples);
  while (result.bytesWritten < totalBytes)
  {
    const std::size_t remainingBytes = totalBytes - result.bytesWritten;
    const WriteAttempt attempt = writer(
        static_cast<const void*>(samples + result.bytesWritten / sizeof(*samples)), remainingBytes);
    if (attempt.bytesWritten > remainingBytes || attempt.bytesWritten % sizeof(*samples) != 0)
    {
      return result;
    }
    // Include valid zero-progress attempts, but never wrap a diagnostic counter.
    if (attempt.bytesWritten < remainingBytes &&
        result.shortWrites < (std::numeric_limits<unsigned>::max)())
    {
      ++result.shortWrites;
    }
    result.bytesWritten += attempt.bytesWritten;
    if (!attempt.retryable || attempt.bytesWritten == 0)
    {
      return result;
    }
  }
  result.complete = true;
  return result;
}

}  // namespace device::audio

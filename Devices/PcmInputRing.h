#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace device::audio {

// The caller serializes access. A write either fits in full or changes nothing.
template<std::size_t Capacity>
class PcmInputRing
{
  static_assert(Capacity > 0);
public:
  void Reset() { head_ = count_ = 0; }
  std::size_t Size() const { return count_; }

  bool Write(const std::int16_t* samples, std::size_t frames)
  {
    if (frames > Capacity - count_ || (samples == nullptr && frames != 0)) return false;
    if (frames == 0) return true;
    const std::size_t tail = (head_ + count_) % Capacity;
    const std::size_t first = std::min(frames, Capacity - tail);
    std::memcpy(samples_.data() + tail, samples, first * sizeof(*samples));
    if (first < frames)
      std::memcpy(samples_.data(), samples + first, (frames - first) * sizeof(*samples));
    count_ += frames;
    return true;
  }

  std::size_t Read(std::int16_t* destination, std::size_t frames)
  {
    const std::size_t read = std::min(frames, count_);
    if (destination == nullptr || read == 0) return 0;
    const std::size_t first = std::min(read, Capacity - head_);
    std::memcpy(destination, samples_.data() + head_, first * sizeof(*destination));
    if (first < read)
      std::memcpy(destination + first, samples_.data(), (read - first) * sizeof(*destination));
    head_ = (head_ + read) % Capacity;
    count_ -= read;
    return read;
  }

private:
  std::array<std::int16_t, Capacity> samples_{};
  std::size_t head_ = 0;
  std::size_t count_ = 0;
};

}  // namespace device::audio

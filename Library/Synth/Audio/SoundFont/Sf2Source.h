#pragma once

// Where a SoundFont's bytes come from: a file on the card on the device, a buffer in the tests.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace synth::audio::sf2 {

class ByteSource {
public:
  virtual ~ByteSource() = default;
  virtual std::uint64_t Size() const = 0;
  // Exactly `count` bytes from `offset`; false when they are not all there. `destination` may be in
  // PSRAM (a preset's samples go there): a source on the card reads through a buffer in internal
  // RAM, since the card's DMA does not reach PSRAM.
  virtual bool Read(std::uint64_t offset, void* destination, std::size_t count) = 0;
};

class MemorySource final : public ByteSource {
public:
  MemorySource(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
  std::uint64_t Size() const override { return size_; }
  bool Read(std::uint64_t offset, void* destination, std::size_t count) override
  {
    if (offset > size_ || count > size_ - offset) return false;
    std::memcpy(destination, data_ + offset, count);
    return true;
  }

private:
  const std::uint8_t* data_;
  std::size_t size_;
};

// Reads through one window of the source: the tables are walked a few bytes at a time, and each read
// of the card is a seek and a sector. The window is the caller's (internal RAM on the device: the
// card's DMA does not reach PSRAM).
class Reader {
public:
  Reader(ByteSource& source, std::uint8_t* window, std::size_t windowBytes) : source_(source), window_(window), windowBytes_(windowBytes) {}

  std::uint64_t Size() const { return source_.Size(); }

  bool Read(std::uint64_t offset, void* destination, std::size_t count)
  {
    if (count == 0) return true;
    const std::uint64_t size = source_.Size();
    if (offset > size || count > size - offset) return false;
    if (count > windowBytes_) return source_.Read(offset, destination, count);
    if (offset < windowOffset_ || offset + count > windowOffset_ + windowFilled_)
    {
      const std::uint64_t left = size - offset;
      const std::size_t fill = left < windowBytes_ ? static_cast<std::size_t>(left) : windowBytes_;
      windowFilled_ = 0;
      if (!source_.Read(offset, window_, fill)) return false;
      windowOffset_ = offset;
      windowFilled_ = fill;
    }
    std::memcpy(destination, window_ + (offset - windowOffset_), count);
    return true;
  }

private:
  ByteSource& source_;
  std::uint8_t* window_;
  std::size_t windowBytes_;
  std::uint64_t windowOffset_ = 0;
  std::size_t windowFilled_ = 0;
};

// The file is little-endian whatever the machine is.
inline std::uint16_t U16(const std::uint8_t* bytes) { return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8)); }
inline std::int16_t S16(const std::uint8_t* bytes) { return static_cast<std::int16_t>(U16(bytes)); }
inline std::uint32_t U32(const std::uint8_t* bytes)
{
  return static_cast<std::uint32_t>(bytes[0]) | (static_cast<std::uint32_t>(bytes[1]) << 8) | (static_cast<std::uint32_t>(bytes[2]) << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

}  // namespace synth::audio::sf2

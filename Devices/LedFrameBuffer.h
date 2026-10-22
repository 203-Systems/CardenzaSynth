#pragma once

#include <algorithm>
#include <array>

#include "Devices/Device.h"

namespace Device::LED::detail {

struct LedFrame {
  std::array<std::uint8_t, kLedCount> levels{};
  std::uint8_t master = kBrightnessMax;
  bool enabled = false;
};

// Build a new frame while the last published frame keeps driving the LEDs.
// Outside a frame transaction, setters retain their immediate-update behavior.
class LedFrameBuffer {
public:
  const LedFrame& Current() const { return current_; }

  void BeginFrame() { pending_ = current_; building_ = true; }
  bool Present() { building_ = false; return Publish(); }

  bool SetKey(std::size_t row, std::size_t column, std::uint8_t level) {
    if (row >= kRows || column >= kColumns) return false;
    pending_.levels[row * kColumns + column] = std::min(level, kBrightnessMax);
    return PublishUnlessBuilding();
  }
  bool Fill(std::uint8_t level) {
    pending_.levels.fill(std::min(level, kBrightnessMax));
    return PublishUnlessBuilding();
  }
  bool SetMaster(std::uint8_t level) {
    pending_.master = std::min(level, kBrightnessMax);
    return PublishUnlessBuilding();
  }
  bool SetEnabled(bool enabled) {
    pending_.enabled = enabled;
    return PublishUnlessBuilding();
  }

private:
  bool PublishUnlessBuilding() { return !building_ && Publish(); }
  bool Publish() {
    if (current_.levels == pending_.levels && current_.master == pending_.master && current_.enabled == pending_.enabled)
      return false;
    current_ = pending_;
    return true;
  }

  LedFrame current_{};
  LedFrame pending_{};
  bool building_ = false;
};

}  // namespace Device::LED::detail

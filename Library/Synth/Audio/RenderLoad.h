#pragma once

#include <algorithm>
#include <cstdint>

namespace synth::audio {

// Whether the voices are too much for the audio core, from how long each render takes: one longer than
// the sound it makes eats into the DMA queue (about 35 ms), a shorter one gives that back. The time
// between renders would not do: it follows the output clock, and the smallest difference between that
// and our budget adds up, until a voice dies every half minute with nothing wrong.
class RenderLoad {
public:
  static constexpr std::int64_t kLimitMicros = 12000;

  // True when a voice has to go.
  bool Add(std::int64_t renderMicros, std::int64_t budgetMicros)
  {
    // One long render (a preset loading, flash being written) is over at once and says nothing about
    // the voices, so it counts as one block at most.
    debt_ = (std::max<std::int64_t>)(0, debt_ + (std::min)(renderMicros - budgetMicros, budgetMicros));
    if (debt_ < kLimitMicros) return false;
    // Half: give the voice that goes a moment to show in the render times before the next one.
    debt_ /= 2;
    return true;
  }

private:
  std::int64_t debt_ = 0;
};

}  // namespace synth::audio

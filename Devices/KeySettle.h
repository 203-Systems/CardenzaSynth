#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace Device::Keyboard {

// A second opinion on keys whose wrong reading does the most damage. The HAL debounces for 3 ms, which
// is right for a contact, not for a signal that is off for longer: a key that chatters would be a row
// of taps, and one that reads as held for no reason would hijack every other key. So a key's state
// only changes once the reading has held long enough, and a key that reads as held for implausibly
// long counts as released until it really lets go. (Devices/Cardenza/Device.cpp uses it on the left
// column; polled once per frame.)
template <std::size_t kKeys>
class KeySettle {
public:
  static constexpr std::int64_t kSettleUs = 12000;
  static constexpr std::int64_t kLongestHoldUs = 15000000;

  bool Update(std::size_t key, bool reading, std::int64_t nowUs)
  {
    State& state = states_[key];
    if (state.ignored)
    {
      // Stays released until the reading has been off for the settle time.
      if (reading) state.offSince = nowUs;
      else if (nowUs - state.offSince >= kSettleUs) state.ignored = false;
      return false;
    }
    if (reading == state.shown)
    {
      if (state.pending) ++glitches_;
      state.pending = false;
    }
    else if (!state.pending || reading != state.candidate)
    {
      if (state.pending) ++glitches_;
      state.pending = true;
      state.candidate = reading;
      state.since = nowUs;
    }
    else if (nowUs - state.since >= kSettleUs)
    {
      state.pending = false;
      state.shown = reading;
      state.heldSince = nowUs;
    }
    if (state.shown && nowUs - state.heldSince >= kLongestHoldUs)
    {
      state = State{};
      state.ignored = true;
      state.offSince = nowUs;
      ++stuck_;
    }
    return state.shown;
  }

  std::uint32_t Glitches() const { return glitches_; }
  std::uint32_t Stuck() const { return stuck_; }

private:
  struct State {
    bool shown = false;
    bool pending = false;
    bool candidate = false;
    bool ignored = false;
    std::int64_t since = 0;
    std::int64_t heldSince = 0;
    std::int64_t offSince = 0;
  };
  std::array<State, kKeys> states_{};
  std::uint32_t glitches_ = 0;
  std::uint32_t stuck_ = 0;
};

}  // namespace Device::Keyboard

#pragma once

// One preset of a SoundFont, ready to play: its regions with the preset's and the instrument's
// generators merged and every range intersected, and the samples they play, in memory. Built on the
// loading thread (it reads the card and allocates); the render thread only reads it.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "Library/Synth/Audio/SoundFont/Sf2File.h"
#include "Library/Synth/Audio/SoundFont/Sf2Format.h"

namespace synth::audio::sf2 {

// An envelope as the file gives it: times in timecents, the sustain in the envelope's own unit.
struct Envelope {
  std::int16_t delay = -12000;
  std::int16_t attack = -12000;
  std::int16_t hold = -12000;
  std::int16_t decay = -12000;
  std::int16_t sustain = 0;  // volume: attenuation in cB; modulation: 0.1 % below the peak
  std::int16_t release = -12000;
  std::int16_t keyToHold = 0;  // timecents per key below 60
  std::int16_t keyToDecay = 0;
};

struct Region {
  std::uint8_t keyLow = 0;
  std::uint8_t keyHigh = 127;
  std::uint8_t velLow = 0;
  std::uint8_t velHigh = 127;
  std::uint16_t slot = 0;  // the sample it plays
  std::uint8_t mode = 0;   // 0 plays once, 1 loops, 3 loops while the key is down and then plays on
  std::uint8_t exclusiveClass = 0;
  std::int8_t fixedKey = -1;
  std::int8_t fixedVelocity = -1;
  std::uint8_t rootKey = 60;
  std::int16_t scaleTuning = 100;  // cents per key
  std::int16_t tuneCents = 0;      // coarse and fine tune and the sample's pitch correction
  std::uint32_t start = 0;         // frames in its sample slot
  std::uint32_t end = 0;           // one past the last frame
  std::uint32_t loopStart = 0;
  std::uint32_t loopEnd = 0;  // one past the last frame of the loop
  std::uint32_t rateRatioQ24 = 1u << 24;  // the sample's rate over the output's
  std::int16_t attenuationCb = 0;
  std::int16_t filterFc = 13500;  // absolute cents
  std::int16_t filterQ = 0;       // cB
  std::int16_t modLfoToPitch = 0;
  std::int16_t vibLfoToPitch = 0;
  std::int16_t modEnvToPitch = 0;
  std::int16_t modLfoToFilter = 0;
  std::int16_t modEnvToFilter = 0;
  std::int16_t modLfoToVolume = 0;  // cB
  std::int16_t modLfoDelay = -12000;
  std::int16_t modLfoFreq = 0;  // absolute cents
  std::int16_t vibLfoDelay = -12000;
  std::int16_t vibLfoFreq = 0;
  Envelope modEnv;
  Envelope volEnv;

  bool Sounds(unsigned key, unsigned velocity) const { return key >= keyLow && key <= keyHigh && velocity >= velLow && velocity <= velHigh; }
};

class Program {
public:
  struct Result {
    Status status = Status::Ok;
    std::size_t neededBytes = 0;  // what the preset takes (known from TooBig on)
    std::unique_ptr<Program> program;
  };
  // Reads the preset at `position` of the file's list. With more than `budgetBytes` of regions and
  // samples it is refused (TooBig) and nothing is loaded.
  // The card loader can also check the space available at each allocation.
  using RoomCheck = bool (*)(std::uint64_t bytes);
  static Result Build(File& file, std::size_t position, std::size_t budgetBytes, RoomCheck roomFor = nullptr);

  std::size_t RegionCount() const { return regionCount_; }
  const Region& RegionAt(std::size_t index) const { return regions_[index]; }
  // A slot's frames; one zero frame follows the last.
  const std::int16_t* SlotData(std::size_t slot) const { return slots_[slot].data.get(); }
  std::size_t SlotCount() const { return slotCount_; }
  std::uint32_t SlotFrames(std::size_t slot) const { return slots_[slot].frames; }
  std::size_t Bytes() const { return bytes_; }

  // The regions a key at a velocity sounds, in the file's order; how many were written.
  std::size_t Find(unsigned key, unsigned velocity, const Region** regions, std::size_t capacity) const;

private:
  struct Slot {
    std::uint32_t frames = 0;
    std::unique_ptr<std::int16_t[]> data;
  };

  std::unique_ptr<Region[]> regions_;
  std::size_t regionCount_ = 0;
  std::unique_ptr<Slot[]> slots_;
  std::size_t slotCount_ = 0;
  std::size_t bytes_ = 0;
};

}  // namespace synth::audio::sf2

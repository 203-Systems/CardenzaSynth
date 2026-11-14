#pragma once

// One sounding region of a SoundFont preset. Integers only: the sample is read with a 16.16 position
// and linear interpolation; the envelopes, the LFOs, the pitch and the filter move once every
// kControlFrames frames and the gain glides between those points frame by frame. A voice renders a
// run of frames at a time and adds itself to the track's accumulator. Nothing here allocates.

#include <cstddef>
#include <cstdint>

#include "Library/Synth/Audio/SoundFont/Sf2Program.h"

namespace synth::audio::sf2 {

// What the track adds to every region of its preset (the TONE page).
struct Tone {
  std::int16_t tuneCents = 0;
  std::int16_t cutoffCents = 0;
  std::int16_t resonanceCb = 0;
  std::int16_t attackTimecents = 0;
  std::int16_t releaseTimecents = 0;
  std::int16_t attenuationCb = 0;
};

class Voice {
public:
  // A note begins. `data` is the region's sample slot.
  void Start(const Region& region, const std::int16_t* data, unsigned key, unsigned velocity, const Tone& tone, std::uint32_t order);
  // The same, once the note this voice plays now has faded out (one control step): a voice taken
  // from an older note never cuts it off with a click.
  void StartAfterFade(const Region& region, const std::int16_t* data, unsigned key, unsigned velocity, const Tone& tone, std::uint32_t order);
  // The key went up: the release begins, and a region that loops while held plays on to its end.
  void Release();
  // Let go and gone in `controlSteps` control steps, whatever the release time says (an exclusive
  // class, the same key again). A note waiting to start is dropped.
  void Choke(unsigned controlSteps);
  // To silence in `controlSteps` control steps, then off (or on to the note waiting to start).
  void Fade(unsigned controlSteps);
  void Stop();

  // Adds `frames` frames of the voice to `accumulator`; a voice at full level spans the int16 range.
  void Render(std::int32_t* accumulator, std::size_t frames);

  bool Active() const { return stage_ != Stage::Off; }
  // Its key is down (or the note waiting to start is).
  bool Held() const { return Active() && (hasPending_ ? !pendingReleased_ : !released_); }
  bool Fading() const { return fading_; }
  bool Waiting() const { return hasPending_; }
  unsigned Key() const { return hasPending_ ? pending_.key : key_; }
  unsigned ExclusiveClass() const { return hasPending_ ? pending_.region->exclusiveClass : exclusiveClass_; }
  std::uint32_t Order() const { return hasPending_ ? pending_.order : order_; }
  // Its gain now, Q30 (1 << 30 is unity).
  std::int32_t Gain() const { return gain_; }
  // What it adds to its position per frame now, 16.16.
  std::uint32_t PositionStepQ16() const { return step_; }

private:
  enum class Stage : std::uint8_t { Off, Delay, Attack, Hold, Decay, Sustain, Release };
  struct Pending {
    const Region* region = nullptr;
    const std::int16_t* data = nullptr;
    Tone tone;
    std::uint32_t order = 0;
    std::uint8_t key = 0;
    std::uint8_t velocity = 0;
  };

  void ControlStep();
  void AdvanceVolume();
  void AdvanceModulation();
  void RenderRun(std::int32_t* accumulator, std::size_t frames);

  const Region* region_ = nullptr;
  const std::int16_t* data_ = nullptr;

  // Where it reads: a frame of the slot and 16 bits of the way to the next.
  std::uint32_t index_ = 0;
  std::uint32_t fraction_ = 0;
  std::uint32_t step_ = 1u << 16;
  std::uint32_t end_ = 0;
  std::uint32_t loopStart_ = 0;
  std::uint32_t loopEnd_ = 0;
  bool looping_ = false;
  std::uint32_t rateRatioQ24_ = 1u << 24;

  std::int32_t gain_ = 0;      // Q30
  std::int32_t gainStep_ = 0;  // per frame
  std::size_t chunkLeft_ = 0;

  // The volume envelope: its rise is linear in amplitude, everything after it linear in decibels
  // (1000 cB, 100 %, in the decay or release time).
  Stage stage_ = Stage::Off;
  std::uint32_t counterQ8_ = 0;  // control steps left of the delay or the hold, Q8
  std::uint32_t holdStepsQ8_ = 0;
  std::uint32_t attackLevel_ = 0;  // Q30
  std::uint32_t attackStep_ = 0;
  std::int32_t attenuation_ = 0;  // below the peak, 1/4096 cB
  std::int32_t decayStep_ = 0;
  std::int32_t sustain_ = 0;
  std::int32_t releaseStep_ = 0;
  std::int32_t staticAttenuation_ = 0;  // the region's, the velocity's and the track's, 1/4096 cB

  std::int32_t fade_ = 32768;  // Q15, on top of everything
  std::int32_t fadeStep_ = 0;
  bool fading_ = false;

  // The modulation envelope, Q30, linear in every stage.
  Stage modStage_ = Stage::Off;
  std::uint32_t modCounterQ8_ = 0;
  std::uint32_t modHoldStepsQ8_ = 0;
  std::uint32_t modValue_ = 0;
  std::uint32_t modAttackStep_ = 0;
  std::uint32_t modDecayStep_ = 0;
  std::uint32_t modSustain_ = 0;
  std::uint32_t modReleaseStep_ = 0;

  std::uint32_t modLfoPhase_ = 0;
  std::uint32_t modLfoStep_ = 0;
  std::uint32_t modLfoDelayQ8_ = 0;
  std::uint32_t vibLfoPhase_ = 0;
  std::uint32_t vibLfoStep_ = 0;
  std::uint32_t vibLfoDelayQ8_ = 0;

  std::int32_t baseCentsQ8_ = 0;
  bool pitchModulated_ = false;

  // A two-integrator state-variable lowpass; coefficients Q30, state in sample units times 256.
  bool filtered_ = false;
  bool filterModulated_ = false;
  bool filterPrimed_ = false;
  std::int32_t baseCutoffQ8_ = 0;
  std::int32_t resonanceCb_ = 0;
  std::int32_t a1_ = 0;
  std::int32_t a2_ = 0;
  std::int32_t a3_ = 0;
  std::int32_t a1Step_ = 0;
  std::int32_t a2Step_ = 0;
  std::int32_t a3Step_ = 0;
  std::int32_t ic1_ = 0;
  std::int32_t ic2_ = 0;

  std::uint8_t key_ = 0;
  std::uint8_t exclusiveClass_ = 0;
  bool released_ = false;
  std::uint32_t order_ = 0;

  bool hasPending_ = false;
  bool pendingReleased_ = false;
  Pending pending_;
};

}  // namespace synth::audio::sf2

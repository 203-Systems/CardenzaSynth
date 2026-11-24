#include "Library/Synth/Audio/SoundFont/Sf2Voice.h"

#include <algorithm>

#include "Library/Synth/Audio/SoundFont/Sf2Tables.h"

#if defined(__GNUC__) || defined(__clang__)
#define SF2_ALWAYS_INLINE __attribute__((always_inline))
#else
#define SF2_ALWAYS_INLINE
#endif

namespace synth::audio::sf2 {

namespace {

constexpr std::uint32_t kUnityQ30 = 1u << 30;
constexpr std::int32_t kFullScaleQ12 = 1000 << 12;  // an envelope's 100 %: 100 dB
constexpr std::int32_t kStateLimit = 1 << 30;       // the filter's state never wraps
constexpr std::uint32_t kUnityQ8Steps = 256;        // one control step, Q8

// What a stage that runs 100 % in `stepsQ8` control steps moves per control step (`full` is 100 %).
std::uint32_t RatePerStep(std::uint64_t full, std::uint32_t stepsQ8)
{
  if (stepsQ8 <= kUnityQ8Steps) return static_cast<std::uint32_t>(full);
  return static_cast<std::uint32_t>((full * kUnityQ8Steps) / stepsQ8);
}

// A triangle from 0 up to +1, down to -1 and back, Q15, as the SoundFont's LFOs begin.
std::int32_t Triangle(std::uint32_t phase)
{
  const std::int32_t p = static_cast<std::int32_t>(phase >> 16);
  if (p < 16384) return 2 * p;
  if (p < 49152) return 2 * (32768 - p);
  return 2 * (p - 65536);
}

// A modulation source (Q15 or, for the envelope, Q15 from 0 to 1) times an amount in cents, in 1/256
// cent.
std::int32_t CentsQ8(std::int32_t sourceQ15, std::int32_t cents) { return (sourceQ15 * cents) / 128; }

std::int32_t Saturate(std::int64_t value) { return static_cast<std::int32_t>((std::clamp)(value, std::int64_t{-kStateLimit}, std::int64_t{kStateLimit})); }

}  // namespace

void Voice::Start(const Region& region, const std::int16_t* data, unsigned key, unsigned velocity, const Tone& tone, std::uint32_t order)
{
  hasPending_ = false;
  pendingReleased_ = false;
  region_ = &region;
  data_ = data;
  key_ = static_cast<std::uint8_t>(key);
  exclusiveClass_ = region.exclusiveClass;
  order_ = order;
  released_ = false;
  fade_ = 32768;
  fadeStep_ = 0;
  fading_ = false;

  const int soundingKey = region.fixedKey >= 0 ? region.fixedKey : static_cast<int>(key);
  const unsigned soundingVelocity = region.fixedVelocity >= 0 ? static_cast<unsigned>(region.fixedVelocity) : velocity;
  const int keyFrom60 = 60 - soundingKey;

  index_ = region.start;
  fraction_ = 0;
  end_ = region.end;
  loopStart_ = region.loopStart;
  loopEnd_ = region.loopEnd;
  looping_ = region.mode == 1 || region.mode == 3;
  rateRatioQ24_ = region.rateRatioQ24;
  baseCentsQ8_ = (region.scaleTuning * (soundingKey - region.rootKey) + region.tuneCents + tone.tuneCents) * 256;
  pitchModulated_ = region.modLfoToPitch != 0 || region.vibLfoToPitch != 0 || region.modEnvToPitch != 0;
  step_ = PositionStep(baseCentsQ8_, rateRatioQ24_);

  // A division a note, not per frame (the attenuation is at most 1440 cB: within 32 bits times 127).
  const std::int32_t depth = tone.velocityDepth > 127 ? 127 : tone.velocityDepth;
  staticAttenuation_ = (region.attenuationCb + tone.attenuationCb) * 4096 + VelocityAttenuationQ12(soundingVelocity) * depth / 127;

  const Envelope& vol = region.volEnv;
  stage_ = Stage::Delay;
  counterQ8_ = ControlStepsQ8(vol.delay);
  attackLevel_ = 0;
  attackStep_ = RatePerStep(kUnityQ30, ControlStepsQ8(vol.attack + tone.attackTimecents));
  holdStepsQ8_ = ControlStepsQ8(vol.hold + keyFrom60 * vol.keyToHold);
  attenuation_ = 0;
  decayStep_ = static_cast<std::int32_t>(RatePerStep(kFullScaleQ12, ControlStepsQ8(vol.decay + keyFrom60 * vol.keyToDecay)));
  sustain_ = (std::clamp)(static_cast<std::int32_t>(vol.sustain), std::int32_t{0}, std::int32_t{1440}) * 4096;
  releaseStep_ = static_cast<std::int32_t>(RatePerStep(kFullScaleQ12, ControlStepsQ8(vol.release + tone.releaseTimecents)));

  const Envelope& mod = region.modEnv;
  modStage_ = Stage::Delay;
  modCounterQ8_ = ControlStepsQ8(mod.delay);
  modValue_ = 0;
  modAttackStep_ = RatePerStep(kUnityQ30, ControlStepsQ8(mod.attack));
  modHoldStepsQ8_ = ControlStepsQ8(mod.hold + keyFrom60 * mod.keyToHold);
  modDecayStep_ = RatePerStep(kUnityQ30, ControlStepsQ8(mod.decay + keyFrom60 * mod.keyToDecay));
  modSustain_ = static_cast<std::uint32_t>((static_cast<std::uint64_t>(1000 - (std::clamp)(static_cast<int>(mod.sustain), 0, 1000)) << 30) / 1000);
  modReleaseStep_ = RatePerStep(kUnityQ30, ControlStepsQ8(mod.release));

  modLfoPhase_ = 0;
  modLfoStep_ = LfoStep(region.modLfoFreq);
  modLfoDelayQ8_ = ControlStepsQ8(region.modLfoDelay);
  vibLfoPhase_ = 0;
  vibLfoStep_ = LfoStep(region.vibLfoFreq);
  vibLfoDelayQ8_ = ControlStepsQ8(region.vibLfoDelay);

  baseCutoffQ8_ = (region.filterFc + tone.cutoffCents) * 256;
  resonanceCb_ = (std::clamp)(region.filterQ + tone.resonanceCb, 0, kMaxResonanceCb);
  filterModulated_ = region.modLfoToFilter != 0 || region.modEnvToFilter != 0;
  // Open, without resonance and unmodulated, the filter is not run at all.
  filtered_ = filterModulated_ || resonanceCb_ > 0 || baseCutoffQ8_ < (13000 << 8);
  filterPrimed_ = false;
  a1Step_ = a2Step_ = a3Step_ = 0;
  ic1_ = ic2_ = 0;

  gain_ = 0;
  gainStep_ = 0;
  chunkLeft_ = 0;
}

void Voice::StartAfterFade(const Region& region, const std::int16_t* data, unsigned key, unsigned velocity, const Tone& tone, std::uint32_t order)
{
  if (!Active())
  {
    Start(region, data, key, velocity, tone, order);
    return;
  }
  pending_.region = &region;
  pending_.data = data;
  pending_.tone = tone;
  pending_.order = order;
  pending_.key = static_cast<std::uint8_t>(key);
  pending_.velocity = static_cast<std::uint8_t>(velocity);
  hasPending_ = true;
  pendingReleased_ = false;
  Fade(1);
}

void Voice::Release()
{
  if (hasPending_)
  {
    pendingReleased_ = true;
    return;
  }
  if (!Active() || released_) return;
  released_ = true;
  if (region_->mode == 3) looping_ = false;
  switch (stage_)
  {
    case Stage::Delay:
      // Nothing has sounded yet.
      Stop();
      return;
    case Stage::Attack:
      // From the level it has reached, without a jump: that level as an attenuation.
      attenuation_ = AttenuationOfGain(attackLevel_ >> 15);
      break;
    default:
      break;
  }
  stage_ = Stage::Release;
  modStage_ = Stage::Release;
}

void Voice::Choke(unsigned controlSteps)
{
  hasPending_ = false;
  if (!Active()) return;
  released_ = true;
  if (region_->mode == 3) looping_ = false;
  Fade(controlSteps);
}

void Voice::Fade(unsigned controlSteps)
{
  if (!Active()) return;
  const std::int32_t step = static_cast<std::int32_t>((32768u + (std::max)(controlSteps, 1u) - 1u) / (std::max)(controlSteps, 1u));
  fadeStep_ = fading_ ? (std::max)(fadeStep_, step) : step;
  fading_ = true;
}

void Voice::Stop()
{
  stage_ = Stage::Off;
  hasPending_ = false;
  fading_ = false;
  gain_ = 0;
}

void Voice::AdvanceVolume()
{
  switch (stage_)
  {
    case Stage::Delay:
      if (counterQ8_ > kUnityQ8Steps)
      {
        counterQ8_ -= kUnityQ8Steps;
        return;
      }
      stage_ = Stage::Attack;
      [[fallthrough]];
    case Stage::Attack:
      attackLevel_ = (std::min)(attackLevel_ + attackStep_, kUnityQ30);
      if (attackLevel_ < kUnityQ30) return;
      stage_ = Stage::Hold;
      counterQ8_ = holdStepsQ8_;
      return;
    case Stage::Hold:
      if (counterQ8_ > kUnityQ8Steps)
      {
        counterQ8_ -= kUnityQ8Steps;
        return;
      }
      stage_ = Stage::Decay;
      [[fallthrough]];
    case Stage::Decay:
      attenuation_ += decayStep_;
      if (attenuation_ < sustain_) return;
      attenuation_ = sustain_;
      // A sustain at full attenuation is silence: the note is over.
      stage_ = sustain_ >= kFullScaleQ12 ? Stage::Off : Stage::Sustain;
      return;
    case Stage::Release:
      attenuation_ += releaseStep_;
      if (attenuation_ >= kFullScaleQ12) stage_ = Stage::Off;
      return;
    case Stage::Sustain:
    case Stage::Off:
      return;
  }
}

void Voice::AdvanceModulation()
{
  switch (modStage_)
  {
    case Stage::Delay:
      if (modCounterQ8_ > kUnityQ8Steps)
      {
        modCounterQ8_ -= kUnityQ8Steps;
        return;
      }
      modStage_ = Stage::Attack;
      [[fallthrough]];
    case Stage::Attack:
      modValue_ = (std::min)(modValue_ + modAttackStep_, kUnityQ30);
      if (modValue_ < kUnityQ30) return;
      modStage_ = Stage::Hold;
      modCounterQ8_ = modHoldStepsQ8_;
      return;
    case Stage::Hold:
      if (modCounterQ8_ > kUnityQ8Steps)
      {
        modCounterQ8_ -= kUnityQ8Steps;
        return;
      }
      modStage_ = Stage::Decay;
      [[fallthrough]];
    case Stage::Decay:
      modValue_ = modValue_ > modSustain_ + modDecayStep_ ? modValue_ - modDecayStep_ : modSustain_;
      if (modValue_ == modSustain_) modStage_ = Stage::Sustain;
      return;
    case Stage::Release:
      modValue_ = modValue_ > modReleaseStep_ ? modValue_ - modReleaseStep_ : 0;
      return;
    case Stage::Sustain:
    case Stage::Off:
      return;
  }
}

void Voice::ControlStep()
{
  if (fading_ && fade_ <= 0)
  {
    stage_ = Stage::Off;
    return;
  }
  if (fading_) fade_ = (std::max)(std::int32_t{0}, fade_ - fadeStep_);

  AdvanceVolume();
  if (stage_ == Stage::Off)
  {
    gain_ = 0;
    return;
  }
  AdvanceModulation();

  std::int32_t modLfo = 0;
  if (modLfoDelayQ8_ > kUnityQ8Steps) modLfoDelayQ8_ -= kUnityQ8Steps;
  else
  {
    modLfo = Triangle(modLfoPhase_);
    modLfoPhase_ += modLfoStep_;
  }
  std::int32_t vibLfo = 0;
  if (vibLfoDelayQ8_ > kUnityQ8Steps) vibLfoDelayQ8_ -= kUnityQ8Steps;
  else
  {
    vibLfo = Triangle(vibLfoPhase_);
    vibLfoPhase_ += vibLfoStep_;
  }
  const std::int32_t modEnv = static_cast<std::int32_t>(modValue_ >> 15);

  if (pitchModulated_)
  {
    const std::int32_t cents = baseCentsQ8_ + CentsQ8(modEnv, region_->modEnvToPitch) + CentsQ8(modLfo, region_->modLfoToPitch) + CentsQ8(vibLfo, region_->vibLfoToPitch);
    step_ = PositionStep(cents, rateRatioQ24_);
  }

  if (filtered_ && (filterModulated_ || !filterPrimed_))
  {
    const std::int32_t cutoff = baseCutoffQ8_ + CentsQ8(modEnv, region_->modEnvToFilter) + CentsQ8(modLfo, region_->modLfoToFilter);
    const FilterCoefficients target = FilterFor(cutoff, resonanceCb_);
    if (!filterPrimed_)
    {
      a1_ = target.a1;
      a2_ = target.a2;
      a3_ = target.a3;
      a1Step_ = a2Step_ = a3Step_ = 0;
      filterPrimed_ = true;
    }
    else
    {
      // Glide over the control step: a jump in the coefficients is a click in a resonant filter.
      a1Step_ = (target.a1 - a1_) / static_cast<std::int32_t>(kControlFrames);
      a2Step_ = (target.a2 - a2_) / static_cast<std::int32_t>(kControlFrames);
      a3Step_ = (target.a3 - a3_) / static_cast<std::int32_t>(kControlFrames);
    }
  }

  // The modulation LFO's tremolo: a positive amount makes a positive swing louder.
  const std::int32_t tremolo = -((modLfo * region_->modLfoToVolume) / 8);
  std::int64_t target = 0;
  if (stage_ == Stage::Attack) target = (static_cast<std::int64_t>(attackLevel_) * GainQ15(staticAttenuation_ + tremolo)) >> 15;
  else if (stage_ != Stage::Delay) target = static_cast<std::int64_t>(GainQ15(staticAttenuation_ + attenuation_ + tremolo)) << 15;
  target = (target * fade_) >> 15;
  if (gain_ < 0) gain_ = 0;
  // A shift, not a 64-bit division (a library call here): it rounds down, and a gain that ends a
  // hair under zero is set back to zero at the next step.
  static_assert(kControlFrames == 64, "the glide divides by shifting");
  gainStep_ = static_cast<std::int32_t>((target - gain_) >> 6);
  chunkLeft_ = kControlFrames;
}

void Voice::Render(std::int32_t* accumulator, std::size_t frames)
{
  while (frames > 0 && Active())
  {
    if (chunkLeft_ == 0) ControlStep();
    if (Active())
    {
      const std::size_t rendered = RenderRun(accumulator, (std::min)(frames, chunkLeft_));
      accumulator += rendered;
      frames -= rendered;
      chunkLeft_ -= rendered;
    }
    if (Active()) continue;
    // The sample or its envelope may have ended before the fade. The waiting note owns the
    // next output frame in all three cases, including in the middle of this render block.
    if (!hasPending_)
    {
      Stop();
      return;
    }
    const Pending next = pending_;
    const bool releasedBeforeStart = pendingReleased_;
    Start(*next.region, next.data, next.key, next.velocity, next.tone, next.order);
    if (releasedBeforeStart) Release();
  }
}

std::size_t Voice::RenderRun(std::int32_t* out, std::size_t frames)
{
  const std::size_t requested = frames;
  const std::int16_t* const data = data_;
  std::uint32_t index = index_;
  std::uint32_t fraction = fraction_;
  const std::uint32_t step = step_;
  std::int32_t gain = gain_;
  const std::int32_t gainStep = gainStep_;
  const bool filtered = filtered_;
  std::int32_t a1 = a1_;
  std::int32_t a2 = a2_;
  std::int32_t a3 = a3_;
  std::int32_t ic1 = ic1_;
  std::int32_t ic2 = ic2_;

  // One output frame from frame `a` and the one after it, `b`. Every product is rounded, not cut:
  // inside the filter a cut is a constant error that its feedback turns into an offset (k / g times
  // larger, hundreds of times at a low cutoff). Inlined by force: called once per frame, a call
  // would cost more than the frame (AGENT.md, hot code), and GCC kept it out of line.
  constexpr std::int64_t kHalf30 = std::int64_t{1} << 29;
  const auto frame = [&](std::int32_t a, std::int32_t b) SF2_ALWAYS_INLINE {
    const std::int32_t sample = a + (((b - a) * static_cast<std::int32_t>(fraction >> 1) + (1 << 14)) >> 15);
    std::int32_t value;
    if (filtered)
    {
      // v3 = v0 - ic2; v1 = a1 ic1 + a2 v3; v2 = ic2 + a2 ic1 + a3 v3; each integrator's state
      // moves to 2 v - ic. The lowpass is v2.
      const std::int32_t v3 = sample * 256 - ic2;
      const std::int32_t v1 = static_cast<std::int32_t>((static_cast<std::int64_t>(a1) * ic1 + static_cast<std::int64_t>(a2) * v3 + kHalf30) >> 30);
      const std::int32_t v2 = ic2 + static_cast<std::int32_t>((static_cast<std::int64_t>(a2) * ic1 + static_cast<std::int64_t>(a3) * v3 + kHalf30) >> 30);
      ic1 = Saturate(2 * static_cast<std::int64_t>(v1) - ic1);
      ic2 = Saturate(2 * static_cast<std::int64_t>(v2) - ic2);
      value = static_cast<std::int32_t>((static_cast<std::int64_t>(v2) * gain + (std::int64_t{1} << 37)) >> 38);
      a1 += a1Step_;
      a2 += a2Step_;
      a3 += a3Step_;
    }
    else
    {
      value = static_cast<std::int32_t>((static_cast<std::int64_t>(sample) * gain + kHalf30) >> 30);
    }
    *out++ += value;
    gain += gainStep;
    fraction += step;
    index += fraction >> 16;
    fraction &= 0xFFFFu;
  };

  while (frames > 0)
  {
    // Where the plain run stops: the loop's last frame needs the loop's first beside it.
    const std::uint32_t limit = looping_ ? loopEnd_ - 1 : end_;
    if (index >= limit)
    {
      if (!looping_)
      {
        stage_ = Stage::Off;
        gain = 0;
        break;
      }
      if (index >= loopEnd_)
      {
        const std::uint32_t length = loopEnd_ - loopStart_;
        index = loopStart_ + (index - loopStart_) % length;
        continue;
      }
      frame(data[index], data[loopStart_]);
      --frames;
      continue;
    }
    // How many frames before it gets there: none of them needs a test.
    std::size_t count = frames;
    const std::uint32_t left = limit - index;
    if (left <= (kMaxPositionStep >> 16) * kControlFrames + 1)
    {
      const std::uint32_t distance = (left << 16) - fraction;
      const std::uint32_t needed = (distance + step - 1) / step;
      if (needed < count) count = needed;
    }
    frames -= count;
    for (; count > 0; --count) frame(data[index], data[index + 1]);
  }

  index_ = index;
  fraction_ = fraction;
  gain_ = gain;
  a1_ = a1;
  a2_ = a2;
  a3_ = a3;
  ic1_ = ic1;
  ic2_ = ic2;
  return requested - frames;
}

}  // namespace synth::audio::sf2

#include "Library/Synth/Audio/Fx/SynthFxDsp.h"

#include "Library/Memory/Memory.h"

#include <algorithm>
#include <cmath>
#include <new>

#if defined(ESP_PLATFORM) && defined(__XTENSA__)
#include <xtensa/config/core-isa.h>  // XCHAL_HAVE_CLAMPS (the ESP32-S3 core has it; GCC does not use it)
#endif

namespace synth::fx::dsp {

namespace {

constexpr float kPi = 3.14159265358979f;

// Parameter smoothing: one pole per block (or sub-block), time constant ~20 ms.
constexpr float kSmoothingFrames = 882.0f;
constexpr float SmoothingFor(float frames) { return frames / (frames + kSmoothingFrames); }
constexpr std::size_t kFilterSubBlock = 32;
constexpr std::size_t kChorusSubBlock = 32;
constexpr std::size_t kCompressorSubBlock = 16;
constexpr std::size_t kShelfSubBlock = 16;
constexpr float kBlockSmoothing = SmoothingFor(static_cast<float>(kMaxBlockFrames));

// int16 delay lines hold +-2.0.
constexpr float kLineScale = 16384.0f;
constexpr float kInverseLineScale = 1.0f / kLineScale;

// Drive: pre-gain 1x..8x; the output is scaled so a 0.25 peak keeps its level.
constexpr float kDriveReference = 0.25f;

// DJ filter: positions within 2 of the centre bypass, the next 6 fade the filter in.
constexpr float kFilterDeadZone = 2.0f;
constexpr float kFilterInverseFadeWidth = 1.0f / 6.0f;
constexpr float kFilterLowPassOctaves = 7.9657843f;   // log2(20 kHz / 80 Hz)
constexpr float kFilterHighPassOctaves = 8.6438562f;  // log2(8 kHz / 20 Hz)

// Juno chorus (amy/juno.py): OFF, I, II, I+II rates and depths; the delay swings around 5.8 ms.
constexpr std::array<float, 4> kChorusRates{{0.0f, 0.5f, 0.83f, 1.0f}};
constexpr std::array<float, 4> kChorusDepths{{0.0f, 0.5f, 0.5f, 0.08f}};
constexpr float kChorusCentreFrames = 256.0f;
constexpr float kChorusFadeFrames = 882.0f;
constexpr float kChorusDryCut = 0.2f;   // dry 1.0 -> 0.8 at full chorus
constexpr float kChorusTapGain = 0.35f; // each of the two taps 180 degrees apart
// One pole at 150 Hz on the taps keeps the bass out of the chorus comb (mono sum of both taps).
constexpr float kChorusHighPass = 0.021145f;

// Polyphase FIR (20 taps, Kaiser beta 4, -6 dB at 4.5 kHz) between 44.1 kHz and the bus rate.
// Decimation: the input frame at phase p of a group adds kDecimation[p][k] to output k groups
// ahead. Interpolation: the output frame at phase p is the dot product of kInterpolation[p] and
// the latest bus-rate samples (phase 3 runs after the group's bus tick); each phase sums to 1.
constexpr float kDecimation[4][5] = {
    {-0.016881060f, 0.112487852f, 0.166337492f, -0.011595608f, -0.000565575f},
    {-0.011859470f, 0.055744011f, 0.199243058f, 0.011915921f, -0.004826622f},
    {-0.004826622f, 0.011915921f, 0.199243058f, 0.055744011f, -0.011859470f},
    {-0.000565575f, -0.011595608f, 0.166337492f, 0.112487852f, -0.016881060f}};
constexpr float kInterpolation[4][5] = {
    {-0.019289751f, 0.047622367f, 0.796281383f, 0.222782761f, -0.047396760f},
    {-0.047396760f, 0.222782761f, 0.796281383f, 0.047622367f, -0.019289751f},
    {-0.067582873f, 0.450342121f, 0.665927724f, -0.046422707f, -0.002264265f},
    {-0.002264265f, -0.046422707f, 0.665927724f, 0.450342121f, -0.067582873f}};
constexpr float kResamplingLatencyFrames = 19.0f;  // both FIRs, linear phase

// Tape delay.
constexpr float kDelayLatencyTicks = kResamplingLatencyFrames / static_cast<float>(kBusDecimation);
constexpr float kDelayMaxFeedback = 0.95f;
constexpr float kDelayGlideRate = 0.0008f;  // per tick, towards the new time
constexpr float kDelayMaxGlide = 0.2f;      // read-head speed change limit (+-20 %)
constexpr float kDelayLimit = 1.5f;         // soft clip level in the feedback loop
constexpr float kDelayHighPass = 0.044568563f;  // one pole at 80 Hz, 11.025 kHz
constexpr float kQuietLevel = 1e-5f;

// Reverb.
constexpr float kReverbDiffusion = 0.62f;
constexpr float kReverbInputGain = 0.5f;
constexpr float kReverbOutputGain = 1.4f;  // full send and level: ~0.7 x the input RMS at the default size
// The in-loop damping shortens the decay a little; aim the RT60 slightly long to compensate.
constexpr float kReverbDecayStretch = 1.15f;

// Compressor: 4:1 above a threshold that falls to -30 dBFS, up to +11.25 dB makeup.
constexpr float kDecibelsPerOctave = 6.0205999f;
constexpr float kCompressorRangeDb = 30.0f;
constexpr float kCompressorMakeupDb = 11.25f;
constexpr float kCompressorSlope = 0.75f;  // 1 - 1/ratio
constexpr float kCompressorAttack = 0.0045248791f;   // one pole, 5 ms
constexpr float kCompressorRelease = 0.00018894662f; // one pole, 120 ms

float Normalized127(int value)
{
  return static_cast<float>(std::clamp(value, 0, 127)) * (1.0f / 127.0f);
}

// 1 / frames without a divide for the usual block and sub-block sizes.
float Inverse(std::size_t frames)
{
  switch (frames)
  {
    case kMaxBlockFrames: return 1.0f / static_cast<float>(kMaxBlockFrames);
    case 32: return 1.0f / 32.0f;
    case 16: return 1.0f / 16.0f;
    case 64: return 1.0f / 64.0f;
    default: return frames == 0 ? 0.0f : 1.0f / static_cast<float>(frames);
  }
}

float Smoothing(std::size_t frames)
{
  return frames == kMaxBlockFrames ? kBlockSmoothing : SmoothingFor(static_cast<float>(frames));
}

float Approach(float current, float target, float amount, float snap)
{
  const float next = current + (target - current) * amount;
  return std::fabs(target - next) < snap ? target : next;
}

float StepTowards(float current, float target, float step)
{
  return current < target ? std::min(target, current + step) : std::max(target, current - step);
}

float OnePoleCoefficient(float hz, float rate)
{
  return std::min(1.0f, 1.0f - std::exp(-2.0f * kPi * hz / rate));
}

// Saturating store of a value already in line units. Truncation toward zero, so decaying feedback
// cannot settle into a +-1 LSB limit cycle.
std::int16_t Saturate(float lineUnits)
{
  const std::int32_t value = static_cast<std::int32_t>(lineUnits);
  return static_cast<std::int16_t>(value < -32768 ? -32768 : (value > 32767 ? 32767 : value));
}

// Saturate() stored with one CLAMPS instruction on cores that have it (the ESP32-S3 does; GCC does
// not emit it). Inline assembly keeps GCC from using a zero-overhead loop, so this is only worth it
// where several stores share a loop (the reverb's seven per tick).
void StoreSaturated(std::int16_t* slot, float lineUnits)
{
#if defined(XCHAL_HAVE_CLAMPS) && XCHAL_HAVE_CLAMPS
  std::int32_t clamped;
  __asm__("clamps %0, %1, 15" : "=r"(clamped) : "r"(static_cast<std::int32_t>(lineUnits)));
  *slot = static_cast<std::int16_t>(clamped);
#else
  *slot = Saturate(lineUnits);
#endif
}

std::int16_t ToLine(float value)
{
  return Saturate(value * kLineScale);
}

float Flush(float value)
{
  return std::fabs(value) < 1e-20f ? 0.0f : value;
}

bool AllZero(const float* samples, std::size_t frames)
{
  for (std::size_t index = 0; index < frames; ++index)
  {
    if (samples[index] != 0.0f) return false;
  }
  return true;
}

float ShelfDecibels(int value)
{
  const int clamped = std::clamp(value, 0, 127);
  return clamped >= 64 ? static_cast<float>(clamped - 64) * (12.0f / 63.0f) : static_cast<float>(clamped - 64) * (12.0f / 64.0f);
}

}  // namespace

float MaxDelaySeconds()
{
  return (static_cast<float>(kDelayLongLineFrames) - 8.0f) / kBusRate;
}

float DelaySeconds(int division, float bpm)
{
  if (!(bpm > 0.0f)) bpm = 120.0f;
  bpm = std::clamp(bpm, 20.0f, 400.0f);
  float seconds = kDelaySixteenths[static_cast<std::size_t>(std::clamp(division, 0, static_cast<int>(kDelaySixteenths.size()) - 1))] * 15.0f / bpm;
  const float longest = MaxDelaySeconds();
  while (seconds > longest) seconds *= 0.5f;
  return seconds;
}

// RBJ audio-EQ-cookbook shelves, 44.1 kHz.
void Biquad::LowShelf(float cosine, float sine, float gainDb)
{
  const float a = std::exp2(gainDb * (3.3219281f / 40.0f));  // 10^(dB / 40)
  const float twoRootAAlpha = 2.0f * std::sqrt(a) * sine * 0.70710678f;  // shelf slope 1
  const float inverseA0 = 1.0f / ((a + 1.0f) + (a - 1.0f) * cosine + twoRootAAlpha);
  b0 = a * ((a + 1.0f) - (a - 1.0f) * cosine + twoRootAAlpha) * inverseA0;
  b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cosine) * inverseA0;
  b2 = a * ((a + 1.0f) - (a - 1.0f) * cosine - twoRootAAlpha) * inverseA0;
  a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cosine) * inverseA0;
  a2 = ((a + 1.0f) + (a - 1.0f) * cosine - twoRootAAlpha) * inverseA0;
}

void Biquad::HighShelf(float cosine, float sine, float gainDb)
{
  const float a = std::exp2(gainDb * (3.3219281f / 40.0f));
  const float twoRootAAlpha = 2.0f * std::sqrt(a) * sine * 0.70710678f;
  const float inverseA0 = 1.0f / ((a + 1.0f) - (a - 1.0f) * cosine + twoRootAAlpha);
  b0 = a * ((a + 1.0f) + (a - 1.0f) * cosine + twoRootAAlpha) * inverseA0;
  b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cosine) * inverseA0;
  b2 = a * ((a + 1.0f) + (a - 1.0f) * cosine - twoRootAAlpha) * inverseA0;
  a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cosine) * inverseA0;
  a2 = ((a + 1.0f) - (a - 1.0f) * cosine - twoRootAAlpha) * inverseA0;
}

bool IsNeutral(const TrackInsertSettings& settings)
{
  return settings.drive <= 0 && settings.crush <= 0 && settings.chorus <= 0 &&
         std::abs(settings.filter - kFilterCenter) <= static_cast<int>(kFilterDeadZone);
}

// ---------------------------------------------------------------------------------------------
// Track inserts

bool TrackInserts::AllocateChorus()
{
  if (!chorusLine_)
  {
    chorusLine_.reset(new (std::nothrow) std::int16_t[kChorusLineFrames]());
  }
  return chorusLine_ != nullptr;
}

void TrackInserts::Reset()
{
  std::unique_ptr<std::int16_t[]> line = std::move(chorusLine_);
  *this = TrackInserts{};
  chorusLine_ = std::move(line);
}

bool TrackInserts::Settled() const
{
  return drive_ == 0.0f && crush_ == 0.0f && chorusMix_ == 0.0f &&
         std::fabs(filterPosition_ - static_cast<float>(kFilterCenter)) <= kFilterDeadZone;
}

SYNTH_FX_HOT void TrackInserts::Process(float* io, std::size_t frames, const TrackInsertSettings& settings)
{
  frames = std::min(frames, kMaxBlockFrames);
  if (frames == 0) return;

  const bool silent = AllZero(io, frames);
  if (silent && quietBlocks_ >= 2)
  {
    // Nothing to process and nothing left ringing: parameter moves cannot click, so jump.
    SnapToTargets(settings);
    return;
  }

  ProcessDrive(io, frames, Normalized127(settings.drive));
  ProcessCrush(io, frames, Normalized127(settings.crush));
  ProcessFilter(io, frames, static_cast<float>(std::clamp(settings.filter, 0, 127)), static_cast<float>(std::clamp(settings.resonance, 0, 127)));
  ProcessChorus(io, frames, std::clamp(settings.chorus, 0, 3));

  if (!silent)
  {
    quietBlocks_ = 0;
    return;
  }
  float peak = 0.0f;
  for (std::size_t index = 0; index < frames; ++index) peak = std::max(peak, std::fabs(io[index]));
  quietBlocks_ = peak < 1e-6f ? std::min(quietBlocks_ + 1, 1000) : 0;
}

void TrackInserts::SnapToTargets(const TrackInsertSettings& settings)
{
  drive_ = Normalized127(settings.drive);
  crush_ = Normalized127(settings.crush);
  crushPhase_ = 1.0f;
  crushHeld_ = 0.0f;
  filterPosition_ = static_cast<float>(std::clamp(settings.filter, 0, 127));
  filterResonance_ = static_cast<float>(std::clamp(settings.resonance, 0, 127));
  filterActive_ = false;  // Recomputed (from a cleared state) by the next processed block.
  filterMix_ = 0.0f;
  filterIc1_ = filterIc2_ = 0.0f;
  const int mode = std::clamp(settings.chorus, 0, 3);
  if (mode > 0 && chorusLine_)
  {
    if (chorusMix_ == 0.0f) ClearChorus();
    chorusMix_ = 1.0f;
    chorusRate_ = kChorusRates[static_cast<std::size_t>(mode)];
    chorusDepth_ = kChorusDepths[static_cast<std::size_t>(mode)];
  }
  else
  {
    chorusMix_ = 0.0f;
  }
}

TrackInserts::DriveStage TrackInserts::DriveFor(float amount)
{
  const float gain = std::exp2(3.0f * amount);
  return DriveStage{gain, kDriveReference / SoftClip(gain * kDriveReference), std::min(1.0f, 4.0f * amount)};
}

SYNTH_FX_HOT void TrackInserts::ProcessDrive(float* io, std::size_t frames, float target)
{
  const float start = drive_;
  drive_ = Approach(drive_, target, Smoothing(frames), 0.0005f);
  if (start == 0.0f && drive_ == 0.0f) return;

  // Stage parameters are cached: a steady drive costs no exp2 or divide per block.
  const DriveStage from = start == driveCachedAmount_ ? driveCached_ : DriveFor(start);
  if (drive_ != driveCachedAmount_)
  {
    driveCached_ = DriveFor(drive_);
    driveCachedAmount_ = drive_;
  }
  const DriveStage to = driveCached_;
  if (start == drive_)
  {
    const float gain = to.gain, makeup = to.makeup, wet = to.wet;
    if (wet == 1.0f)
    {
      for (std::size_t index = 0; index < frames; ++index) io[index] = makeup * SoftClip(gain * io[index]);
      return;
    }
    for (std::size_t index = 0; index < frames; ++index)
    {
      const float x = io[index];
      io[index] = x + wet * (makeup * SoftClip(gain * x) - x);
    }
    return;
  }
  const float inverse = Inverse(frames);
  const float gainStep = (to.gain - from.gain) * inverse;
  const float makeupStep = (to.makeup - from.makeup) * inverse;
  const float wetStep = (to.wet - from.wet) * inverse;
  float gain = from.gain, makeup = from.makeup, wet = from.wet;
  for (std::size_t index = 0; index < frames; ++index)
  {
    const float x = io[index];
    io[index] = x + wet * (makeup * SoftClip(gain * x) - x);
    gain += gainStep;
    makeup += makeupStep;
    wet += wetStep;
  }
}

SYNTH_FX_HOT void TrackInserts::ProcessCrush(float* io, std::size_t frames, float target)
{
  const float start = crush_;
  crush_ = Approach(crush_, target, Smoothing(frames), 0.0005f);
  if (start == 0.0f && crush_ == 0.0f)
  {
    crushPhase_ = 1.0f;
    crushHeld_ = 0.0f;
    return;
  }

  // Sample-and-hold down to 1/16 of the rate and rounding down to 4 bits, faded in from bypass.
  if (crush_ != crushCachedAmount_)
  {
    crushCachedAmount_ = crush_;
    crushHoldStep_ = 1.0f / (1.0f + 15.0f * crush_ * crush_);
    crushLevels_ = std::exp2(15.0f - 12.0f * crush_);
    crushInverseLevels_ = 1.0f / crushLevels_;
  }
  const float holdStep = crushHoldStep_, levels = crushLevels_, inverseLevels = crushInverseLevels_;
  const float wetFrom = std::min(1.0f, 8.0f * start);
  const float wetStep = (std::min(1.0f, 8.0f * crush_) - wetFrom) * Inverse(frames);
  float wet = wetFrom;
  float phase = crushPhase_;
  float held = crushHeld_;
  for (std::size_t index = 0; index < frames; ++index)
  {
    const float x = io[index];
    phase += holdStep;
    if (phase >= 1.0f)
    {
      phase -= 1.0f;
      const float scaled = std::clamp(x, -4.0f, 4.0f) * levels;
      held = static_cast<float>(static_cast<int>(scaled + (scaled >= 0.0f ? 0.5f : -0.5f))) * inverseLevels;
    }
    io[index] = x + wet * (held - x);
    wet += wetStep;
  }
  crushPhase_ = phase;
  crushHeld_ = held;
}

void TrackInserts::UpdateFilterCoefficients()
{
  const float offset = filterPosition_ - static_cast<float>(kFilterCenter);
  float hz = 0.0f;
  if (offset < 0.0f)
  {
    const float sweep = std::clamp((-offset - kFilterDeadZone) * (1.0f / (64.0f - kFilterDeadZone)), 0.0f, 1.0f);
    hz = 20000.0f * std::exp2(-kFilterLowPassOctaves * sweep);
    filterHighPass_ = false;
  }
  else
  {
    const float sweep = std::clamp((offset - kFilterDeadZone) * (1.0f / (63.0f - kFilterDeadZone)), 0.0f, 1.0f);
    hz = 20.0f * std::exp2(kFilterHighPassOctaves * sweep);
    filterHighPass_ = true;
  }
  hz = std::min(hz, 0.45f * kSampleRate);
  // Topology-preserving-transform state-variable filter (Zavalishin), Q 0.5..8.
  filterG_ = std::tan(kPi * hz / kSampleRate);
  filterK_ = 2.0f * std::exp2(-4.0f * filterResonance_ * (1.0f / 127.0f));
  filterA1_ = 1.0f / (1.0f + filterG_ * (filterG_ + filterK_));
  filterA2_ = filterG_ * filterA1_;
  filterA3_ = filterG_ * filterA2_;
}

SYNTH_FX_HOT void TrackInserts::ProcessFilter(float* io, std::size_t frames, float targetPosition, float targetResonance)
{
  if (!filterActive_ && targetPosition == filterPosition_ && targetResonance == filterResonance_ &&
      std::fabs(filterPosition_ - static_cast<float>(kFilterCenter)) <= kFilterDeadZone)
  {
    return;  // Bypassed and staying so.
  }
  constexpr float kAmount = SmoothingFor(static_cast<float>(kFilterSubBlock));
  for (std::size_t offset = 0; offset < frames; offset += kFilterSubBlock)
  {
    const std::size_t count = std::min(kFilterSubBlock, frames - offset);
    bool moved = false;
    if (filterPosition_ != targetPosition || filterResonance_ != targetResonance)
    {
      filterPosition_ = Approach(filterPosition_, targetPosition, kAmount, 0.01f);
      filterResonance_ = Approach(filterResonance_, targetResonance, kAmount, 0.01f);
      moved = true;
    }
    const float distance = std::fabs(filterPosition_ - static_cast<float>(kFilterCenter));
    if (distance <= kFilterDeadZone)
    {
      if (filterActive_)
      {
        filterActive_ = false;
        filterMix_ = 0.0f;
        filterIc1_ = filterIc2_ = 0.0f;
      }
      continue;
    }
    if (moved || !filterActive_)
    {
      UpdateFilterCoefficients();
      filterActive_ = true;
    }

    const float mixTarget = std::min(1.0f, (distance - kFilterDeadZone) * kFilterInverseFadeWidth);
    const float mixStep = (mixTarget - filterMix_) * Inverse(count);
    float mix = filterMix_;
    const float a1 = filterA1_, a2 = filterA2_, a3 = filterA3_, k = filterK_;
    float ic1 = filterIc1_, ic2 = filterIc2_;
    float* samples = io + offset;
    if (filterHighPass_)
    {
      for (std::size_t index = 0; index < count; ++index)
      {
        const float x = samples[index];
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        mix += mixStep;
        samples[index] = x + mix * ((x - k * v1 - v2) - x);
      }
    }
    else
    {
      for (std::size_t index = 0; index < count; ++index)
      {
        const float x = samples[index];
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        mix += mixStep;
        samples[index] = x + mix * (v2 - x);
      }
    }
    filterMix_ = mixTarget;
    filterIc1_ = Flush(ic1);
    filterIc2_ = Flush(ic2);
  }
}

void TrackInserts::ClearChorus()
{
  std::fill_n(chorusLine_.get(), kChorusLineFrames, std::int16_t{0});
  chorusWrite_ = 0;
  chorusPhase_ = 0.0f;
  chorusLow_ = 0.0f;
}

SYNTH_FX_HOT void TrackInserts::ProcessChorus(float* io, std::size_t frames, int mode)
{
  if (!chorusLine_)
  {
    chorusMix_ = 0.0f;
    return;
  }
  const float target = mode > 0 ? 1.0f : 0.0f;
  if (chorusMix_ == 0.0f && target == 0.0f) return;
  float depthTarget = chorusDepth_;
  if (mode > 0)
  {
    chorusRate_ = kChorusRates[static_cast<std::size_t>(mode)];
    depthTarget = kChorusDepths[static_cast<std::size_t>(mode)];
  }
  if (chorusMix_ == 0.0f)
  {
    // Fresh start: whatever the line held from an earlier use is stale.
    ClearChorus();
    chorusDepth_ = depthTarget;
  }

  const float inverseFrames = Inverse(frames);
  const float mixFrom = chorusMix_;
  const float mixTo = StepTowards(chorusMix_, target, static_cast<float>(frames) * (1.0f / kChorusFadeFrames));
  const float depthFrom = chorusDepth_ * kChorusCentreFrames;
  const float depthTo = Approach(chorusDepth_, depthTarget, Smoothing(frames), 0.0001f) * kChorusCentreFrames;
  const float mixStep = (mixTo - mixFrom) * inverseFrames;
  // Dry and wet gains ramp with the fade; the wet gain also takes the taps out of line units.
  float dry = 1.0f - kChorusDryCut * mixFrom;
  float wet = kChorusTapGain * kInverseLineScale * mixFrom;
  const float dryStep = -kChorusDryCut * mixStep;
  const float wetStep = kChorusTapGain * kInverseLineScale * mixStep;
  const float highPass = kChorusHighPass;
  const float phaseStep = chorusRate_ * (1.0f / kSampleRate);
  constexpr std::uint32_t kMask = kChorusLineFrames - 1;
  constexpr float kFixedOne = 65536.0f;  // read heads in 16.16 fixed point
  std::int16_t* line = chorusLine_.get();
  std::uint32_t write = static_cast<std::uint32_t>(chorusWrite_);
  float phase = chorusPhase_;
  float low = chorusLow_ * kLineScale;  // in line units inside the loop
  const auto sweepAt = [](float lfoPhase, float depth) { return depth * (4.0f * std::fabs(lfoPhase - 0.5f) - 1.0f); };
  // The triangle LFO moves the taps by well under a sample per frame, so each sub-block moves the
  // two read heads linearly between the delays at its ends, stepping them in fixed point.
  for (std::size_t offset = 0; offset < frames; offset += kChorusSubBlock)
  {
    const std::size_t count = std::min(kChorusSubBlock, frames - offset);
    const float inverseCount = Inverse(count);
    float phaseEnd = phase + phaseStep * static_cast<float>(count);
    if (phaseEnd >= 1.0f) phaseEnd -= 1.0f;
    const float depthStart = depthFrom + (depthTo - depthFrom) * (static_cast<float>(offset) * inverseFrames);
    const float depthEnd = depthFrom + (depthTo - depthFrom) * (static_cast<float>(offset + count) * inverseFrames);
    const float sweepStart = sweepAt(phase, depthStart);
    const float sweepEnd = sweepAt(phaseEnd, depthEnd);
    phase = phaseEnd;
    const auto head = [&](float delay) {
      float position = static_cast<float>(write) - delay;
      if (position < 0.0f) position += static_cast<float>(kChorusLineFrames);
      return static_cast<std::uint32_t>(static_cast<std::int32_t>(position * kFixedOne));
    };
    // Each frame the write head advances by one and the delay by (end - start) / count.
    std::uint32_t headA = head(kChorusCentreFrames + sweepStart);
    std::uint32_t headB = head(kChorusCentreFrames - sweepStart);
    const float slope = (sweepEnd - sweepStart) * inverseCount;
    const std::uint32_t stepA = static_cast<std::uint32_t>(static_cast<std::int32_t>((1.0f - slope) * kFixedOne));
    const std::uint32_t stepB = static_cast<std::uint32_t>(static_cast<std::int32_t>((1.0f + slope) * kFixedOne));
    float* samples = io + offset;
    for (std::size_t index = 0; index < count; ++index)
    {
      const float x = samples[index];
      const std::uint32_t a0 = (headA >> 16) & kMask;
      const std::uint32_t b0 = (headB >> 16) & kMask;
      const float a = line[a0];
      const float b = line[b0];
      const float taps = a + static_cast<float>(headA & 0xFFFFu) * (1.0f / kFixedOne) * (static_cast<float>(line[(a0 + 1) & kMask]) - a) +
                         b + static_cast<float>(headB & 0xFFFFu) * (1.0f / kFixedOne) * (static_cast<float>(line[(b0 + 1) & kMask]) - b);
      line[write] = ToLine(x);
      write = (write + 1) & kMask;
      headA += stepA;
      headB += stepB;
      low += highPass * (taps - low);
      samples[index] = x * dry + wet * (taps - low);
      dry += dryStep;
      wet += wetStep;
    }
  }
  chorusDepth_ = depthTo * (1.0f / kChorusCentreFrames);
  chorusWrite_ = write;
  chorusPhase_ = phase;
  chorusLow_ = Flush(low * kInverseLineScale);
  chorusMix_ = mixTo;
}

// ---------------------------------------------------------------------------------------------
// Send buses

namespace {

// Keeps the compiler from hoisting a coefficient table into registers it then has to spill: the
// Xtensa FPU has 16 registers, and reloading a coefficient is a single instruction.
#if defined(__GNUC__)
#define SYNTH_FX_RELOAD(pointer) __asm__("" : "+r"(pointer))
#else
#define SYNTH_FX_RELOAD(pointer) static_cast<void>(pointer)
#endif

// Polyphase decimation by kBusDecimation of `frames` inputs into `out` (one sample per completed
// group), transposed form: the input at phase p adds coefficients[p][k] to the output k groups
// ahead. Returns the number of outputs.
SYNTH_FX_NOINLINE SYNTH_FX_HOT std::size_t Decimate(const float* in, std::size_t frames, int phase, float* accumulators, const float* coefficients, float* out)
{
  float a0 = accumulators[0], a1 = accumulators[1], a2 = accumulators[2], a3 = accumulators[3], a4 = accumulators[4];
  std::size_t count = 0;
  std::size_t index = 0;
  const auto single = [&](float x, int at) {
    const float* row = coefficients + at * 5;
    a0 += row[0] * x;
    a1 += row[1] * x;
    a2 += row[2] * x;
    a3 += row[3] * x;
    a4 += row[4] * x;
  };
  const auto emit = [&]() {
    out[count++] = a0;
    a0 = a1;
    a1 = a2;
    a2 = a3;
    a3 = a4;
    a4 = 0.0f;
  };
  for (; index < frames && phase != 0; ++index)
  {
    single(in[index], phase);
    if (++phase == kBusDecimation)
    {
      phase = 0;
      emit();
    }
  }
  const float* c = coefficients;
  for (; index + kBusDecimation <= frames; index += kBusDecimation)
  {
    SYNTH_FX_RELOAD(c);
    const float x0 = in[index], x1 = in[index + 1], x2 = in[index + 2], x3 = in[index + 3];
    a0 += c[0] * x0 + c[5] * x1 + c[10] * x2 + c[15] * x3;
    a1 += c[1] * x0 + c[6] * x1 + c[11] * x2 + c[16] * x3;
    a2 += c[2] * x0 + c[7] * x1 + c[12] * x2 + c[17] * x3;
    a3 += c[3] * x0 + c[8] * x1 + c[13] * x2 + c[18] * x3;
    a4 += c[4] * x0 + c[9] * x1 + c[14] * x2 + c[19] * x3;
    emit();
  }
  for (; index < frames; ++index)
  {
    single(in[index], phase++);
  }
  accumulators[0] = a0;
  accumulators[1] = a1;
  accumulators[2] = a2;
  accumulators[3] = a3;
  accumulators[4] = a4;
  return count;
}

// Polyphase interpolation by kBusDecimation of the bus-rate samples `in` into `mix` (added),
// mirroring Decimate's phases: the frame that completes a group first takes the group's new bus
// sample into the history (newest first).
SYNTH_FX_NOINLINE SYNTH_FX_HOT void Interpolate(const float* in, float* mix, std::size_t frames, int phase, float* history, const float* coefficients)
{
  float u0 = history[0], u1 = history[1], u2 = history[2], u3 = history[3], u4 = history[4];
  std::size_t next = 0;
  std::size_t index = 0;
  const auto push = [&]() {
    u4 = u3;
    u3 = u2;
    u2 = u1;
    u1 = u0;
    u0 = in[next++];
  };
  const auto single = [&](std::size_t at, int rowIndex) {
    const float* row = coefficients + rowIndex * 5;
    mix[at] += row[0] * u0 + row[1] * u1 + row[2] * u2 + row[3] * u3 + row[4] * u4;
  };
  for (; index < frames && phase != 0; ++index)
  {
    if (phase == kBusDecimation - 1) push();
    single(index, phase);
    phase = phase == kBusDecimation - 1 ? 0 : phase + 1;
  }
  const float* c = coefficients;
  for (; index + kBusDecimation <= frames; index += kBusDecimation)
  {
    SYNTH_FX_RELOAD(c);
    mix[index] += c[0] * u0 + c[1] * u1 + c[2] * u2 + c[3] * u3 + c[4] * u4;
    mix[index + 1] += c[5] * u0 + c[6] * u1 + c[7] * u2 + c[8] * u3 + c[9] * u4;
    mix[index + 2] += c[10] * u0 + c[11] * u1 + c[12] * u2 + c[13] * u3 + c[14] * u4;
    push();
    mix[index + 3] += c[15] * u0 + c[16] * u1 + c[17] * u2 + c[18] * u3 + c[19] * u4;
  }
  for (; index < frames; ++index)
  {
    single(index, phase++);
  }
  history[0] = u0;
  history[1] = u1;
  history[2] = u2;
  history[3] = u3;
  history[4] = u4;
}

}  // namespace

SendBuses::SendBuses()
{
  for (int phase = 0; phase < kBusDecimation; ++phase)
  {
    for (std::size_t tap = 0; tap < kTaps; ++tap)
    {
      decimation_[static_cast<std::size_t>(phase) * kTaps + tap] = kDecimation[phase][tap];
      interpolation_[static_cast<std::size_t>(phase) * kTaps + tap] = kInterpolation[phase][tap];
    }
  }
}

bool SendBuses::AllocateDelay(bool longLine)
{
  const std::size_t chunks = delayFrames_ / kDelayChunkFrames;
  const std::size_t wanted = (longLine ? kDelayLongLineFrames : kDelayLineFrames) / kDelayChunkFrames;
  if (chunks >= wanted) return true;
  std::size_t got = chunks;
  for (; got < wanted; ++got)
  {
    // PSRAM: written and read in order, a sample at a time (Library/Memory/Memory.h).
    delayChunks_[got].reset(new (memory::slow) std::int16_t[kDelayChunkFrames]());
    if (!delayChunks_[got]) break;
  }
  if (got == wanted)
  {
    // The long line replaces the short one: the ticks fall elsewhere, so the echoes start again.
    for (std::size_t chunk = 0; chunk < chunks; ++chunk) std::fill_n(delayChunks_[chunk].get(), kDelayChunkFrames, std::int16_t{0});
    delayFrames_ = static_cast<std::uint32_t>(wanted * kDelayChunkFrames);
    delayWrite_ = 0;
    delayQuietFrames_ = DelayIdleFrames();  // nothing in it
    return true;
  }
  // Not all of it: what there was stays, or else the short line if that fits.
  for (std::size_t chunk = chunks; chunk < got; ++chunk) delayChunks_[chunk].reset();
  if (chunks == 0 && longLine) return AllocateDelay(false);
  return delayFrames_ != 0;
}

bool SendBuses::AllocateReverb()
{
  if (!reverbMemory_)
  {
    // PSRAM, like the delay: seven rings, each read and written in order.
    reverbMemory_.reset(new (memory::slow) std::int16_t[kReverbMemoryFrames]());
  }
  return reverbMemory_ != nullptr;
}

void SendBuses::RestartDelay()
{
  // After a second of silence the line holds only zeros; restart the filters and the head.
  delayDecimator_.fill(0.0f);
  delayTicks_ = delayTargetTicks_;
  delayTone_ = delayHighPassLow_ = 0.0f;
  delayQuietFrames_ = 0;
}

void SendBuses::RestartReverb()
{
  // After a second of silence the lines hold only zeros; restart the filters.
  reverbDecimator_.fill(0.0f);
  reverbLowCutState_ = 0.0f;
  reverbDamping_.fill(0.0f);
  reverbQuietFrames_ = 0;
}

void SendBuses::UpdateDelay(const SendBusSettings& settings)
{
  // A whole number of ticks (within 45 us of the synced time): at rest the read head then sits on
  // a sample and the line interpolation does not dull the repeats.
  // A time the line cannot hold (no memory for the long one) halves until it fits, still on the beat.
  float seconds = settings.delaySeconds;
  const float capacity = static_cast<float>(delayFrames_ != 0 ? delayFrames_ : kDelayLineFrames) - 8.0f;
  while (seconds * kBusRate > capacity) seconds *= 0.5f;
  delayTargetTicks_ = std::floor(std::clamp(seconds * kBusRate - kDelayLatencyTicks, 1.0f, capacity + 4.0f) + 0.5f);
  delayFeedback_ = kDelayMaxFeedback * Normalized127(settings.delayFeedback);
  const float toneHz = 500.0f * std::exp2(4.5f * Normalized127(settings.delayTone));  // 500 Hz .. open
  if (toneHz != delayToneHz_)
  {
    delayToneHz_ = toneHz;
    delayToneCoefficient_ = OnePoleCoefficient(toneHz, kBusRate);
  }
}

void SendBuses::UpdateReverb(const SendBusSettings& settings)
{
  const int size = std::clamp(settings.reverbSize, 0, 127);
  const int damp = std::clamp(settings.reverbDamp, 0, 127);
  const int lowCut = std::clamp(settings.reverbLowCut, 0, 127);
  bool changed = false;
  if (size != reverbSize_)
  {
    reverbSize_ = size;
    const float rt60 = kReverbDecayStretch * 0.3f * std::exp2(4.3219281f * Normalized127(size));  // ~0.3 s .. 6 s
    for (std::size_t index = 0; index < reverbDecay_.size(); ++index)
    {
      // -60 dB after rt60: gain per pass = 10^(-3 L / (fs rt60)).
      reverbDecay_[index] = std::exp2(-9.9657843f * static_cast<float>(kReverbLineFrames[index]) / (kBusRate * rt60));
    }
    changed = true;
  }
  if (damp != reverbDamp_)
  {
    reverbDamp_ = damp;
    reverbDampCoefficient_ = OnePoleCoefficient(5000.0f * std::exp2(-2.6438562f * Normalized127(damp)), kBusRate);  // 5 kHz .. 800 Hz
    changed = true;
  }
  if (changed)
  {
    reverbHold_ = 1.0f - reverbDampCoefficient_;
    for (std::size_t index = 0; index < reverbFeed_.size(); ++index) reverbFeed_[index] = reverbDampCoefficient_ * reverbDecay_[index];
  }
  if (lowCut != reverbLowCut_)
  {
    reverbLowCut_ = lowCut;
    reverbLowCutCoefficient_ = OnePoleCoefficient(20.0f * std::exp2(4.9068906f * Normalized127(lowCut)), kBusRate);  // 20 .. 600 Hz
  }
}

// One delay sample at the bus rate while the read head glides (fractional delay). The line and
// the loop filters work in line units (int16 steps), so reading and writing need no scaling.
SYNTH_FX_INLINE float SendBuses::DelayTick(float input)
{
  const float error = delayTargetTicks_ - delayTicks_;
  delayTicks_ = std::fabs(error) < 0.001f ? delayTargetTicks_ : delayTicks_ + std::clamp(error * kDelayGlideRate, -kDelayMaxGlide, kDelayMaxGlide);
  const std::int32_t whole = static_cast<std::int32_t>(delayTicks_);  // delayTicks_ >= 1: truncation is floor
  const std::uint32_t distance = static_cast<std::uint32_t>(whole);
  const float fraction = delayTicks_ - static_cast<float>(whole);
  // Between the samples `distance` and `distance + 1` ticks ago.
  const std::int16_t newer = DelaySample(delayWrite_ - distance);
  const std::int16_t older = DelaySample(delayWrite_ - distance - 1);
  delayActivity_ |= newer | older;
  const float head = static_cast<float>(newer) + fraction * static_cast<float>(older - newer);
  delayTone_ += delayToneCoefficient_ * (head - delayTone_);
  delayHighPassLow_ += kDelayHighPass * (delayTone_ - delayHighPassLow_);
  const float echo = delayTone_ - delayHighPassLow_;
  constexpr float kToLimit = 1.0f / (kDelayLimit * kLineScale);
  DelaySample(delayWrite_) = Saturate(SoftClip((input * kLineScale + delayFeedback_ * echo) * kToLimit) * (kDelayLimit * kLineScale));
  ++delayWrite_;
  return echo * kInverseLineScale;
}

// The same at rest, the head a whole number of ticks behind: one read, no interpolation.
SYNTH_FX_INLINE float SendBuses::DelayTickAt(float input, std::uint32_t distance)
{
  const std::int16_t sample = DelaySample(delayWrite_ - distance);
  delayActivity_ |= sample;
  delayTone_ += delayToneCoefficient_ * (static_cast<float>(sample) - delayTone_);
  delayHighPassLow_ += kDelayHighPass * (delayTone_ - delayHighPassLow_);
  const float echo = delayTone_ - delayHighPassLow_;
  constexpr float kToLimit = 1.0f / (kDelayLimit * kLineScale);
  DelaySample(delayWrite_) = Saturate(SoftClip(input * (kLineScale * kToLimit) + echo * (delayFeedback_ * kToLimit)) * (kDelayLimit * kLineScale));
  ++delayWrite_;
  return echo * kInverseLineScale;
}

// One reverb sample at the bus rate, in line units like DelayTick. Every delay is read and
// written relative to one counter in the interleaved rings.
SYNTH_FX_INLINE float SendBuses::ReverbTick(float input)
{
  const std::uint32_t now = reverbCounter_++;
  std::int16_t* lines = reverbMemory_.get();
  std::int16_t* diffusers = lines + 4 * kReverbLineRing;
  constexpr std::uint32_t kLineMask = kReverbLineRing - 1;
  constexpr std::uint32_t kDiffuserMask = kReverbDiffuserRing - 1;
  reverbLowCutState_ += reverbLowCutCoefficient_ * (input - reverbLowCutState_);
  float x = (input - reverbLowCutState_) * (kReverbInputGain * kLineScale);
  std::int16_t* diffuserSlot = diffusers + (now & kDiffuserMask) * 3;
  const auto diffuse = [&](std::uint32_t stage, std::uint32_t length) {
    const float delayed = diffusers[((now - length) & kDiffuserMask) * 3 + stage];
    const float w = x + kReverbDiffusion * delayed;
    StoreSaturated(diffuserSlot + stage, w);
    x = delayed - kReverbDiffusion * w;
  };
  diffuse(0, kReverbDiffuserFrames[0]);
  diffuse(1, kReverbDiffuserFrames[1]);
  diffuse(2, kReverbDiffuserFrames[2]);

  const std::int16_t raw0 = lines[((now - kReverbLineFrames[0]) & kLineMask) * 4 + 0];
  const std::int16_t raw1 = lines[((now - kReverbLineFrames[1]) & kLineMask) * 4 + 1];
  const std::int16_t raw2 = lines[((now - kReverbLineFrames[2]) & kLineMask) * 4 + 2];
  const std::int16_t raw3 = lines[((now - kReverbLineFrames[3]) & kLineMask) * 4 + 3];
  // The 10 ms diffusers drain long before a second of silence, so the lines tell when the tail is gone.
  reverbActivity_ |= raw0 | raw1 | raw2 | raw3;
  const float tap0 = raw0, tap1 = raw1, tap2 = raw2, tap3 = raw3;
  const float hold = reverbHold_;
  const float d0 = hold * reverbDamping_[0] + reverbFeed_[0] * tap0;
  const float d1 = hold * reverbDamping_[1] + reverbFeed_[1] * tap1;
  const float d2 = hold * reverbDamping_[2] + reverbFeed_[2] * tap2;
  const float d3 = hold * reverbDamping_[3] + reverbFeed_[3] * tap3;
  reverbDamping_ = {{d0, d1, d2, d3}};
  // Householder feedback matrix I - (2/N) 1 1^T.
  const float reflection = 0.5f * (d0 + d1 + d2 + d3) - x;
  std::int16_t* lineSlot = lines + (now & kLineMask) * 4;
  StoreSaturated(lineSlot + 0, d0 - reflection);
  StoreSaturated(lineSlot + 1, d1 - reflection);
  StoreSaturated(lineSlot + 2, d2 - reflection);
  StoreSaturated(lineSlot + 3, d3 - reflection);
  return (tap0 - tap1 + tap2 - tap3) * (kReverbOutputGain * kInverseLineScale);
}

SYNTH_FX_NOINLINE SYNTH_FX_HOT void SendBuses::RunDelay(const float* inputs, float* returns, std::size_t ticks, float levelTarget)
{
  float level = delayLevel_;
  const float step = (levelTarget - level) * Inverse(ticks);
  float peak = 0.0f;
  delayActivity_ = 0;
  std::size_t tick = 0;
  for (; tick < ticks && delayTicks_ != delayTargetTicks_; ++tick)
  {
    const float delayed = DelayTick(inputs[tick]) * level;
    level += step;
    peak = std::max(peak, std::fabs(delayed));
    returns[tick] = delayed;
  }
  const std::uint32_t distance = static_cast<std::uint32_t>(static_cast<std::int32_t>(delayTicks_));
  for (; tick < ticks; ++tick)
  {
    const float delayed = DelayTickAt(inputs[tick], distance) * level;
    level += step;
    peak = std::max(peak, std::fabs(delayed));
    returns[tick] = delayed;
  }
  delayPeak_ = peak;
  delayLevel_ = levelTarget;
  delayTone_ = Flush(delayTone_);
  delayHighPassLow_ = Flush(delayHighPassLow_);
}

SYNTH_FX_NOINLINE SYNTH_FX_HOT void SendBuses::RunReverb(const float* inputs, float* returns, std::size_t ticks, float levelTarget)
{
  float level = reverbLevel_;
  const float step = (levelTarget - level) * Inverse(ticks);
  float peak = 0.0f;
  reverbActivity_ = 0;
  for (std::size_t tick = 0; tick < ticks; ++tick)
  {
    const float reverberated = ReverbTick(inputs[tick]) * level;
    level += step;
    peak = std::max(peak, std::fabs(reverberated));
    returns[tick] += reverberated;
  }
  reverbPeak_ = peak;
  reverbLevel_ = levelTarget;
  reverbLowCutState_ = Flush(reverbLowCutState_);
  for (float& state : reverbDamping_) state = Flush(state);
}

SYNTH_FX_HOT void SendBuses::Process(const float* delaySend, const float* reverbSend, float* mix, std::size_t frames, const SendBusSettings& settings)
{
  delayPeak_ = reverbPeak_ = 0.0f;
  frames = std::min(frames, kMaxBlockFrames);
  if (frames == 0) return;
  const float delayLevelTarget = KnobGain(settings.delayLevel);
  const float reverbLevelTarget = KnobGain(settings.reverbLevel);
  const bool delayInput = DelayAllocated() && !AllZero(delaySend, frames);
  const bool reverbInput = reverbMemory_ && !AllZero(reverbSend, frames);
  const bool runDelay = DelayAllocated() && (delayInput || !DelayIdle());
  const bool runReverb = reverbMemory_ && (reverbInput || !ReverbIdle());
  if (!runDelay && !runReverb)
  {
    // Both buses idle: their returns died out a second ago.
    delayLevel_ = delayLevelTarget;
    reverbLevel_ = reverbLevelTarget;
    returns_.fill(0.0f);
    phase_ = static_cast<int>((static_cast<std::size_t>(phase_) + frames) % kBusDecimation);
    return;
  }

  // 1. Decimate the sends that run (both see the same group phase).
  std::array<float, kMaxTicks> delayInputs{};
  std::array<float, kMaxTicks> reverbInputs{};
  std::array<float, kMaxTicks> returns;
  std::size_t ticks = 0;
  // A silent send with drained accumulators decimates to zeros (the tails keep running).
  const std::size_t groupTicks = (static_cast<std::size_t>(phase_) + frames) / kBusDecimation;
  const auto decimate = [&](const float* send, bool input, std::array<float, kTaps>& accumulators, float* out) {
    if (!input && std::all_of(accumulators.begin(), accumulators.end(), [](float v) { return v == 0.0f; }))
    {
      std::fill_n(out, groupTicks, 0.0f);
      return groupTicks;
    }
    return Decimate(send, frames, phase_, accumulators.data(), decimation_.data(), out);
  };
  if (runDelay)
  {
    UpdateDelay(settings);
    if (DelayIdle()) RestartDelay();
    ticks = decimate(delaySend, delayInput, delayDecimator_, delayInputs.data());
  }
  if (runReverb)
  {
    UpdateReverb(settings);
    if (ReverbIdle()) RestartReverb();
    ticks = decimate(reverbSend, reverbInput, reverbDecimator_, reverbInputs.data());
  }

  // 2. Run the buses at the bus rate, levels ramping across the block.
  ticks = std::min(ticks, kMaxTicks);
  if (runDelay)
  {
    RunDelay(delayInputs.data(), returns.data(), ticks, delayLevelTarget);
    delayQuietFrames_ = !delayInput && delayActivity_ == 0 ? std::min(delayQuietFrames_ + frames, DelayIdleFrames()) : 0;
  }
  else
  {
    delayLevel_ = delayLevelTarget;
    std::fill_n(returns.data(), ticks, 0.0f);
  }
  if (runReverb)
  {
    RunReverb(reverbInputs.data(), returns.data(), ticks, reverbLevelTarget);
    reverbQuietFrames_ = !reverbInput && reverbActivity_ == 0 ? std::min(reverbQuietFrames_ + frames, kIdleFrames) : 0;
  }
  else
  {
    reverbLevel_ = reverbLevelTarget;
  }

  // 3. Back to 44.1 kHz, summed into the mix.
  Interpolate(returns.data(), mix, frames, phase_, returns_.data(), interpolation_.data());
  phase_ = static_cast<int>((static_cast<std::size_t>(phase_) + frames) % kBusDecimation);
}

// ---------------------------------------------------------------------------------------------
// Master chain

MasterChain::MasterChain()
    : lowCosine_(std::cos(2.0f * kPi * 200.0f / kSampleRate)),
      lowSine_(std::sin(2.0f * kPi * 200.0f / kSampleRate)),
      highCosine_(std::cos(2.0f * kPi * 5000.0f / kSampleRate)),
      highSine_(std::sin(2.0f * kPi * 5000.0f / kSampleRate))
{
}

bool MasterChain::Bypassed() const
{
  return lowDb_ == 0.0f && highDb_ == 0.0f && compressor_ == 0.0f && tape_ == 0.0f;
}

SYNTH_FX_HOT void MasterChain::Process(float* io, std::size_t frames, int eqLow, int eqHigh, int compressor, int tape)
{
  frames = std::min(frames, kMaxBlockFrames);
  if (frames == 0) return;

  // Shelves: 64 is flat and skips the filter entirely. While the gain moves, the coefficients
  // follow it every kShelfSubBlock frames.
  const auto shelf = [&](Biquad& filter, float& current, float target, bool low) {
    if (current == 0.0f && target == 0.0f) return;
    constexpr float kSubAmount = SmoothingFor(static_cast<float>(kShelfSubBlock));
    for (std::size_t offset = 0; offset < frames; offset += kShelfSubBlock)
    {
      if (current != target)
      {
        current = Approach(current, target, kSubAmount, 0.01f);
        if (current == 0.0f)
        {
          filter.Clear();
          return;
        }
        if (low) filter.LowShelf(lowCosine_, lowSine_, current);
        else filter.HighShelf(highCosine_, highSine_, current);
      }
      const std::size_t end = std::min(frames, offset + kShelfSubBlock);
      Biquad state = filter;
      for (std::size_t index = offset; index < end; ++index) io[index] = state.Process(io[index]);
      filter.z1 = state.z1;
      filter.z2 = state.z2;
    }
    filter.z1 = Flush(filter.z1);
    filter.z2 = Flush(filter.z2);
  };
  shelf(low_, lowDb_, ShelfDecibels(eqLow), true);
  shelf(high_, highDb_, ShelfDecibels(eqHigh), false);

  // Feed-forward compressor: peak detector (5 ms attack, 120 ms release), amount and gain
  // updated every 16 frames (gain in the log2 domain) and the gain interpolated per frame.
  const float compressorTarget = Normalized127(compressor);
  if (compressor_ == 0.0f && compressorTarget == 0.0f)
  {
    envelope_ = 0.0f;
    compressorGain_ = 1.0f;
  }
  else
  {
    constexpr float kSubAmount = SmoothingFor(static_cast<float>(kCompressorSubBlock));
    for (std::size_t offset = 0; offset < frames; offset += kCompressorSubBlock)
    {
      compressor_ = Approach(compressor_, compressorTarget, kSubAmount, 0.0005f);
      const float thresholdOctaves = -kCompressorRangeDb * compressor_ * (1.0f / kDecibelsPerOctave);
      const float makeupOctaves = kCompressorMakeupDb * compressor_ * (1.0f / kDecibelsPerOctave);
      const std::size_t count = std::min(kCompressorSubBlock, frames - offset);
      float envelope = envelope_;
      for (std::size_t index = 0; index < count; ++index)
      {
        const float level = std::fabs(io[offset + index]);
        envelope += (level > envelope ? kCompressorAttack : kCompressorRelease) * (level - envelope);
      }
      envelope_ = Flush(envelope);
      const float over = FastLog2(std::max(envelope_, 1e-6f)) - thresholdOctaves;
      const float target = FastExp2(makeupOctaves - (over > 0.0f ? kCompressorSlope * over : 0.0f));
      const float step = (target - compressorGain_) * Inverse(count);
      float gain = compressorGain_;
      for (std::size_t index = 0; index < count; ++index)
      {
        gain += step;
        io[offset + index] *= gain;
      }
      compressorGain_ = target;
    }
  }

  // Tape: cubic saturation (1x..3x drive, unity small-signal gain plus a little makeup) through a
  // gentle high-frequency roll-off (20 kHz down to 8 kHz), faded in from bypass.
  const float tapeFrom = tape_;
  tape_ = Approach(tape_, Normalized127(tape), Smoothing(frames), 0.0005f);
  if (tapeFrom == 0.0f && tape_ == 0.0f)
  {
    tapeLow_ = 0.0f;
    return;
  }
  if (tape_ != tapeCachedAmount_)
  {
    tapeCachedAmount_ = tape_;
    tapeLowPass_ = OnePoleCoefficient(20000.0f * std::exp2(-1.3219281f * tape_), kSampleRate);
  }
  const float drive = 1.0f + 2.0f * tape_;
  const float makeup = (1.0f + 0.25f * tape_) / drive;
  const float lowPass = tapeLowPass_;
  const float wetFrom = std::min(1.0f, 8.0f * tapeFrom);
  const float wetStep = (std::min(1.0f, 8.0f * tape_) - wetFrom) * Inverse(frames);
  float wet = wetFrom;
  float low = tapeLow_;
  for (std::size_t index = 0; index < frames; ++index)
  {
    const float x = io[index];
    low += lowPass * (SoftClip(drive * x) * makeup - low);
    io[index] = x + wet * (low - x);
    wet += wetStep;
  }
  tapeLow_ = Flush(low);
}

}  // namespace synth::fx::dsp

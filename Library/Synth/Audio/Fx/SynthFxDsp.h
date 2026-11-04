#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include "Library/Synth/Audio/SynthFx.h"

// Effects DSP for the groovebox mixer (parameters in SynthFx.h). Pure single-precision code with
// no AMY dependency, so every processor can be unit-tested on its own.
//
// Signal domain: mono float at 44.1 kHz in the runtime's mix units ((L + R) / 65536 of AMY's
// output; 1.0 is about 10 dB over full scale at the output stage). Processors take blocks of at most kMaxBlockFrames
// frames and update their coefficients once per block, or every 16..32 frames while a smoothed
// control moves (DJ filter, chorus taps, master shelves and compressor), interpolating gains
// within the block so knob moves do not zipper.
//
// Written for the ESP32-S3, whose FPU has no fast divide and no double precision: no division,
// transcendental function or double runs per sample (saturation is a cubic), and
// the delay and reverb share one 11.025 kHz rate behind polyphase FIR decimators and a single
// interpolator. Memory is allocated only by the explicit Allocate* calls and is kept until destruction:
// the delay and reverb lines in PSRAM (read and written in order, at the bus rate), the chorus lines in
// internal RAM (read anywhere in them, every sample).
//
// Defining CARDENZA_FX_IRAM places the per-sample kernels in IRAM (ESP-IDF's .iram1 sections), out
// of reach of flash-cache misses, at the cost of internal RAM.
#if defined(ESP_PLATFORM) && defined(CARDENZA_FX_IRAM)
#define SYNTH_FX_HOT __attribute__((section(".iram1.synth_fx")))
#else
#define SYNTH_FX_HOT
#endif

namespace synth::fx::dsp {

inline constexpr float kSampleRate = 44100.0f;
inline constexpr std::size_t kMaxBlockFrames = 256;

inline constexpr std::size_t kChorusLineFrames = 512;  // int16, 11.6 ms: 1 KB per track
inline constexpr int kBusDecimation = 4;                // delay and reverb run at 11.025 kHz
inline constexpr float kBusRate = kSampleRate / static_cast<float>(kBusDecimation);
inline constexpr std::size_t kDelayLineFrames = 8192;   // int16 at 11.025 kHz, 743 ms: 16 KB
// Allocated instead only once a delay time needs it: 1.49 s, 32 KB.
inline constexpr std::size_t kDelayLongLineFrames = 16384;
// Both are kept in blocks of 4 KB: on the device the heap is too broken up for 16 KB in one piece.
inline constexpr std::size_t kDelayChunkFrames = 2048;
// Reverb delays at the bus rate (mutually prime, 69..92 ms lines, 5..10 ms diffusers). The lines
// share one 1024-slot ring and the diffusers one 128-slot ring, interleaved, so a single counter
// addresses them all.
inline constexpr std::array<std::uint32_t, 4> kReverbLineFrames{{757, 853, 937, 1009}};
inline constexpr std::array<std::uint32_t, 3> kReverbDiffuserFrames{{59, 89, 113}};
inline constexpr std::uint32_t kReverbLineRing = 1024;
inline constexpr std::uint32_t kReverbDiffuserRing = 128;
inline constexpr std::size_t kReverbMemoryFrames = 4 * kReverbLineRing + 3 * kReverbDiffuserRing;  // 8.75 KB

// Cubic soft clipper: unit slope at 0, reaches +-1 with zero slope at |x| = 1.5.
inline float SoftClip(float x)
{
  x = x < -1.5f ? -1.5f : (x > 1.5f ? 1.5f : x);
  return x - (4.0f / 27.0f) * x * x * x;
}

// log2 (within 0.005, i.e. 0.03 dB) and 2^x (within 0.015 %), for gain computers.
inline float FastLog2(float x)
{
  std::uint32_t bits = 0;
  std::memcpy(&bits, &x, sizeof(bits));
  const float exponent = static_cast<float>(static_cast<int>((bits >> 23) & 255u) - 128);
  bits = (bits & 0x007FFFFFu) | 0x3F800000u;
  float mantissa = 0.0f;
  std::memcpy(&mantissa, &bits, sizeof(mantissa));
  return exponent + (-0.34484843f * mantissa + 2.02466578f) * mantissa - 0.67487759f;
}

inline float FastExp2(float x)
{
  if (x < -126.0f) return 0.0f;
  if (x > 126.0f) x = 126.0f;
  const int whole = static_cast<int>(x < 0.0f ? x - 1.0f : x);
  const float fraction = x - static_cast<float>(whole);
  const float power = 1.0f + fraction * (0.69583355f + fraction * (0.22606716f + fraction * 0.07809885f));
  const std::uint32_t bits = static_cast<std::uint32_t>(whole + 127) << 23;
  float scale = 0.0f;
  std::memcpy(&scale, &bits, sizeof(scale));
  return scale * power;
}

// Linear gain of a send or return knob (0..127): (value / 127)^2.
inline float KnobGain(int value)
{
  const float normalized = static_cast<float>(value < 0 ? 0 : (value > 127 ? 127 : value)) * (1.0f / 127.0f);
  return normalized * normalized;
}

// Synced delay time in seconds, halved (one note value at a time) until it fits the delay line.
float DelaySeconds(int division, float bpm);
// Longest delay the tape delay can play.
float MaxDelaySeconds();

struct Biquad {
  float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
  float z1 = 0.0f, z2 = 0.0f;

  float Process(float x)
  {
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
  void Clear() { z1 = z2 = 0.0f; }
  // Shelves with slope 1; cosine and sine of the corner frequency (2 pi f / fs) are precomputed.
  void LowShelf(float cosine, float sine, float gainDb);
  void HighShelf(float cosine, float sine, float gainDb);
};

// Insert settings with their SynthFx.h defaults.
struct TrackInsertSettings {
  int drive = TrackParameterDefault("fx.drive");
  int crush = TrackParameterDefault("fx.crush");
  int filter = TrackParameterDefault("fx.filter");
  int resonance = TrackParameterDefault("fx.resonance");
  int chorus = TrackParameterDefault("fx.chorus");
};

// True when every insert of `settings` is off (the DJ filter inside its dead zone).
bool IsNeutral(const TrackInsertSettings& settings);

// Per-track insert chain: drive -> crush -> DJ filter -> chorus.
class TrackInserts {
 public:
  static constexpr std::size_t kChorusMemoryBytes = kChorusLineFrames * sizeof(std::int16_t);

  // The chorus needs its delay line; until then (or if the memory is missing) it stays bypassed.
  bool AllocateChorus();
  bool HasChorus() const { return chorusLine_ != nullptr; }
  // Back to neutral (all stages bypassed), keeping the chorus memory.
  void Reset();
  // Processes `frames` (<= kMaxBlockFrames) samples in place. A silent block on a quiet chain
  // costs only the parameter update.
  void Process(float* io, std::size_t frames, const TrackInsertSettings& settings);
  // Every stage has reached bypass and the chorus has faded out: the track may leave the effects path.
  bool Settled() const;

 private:
  struct DriveStage {
    float gain = 1.0f, makeup = 1.0f, wet = 0.0f;
  };
  void SnapToTargets(const TrackInsertSettings& settings);
  void ProcessDrive(float* io, std::size_t frames, float target);
  void ProcessCrush(float* io, std::size_t frames, float target);
  void ProcessFilter(float* io, std::size_t frames, float targetPosition, float targetResonance);
  void UpdateFilterCoefficients();
  void ProcessChorus(float* io, std::size_t frames, int mode);
  void ClearChorus();
  DriveStage DriveFor(float amount);

  float drive_ = 0.0f;  // smoothed 0..1
  float driveCachedAmount_ = -1.0f;
  DriveStage driveCached_{};
  float crush_ = 0.0f;  // smoothed 0..1
  float crushCachedAmount_ = -1.0f;
  float crushHoldStep_ = 1.0f, crushLevels_ = 32768.0f, crushInverseLevels_ = 1.0f / 32768.0f;
  float crushPhase_ = 1.0f;
  float crushHeld_ = 0.0f;
  float filterPosition_ = static_cast<float>(kFilterCenter);  // smoothed 0..127
  float filterResonance_ = static_cast<float>(TrackParameterDefault("fx.resonance"));  // smoothed 0..127
  bool filterActive_ = false;
  float filterMix_ = 0.0f;
  float filterG_ = 0.0f, filterK_ = 2.0f, filterA1_ = 1.0f, filterA2_ = 0.0f, filterA3_ = 0.0f;
  bool filterHighPass_ = false;
  float filterIc1_ = 0.0f, filterIc2_ = 0.0f;
  float chorusMix_ = 0.0f;   // 0..1 wet fade
  float chorusDepth_ = 0.0f; // 0..1 of the centre delay
  float chorusRate_ = 0.5f;  // Hz
  float chorusPhase_ = 0.0f;
  float chorusLow_ = 0.0f;   // low-passed taps, subtracted to high-pass the wet signal
  std::size_t chorusWrite_ = 0;
  std::unique_ptr<std::int16_t[]> chorusLine_;
  int quietBlocks_ = 0;
};

struct SendBusSettings {
  float delaySeconds = 0.375f;
  int delayFeedback = kMasterParams[static_cast<std::size_t>(MasterParam::DelayFeedback)].defaultValue;
  int delayTone = kMasterParams[static_cast<std::size_t>(MasterParam::DelayTone)].defaultValue;
  int delayLevel = kMasterParams[static_cast<std::size_t>(MasterParam::DelayLevel)].defaultValue;
  int reverbSize = kMasterParams[static_cast<std::size_t>(MasterParam::ReverbSize)].defaultValue;
  int reverbDamp = kMasterParams[static_cast<std::size_t>(MasterParam::ReverbDamp)].defaultValue;
  int reverbLowCut = kMasterParams[static_cast<std::size_t>(MasterParam::ReverbLowCut)].defaultValue;
  int reverbLevel = kMasterParams[static_cast<std::size_t>(MasterParam::ReverbLevel)].defaultValue;
};

// The delay and reverb buses. Both sends go through polyphase FIR decimators (20 taps, -6 dB at
// 4.5 kHz) to 11.025 kHz, where the buses run; their returns are summed there and interpolated
// back to 44.1 kHz once.
//   Delay: tape-style, int16 line of 743 ms, soft-clipped feedback through the TONE low-pass and
//   an 80 Hz high-pass, read-head glide on time changes.
//   Reverb: 4-line feedback delay network (Householder) with three input allpass diffusers,
//   per-line damping, decay gains from an RT60 and a one-pole LOCUT on its input; int16 storage.
class SendBuses {
 public:
  static constexpr std::size_t kDelayMemoryBytes = kDelayLineFrames * sizeof(std::int16_t);
  static constexpr std::size_t kReverbMemoryBytes = kReverbMemoryFrames * sizeof(std::int16_t);

  SendBuses();
  // The long line replaces a short one (and restarts the echoes); if it does not fit, the short one stays.
  bool AllocateDelay(bool longLine);
  bool DelayAllocated() const { return delayFrames_ != 0; }
  bool AllocateReverb();
  bool ReverbAllocated() const { return reverbMemory_ != nullptr; }
  // Feeds `frames` samples of both sends (all zero for a bus without memory) and adds the summed
  // returns, each times its LEVEL gain, to `mix`.
  void Process(const float* delaySend, const float* reverbSend, float* mix, std::size_t frames, const SendBusSettings& settings);
  // No input and a tail below -100 dBFS for a second: the bus may stop processing. The delay also waits
  // as long as its line holds: an echo longer than a second is silence until it comes out.
  bool DelayIdle() const { return delayQuietFrames_ >= DelayIdleFrames(); }
  bool ReverbIdle() const { return reverbQuietFrames_ >= kIdleFrames; }
  // Peaks of the two returns (after LEVEL) during the last Process call.
  float DelayPeak() const { return delayPeak_; }
  float ReverbPeak() const { return reverbPeak_; }

 private:
  static constexpr std::size_t kIdleFrames = 44100;
  static constexpr std::size_t kTaps = 5;  // polyphase taps per phase (20-tap prototype)
  static constexpr std::size_t kMaxTicks = kMaxBlockFrames / kBusDecimation + 1;
  std::size_t DelayIdleFrames() const { return kIdleFrames + static_cast<std::size_t>(delayFrames_) * kBusDecimation; }
  float DelayTick(float input);
  float DelayTickAt(float input, std::uint32_t distance);
  float ReverbTick(float input);
  // Bus-rate loops over one block's ticks: write (delay) or add (reverb) the levelled returns.
  void RunDelay(const float* inputs, float* returns, std::size_t ticks, float levelTarget);
  void RunReverb(const float* inputs, float* returns, std::size_t ticks, float levelTarget);
  void RestartDelay();
  void RestartReverb();
  void UpdateDelay(const SendBusSettings& settings);
  void UpdateReverb(const SendBusSettings& settings);

  // Filter coefficients live in RAM next to the state, so the per-sample loops load them with
  // one instruction instead of materialising constants.
  std::array<float, kBusDecimation * kTaps> decimation_{};
  std::array<float, kBusDecimation * kTaps> interpolation_{};
  int phase_ = 0;  // input frames into the current group of kBusDecimation
  std::array<float, kTaps> delayDecimator_{};
  std::array<float, kTaps> reverbDecimator_{};
  std::array<float, kTaps> returns_{};  // summed returns at the bus rate, newest first

  // Sample `index` ticks after the start of the line, wrapped.
  std::int16_t& DelaySample(std::uint32_t index)
  {
    const std::uint32_t at = index & (delayFrames_ - 1);
    return delayChunks_[at / kDelayChunkFrames][at % kDelayChunkFrames];
  }

  std::array<std::unique_ptr<std::int16_t[]>, kDelayLongLineFrames / kDelayChunkFrames> delayChunks_;
  std::uint32_t delayFrames_ = 0;  // a power of two, whole chunks
  std::uint32_t delayWrite_ = 0;
  float delayTicks_ = 1.0f, delayTargetTicks_ = 1.0f;  // read-head delay in bus ticks
  float delayFeedback_ = 0.0f;
  float delayTone_ = 0.0f, delayToneHz_ = -1.0f, delayToneCoefficient_ = 1.0f;
  float delayHighPassLow_ = 0.0f;
  float delayLevel_ = 0.0f, delayLevelStep_ = 0.0f;
  std::int32_t delayActivity_ = 0;  // OR of the line samples read this block: 0 = silent loop
  float delayPeak_ = 0.0f;
  std::size_t delayQuietFrames_ = kIdleFrames;

  std::unique_ptr<std::int16_t[]> reverbMemory_;  // lines [ring][4], then diffusers [ring][3]
  std::uint32_t reverbCounter_ = 0;
  std::array<float, 4> reverbDamping_{};
  std::array<float, 4> reverbDecay_{};
  float reverbDampCoefficient_ = 1.0f;
  // Damped decay per line: state = hold * state + feed[line] * tap.
  float reverbHold_ = 0.0f;
  std::array<float, 4> reverbFeed_{};
  float reverbLowCutCoefficient_ = 0.0f;
  float reverbLowCutState_ = 0.0f;
  int reverbSize_ = -1, reverbDamp_ = -1, reverbLowCut_ = -1;
  float reverbLevel_ = 0.0f, reverbLevelStep_ = 0.0f;
  std::int32_t reverbActivity_ = 0;  // OR of the line samples read this block: 0 = silent tail
  float reverbPeak_ = 0.0f;
  std::size_t reverbQuietFrames_ = kIdleFrames;
};

// Master bus after the returns: low shelf (200 Hz) and high shelf (5 kHz) +-12 dB, feed-forward
// compressor, tape saturation. Each stage is an exact bypass at its neutral value.
class MasterChain {
 public:
  MasterChain();
  void Process(float* io, std::size_t frames, int eqLow, int eqHigh, int compressor, int tape);
  // Every stage is bypassed and has finished its fade.
  bool Bypassed() const;

 private:
  Biquad low_, high_;
  float lowDb_ = 0.0f, highDb_ = 0.0f;
  float lowCosine_, lowSine_, highCosine_, highSine_;
  float compressor_ = 0.0f;
  float envelope_ = 0.0f;
  float compressorGain_ = 1.0f;
  float tape_ = 0.0f;
  float tapeLow_ = 0.0f;
  float tapeCachedAmount_ = -1.0f;
  float tapeLowPass_ = 1.0f;
};

}  // namespace synth::fx::dsp

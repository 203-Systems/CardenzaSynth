// SPDX-License-Identifier: BSD-3-Clause
// One singing voice: a 64-step wavetable oscillator that walks the segments of a pronunciation (a
// short consonant, then the vowel it holds). The segments, envelope, glide and tie are the NullPerator
// tracker's vocal::Voice (sources/Application/Instruments/VocalEngine.h, ours), in float and a block
// at a time. The oscillator is this firmware's own, in place of that tracker's chip core: the wave is
// stepped as the chip plays it and averaged over each frame (ChipSample), and its pitch modulation
// follows the chip's rule (ChipPitchRatio).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Library/Synth/Core/Engines/Vocal/VocalPack.h"

namespace synth::vocal {

inline constexpr float kVoiceSampleRate = 44100.0f;
// Pitch (glide, vibrato) is worked out once per this many frames (0.7 ms).
inline constexpr std::uint32_t kControlFrames = 32;
// The vibrato comes in over this long once its delay has passed.
inline constexpr std::uint32_t kVibratoFadeFrames = 6615;  // 150 ms
// What a modulation table steps at for a rate of 1: the chip's clock over 65536, in steps per frame.
inline constexpr float kModulationStepsPerFrame = 1789773.0f / 65536.0f / kVoiceSampleRate;

constexpr std::uint32_t Frames(unsigned milliseconds)
{
  return static_cast<std::uint32_t>((static_cast<std::uint64_t>(milliseconds) * 44100u + 500u) / 1000u);
}

// What the voices of a track share: the engine's parameters, worked out when one of them changes.
struct VoiceSettings {
  int transpose = 0;                        // semitones
  unsigned attackMs = 4;
  unsigned releaseMs = 30;
  std::uint32_t glideFrames = 0;            // how long a tied note takes to reach its pitch
  float vibratoDepth = 0.0f;                // semitones at full swing
  float vibratoStep = 0.0f;                 // cycles per control period
  std::uint32_t vibratoDelayFrames = 0;     // from the start of a note that is not tied
};

class Voice {
public:
  bool Active() const { return active_; }
  bool Releasing() const { return releasing_; }
  // Sounding and not let go.
  bool Held() const { return active_ && !releasing_; }
  int Note() const { return note_; }
  float Velocity() const { return velocity_; }
  // The pronunciation it sings (index into the pack's entries).
  unsigned Entry() const { return entry_; }
  // Frames rendered since it started.
  std::uint32_t Age() const { return age_; }
  float Envelope() const { return envelope_; }

  void Stop()
  {
    active_ = false;
    releasing_ = false;
  }

  // A new note: the pronunciation from its first segment, the envelope from silence.
  void Start(const Pack& pack, unsigned entry, int note, float velocity, const VoiceSettings& settings)
  {
    pack_ = &pack;
    note_ = note;
    velocity_ = velocity;
    phase_ = 0;
    filtered_ = 0.0f;
    control_ = 0;
    age_ = 0;
    pitchRatio_ = slideStart_ = slideTarget_ = 1.0f;
    slideFrame_ = slideTotal_ = 0;
    vibratoPhase_ = 0.0f;
    envelope_ = 0.0f;
    Begin(entry, settings);
  }

  // Another pronunciation on the note that sounds: the wave changes, the envelope goes on from where
  // it is. False when the voice is silent or let go.
  bool Pronounce(unsigned entry, const VoiceSettings& settings)
  {
    if (!Held()) return false;
    Begin(entry, settings);
    return true;
  }

  // A tied note (legato): the same pronunciation where it is, at a new pitch, reached from the pitch
  // sounding now over the glide time.
  void Tie(int note, const VoiceSettings& settings)
  {
    if (!Held()) return;
    const float sounding = noteIncrement_ * pitchRatio_;
    note_ = note;
    const std::uint8_t* segment = pack_->Segment(firstSegment_ + segment_);
    const std::uint8_t* next = pack_->SelectWave(U16(segment + field::kSegmentPhoneme), note_ + settings.transpose);
    if (next != record_)
    {
      // Another stretch of the keyboard has another wave; its modulation starts over.
      TakeWave(next);
      modulationDelay_ = segmentFrames_ + Frames(U16(next + field::kWaveModDelayMs));
    }
    UpdateNoteIncrement(settings);
    if (!fixedPitch_ && noteIncrement_ > 0.0f)
    {
      pitchRatio_ = sounding / noteIncrement_;
      slideStart_ = pitchRatio_;
      slideTarget_ = 1.0f;
      slideFrame_ = 0;
      slideTotal_ = settings.glideFrames;
      if (slideTotal_ == 0) pitchRatio_ = 1.0f;
    }
    attackFrames_ = (std::max<std::uint32_t>)(1u, Frames((std::max)(settings.attackMs, phonemeAttackMs_)));
    UpdateIncrement(settings);
  }

  void Release(const VoiceSettings& settings)
  {
    if (!active_ || releasing_) return;
    releasing_ = true;
    releaseEnvelope_ = envelope_;
    releaseFrames_ = 0;
    releaseTotal_ = (std::max<std::uint32_t>)(1u, Frames(settings.releaseMs));
  }

  // The engine's tuning changed under a sounding voice.
  void Retune(const VoiceSettings& settings)
  {
    if (!active_) return;
    UpdateNoteIncrement(settings);
    UpdateIncrement(settings);
  }

  // Adds `frames` frames to `out`, at `gain` for a full-scale wave at full volume.
  void Render(float* out, std::size_t frames, const VoiceSettings& settings, float gain)
  {
    std::size_t done = 0;
    while (done < frames && active_)
    {
      if (!releasing_ && segmentDuration_ != 0 && segmentFrames_ >= segmentDuration_)
      {
        if (segment_ + 1 < segmentCount_)
        {
          ++segment_;
          SelectSegment(settings);
        }
        else
        {
          Release(settings);
        }
      }
      if (control_ == 0)
      {
        control_ = kControlFrames;
        ControlTick(settings);
      }
      std::size_t run = (std::min<std::size_t>)(frames - done, control_);
      if (!releasing_ && segmentDuration_ != 0) run = (std::min<std::size_t>)(run, segmentDuration_ - segmentFrames_);
      if (releasing_) run = (std::min<std::size_t>)(run, releaseTotal_ - releaseFrames_);
      RenderRun(out + done, run, gain);
      done += run;
      control_ -= static_cast<std::uint32_t>(run);
      if (releasing_ && releaseFrames_ >= releaseTotal_) Stop();
    }
  }

  // The chip's pitch modulation: what its 7-bit counter and 6-bit depth add to the pitch, in 64ths of
  // it. The rounding and the wrap are the hardware's (the FDS sound channel's modulation unit as the
  // NESdev wiki documents it); they shape the sound of a deep modulation.
  static float ChipPitchRatio(int counter, int depth)
  {
    int amount = counter * depth;
    const int remainder = amount & 0xF;
    amount >>= 4;
    if (remainder > 0 && (amount & 0x80) == 0) amount += counter < 0 ? -1 : 2;
    if (amount >= 192) amount -= 256;
    else if (amount < -64) amount += 256;
    return 1.0f + static_cast<float>(amount) * (1.0f / 64.0f);
  }

  // One step of a modulation table: the counter moves by the step's code and wraps in 7 bits.
  static int StepModulation(int counter, unsigned code)
  {
    constexpr int kOffsets[8] = {0, 1, 2, 4, 0, -4, -2, -1};
    counter = code == 4 ? 0 : counter + kOffsets[code & 7];
    if (counter > 63) counter -= 128;
    if (counter < -64) counter += 128;
    return counter;
  }

  // The 64-step wave as the chip plays it, without interpolation: its mean over the stretch of phase
  // one output frame covers (`from` up to `from + increment`), which is what a chip clocked forty
  // times a frame averages to. `reciprocal` is 1 / increment.
  static float ChipSample(const std::int8_t* wave, std::uint32_t from, std::uint32_t increment, float reciprocal)
  {
    constexpr std::uint32_t kStep = 1u << 26;
    std::uint32_t index = from >> 26;
    const std::uint32_t offset = from & (kStep - 1);
    if (increment <= kStep - offset) return static_cast<float>(wave[index]);
    float sum = static_cast<float>(wave[index]) * static_cast<float>(kStep - offset);
    std::uint32_t left = increment - (kStep - offset);
    index = (index + 1) & 63;
    while (left > kStep)
    {
      sum += static_cast<float>(wave[index]) * static_cast<float>(kStep);
      left -= kStep;
      index = (index + 1) & 63;
    }
    return (sum + static_cast<float>(wave[index]) * static_cast<float>(left)) * reciprocal;
  }

private:
  void Begin(unsigned entry, const VoiceSettings& settings)
  {
    const std::uint8_t* record = pack_->Entry(entry);
    entry_ = static_cast<std::uint16_t>(entry);
    firstSegment_ = static_cast<std::uint16_t>(U16(record + 2));
    segmentCount_ = static_cast<std::uint16_t>(U16(record + 4));
    segment_ = 0;
    releasing_ = false;
    active_ = true;
    frames_ = 0;
    SelectSegment(settings);
    attackStart_ = envelope_;
  }

  // The wave is copied: the pack may be in PSRAM, and these 64 bytes are read for every frame. The
  // chip changes its wave at once, and the wave's modulation starts with it.
  void TakeWave(const std::uint8_t* record)
  {
    std::memcpy(wave_, record, sizeof(wave_));
    record_ = record;
    // Volume 32 is the chip's loudest; 34 here gives the level the tracker plays the same pack at
    // (its chip core's output is 16/17 of the wave times the volume, measured 2026-11-14).
    volume_ = static_cast<float>(record[field::kWaveVolume]) * (1.0f / 34.0f);
    modulationDepth_ = record[field::kWaveModDepth];
    modulationStep_ = static_cast<float>(U16(record + field::kWaveModRate)) * kModulationStepsPerFrame;
    std::memcpy(modulationTable_, record + field::kWaveModTable, sizeof(modulationTable_));
    modulationAccumulator_ = 0.0f;
    modulationIndex_ = 0;
    modulationCounter_ = 0;
    modulationRatio_ = 1.0f;
  }

  void SelectSegment(const VoiceSettings& settings)
  {
    const std::uint8_t* segment = pack_->Segment(firstSegment_ + segment_);
    const unsigned phonemeIndex = U16(segment + field::kSegmentPhoneme);
    const std::uint8_t* phoneme = pack_->Phoneme(phonemeIndex);
    pitchMode_ = phoneme[field::kPhonemePitchMode];
    pitchValue_ = phoneme[field::kPhonemePitchValue];
    fixedPitch_ = pitchMode_ == 2;
    phonemeGain_ = static_cast<float>(phoneme[field::kPhonemeGain]) * (1.0f / 255.0f);
    phonemeAttackMs_ = U16(phoneme + field::kPhonemeAttackMs);
    segmentDuration_ = Frames(U16(segment + field::kSegmentDurationMs));
    segmentGain_ = static_cast<float>(segment[field::kSegmentGain]) * (1.0f / 255.0f);
    attackFrames_ = (std::max<std::uint32_t>)(1u, Frames((std::max)(settings.attackMs, phonemeAttackMs_)));
    const std::uint8_t* wave = pack_->SelectWave(phonemeIndex, note_ + settings.transpose);
    TakeWave(wave);
    modulationDelay_ = Frames(U16(wave + field::kWaveModDelayMs));
    segmentFrames_ = 0;
    UpdateNoteIncrement(settings);
    UpdateIncrement(settings);
  }

  // The phase a frame advances at the pitch of the note (or of the phoneme, when it has its own).
  void UpdateNoteIncrement(const VoiceSettings& settings)
  {
    int note = note_ + settings.transpose;
    if (pitchMode_ == 1) note += Signed(pitchValue_);
    else if (pitchMode_ == 2) note = pitchValue_;
    note = std::clamp(note, 0, 119);
    // 440 Hz at note 69, as 32-bit phase per frame.
    noteIncrement_ = (440.0f * 4294967296.0f / kVoiceSampleRate) * std::exp2(static_cast<float>(note - 69) * (1.0f / 12.0f));
  }

  void UpdateIncrement(const VoiceSettings& settings)
  {
    float increment = noteIncrement_;
    // A phoneme with a pitch of its own (K, S) stays there through glides and vibrato.
    if (!fixedPitch_)
    {
      increment *= pitchRatio_;
      if (settings.vibratoDepth > 0.0f && age_ > settings.vibratoDelayFrames)
      {
        const std::uint32_t since = age_ - settings.vibratoDelayFrames;
        const float fade = since >= kVibratoFadeFrames ? 1.0f : static_cast<float>(since) * (1.0f / static_cast<float>(kVibratoFadeFrames));
        // A parabola for the sine: x in -1..1 is one cycle.
        const float x = 2.0f * vibratoPhase_ - 1.0f;
        const float sine = -4.0f * x * (1.0f - std::fabs(x));
        // One semitone up is 5.9463 % more.
        increment *= 1.0f + 0.0594631f * settings.vibratoDepth * fade * sine;
      }
    }
    baseIncrement_ = std::clamp(increment, 1.0f, 2147483648.0f);
    ApplyModulation();
  }

  void ApplyModulation()
  {
    const float increment = std::clamp(baseIncrement_ * modulationRatio_, 1.0f, 2147483648.0f);
    increment_ = static_cast<std::uint32_t>(increment);
    reciprocal_ = 1.0f / static_cast<float>(increment_);
  }

  void ControlTick(const VoiceSettings& settings)
  {
    if (slideFrame_ < slideTotal_)
    {
      slideFrame_ = (std::min)(slideTotal_, slideFrame_ + kControlFrames);
      pitchRatio_ = slideStart_ + (slideTarget_ - slideStart_) * (static_cast<float>(slideFrame_) / static_cast<float>(slideTotal_));
    }
    vibratoPhase_ += settings.vibratoStep;
    if (vibratoPhase_ >= 1.0f) vibratoPhase_ -= 1.0f;
    UpdateIncrement(settings);
  }

  // `frames` frames within one segment and one control period. The state the loop touches stays in
  // locals: this chip's FPU has no division (none per frame here), and state read back from memory
  // for every frame costs more than the sound.
  void RenderRun(float* out, std::size_t frames, float gain)
  {
    const float level = gain * phonemeGain_ * segmentGain_ * volume_ * (1.0f / 128.0f);
    const bool modulated = modulationDepth_ != 0 && modulationStep_ > 0.0f;
    const bool releasing = releasing_;
    const float attackSlope = (1.0f - attackStart_) / static_cast<float>(attackFrames_);
    const float releaseSlope = releasing ? releaseEnvelope_ / static_cast<float>(releaseTotal_) : 0.0f;
    std::uint32_t phase = phase_;
    std::uint32_t increment = increment_;
    float reciprocal = reciprocal_;
    std::uint32_t pronounced = frames_;
    std::uint32_t inSegment = segmentFrames_;
    std::uint32_t released = releaseFrames_;
    float filtered = filtered_;
    float envelope = envelope_;
    for (std::size_t frame = 0; frame < frames; ++frame)
    {
      if (releasing)
      {
        ++released;
        envelope = releaseEnvelope_ - releaseSlope * static_cast<float>(released);
        if (envelope < 0.0f) envelope = 0.0f;
      }
      else if (pronounced + 1 >= attackFrames_)
      {
        envelope = 1.0f;
      }
      else
      {
        envelope = attackStart_ + attackSlope * static_cast<float>(pronounced + 1);
      }
      if (modulated && inSegment >= modulationDelay_ && StepModulationUnit())
      {
        increment = increment_;
        reciprocal = reciprocal_;
      }
      const float value = ChipSample(wave_, phase, increment, reciprocal);
      phase += increment;
      filtered += (value - filtered) * 0.5f;
      out[frame] += filtered * envelope * level;
      ++pronounced;
      ++inSegment;
    }
    phase_ = phase;
    age_ += static_cast<std::uint32_t>(frames);
    frames_ = pronounced;
    segmentFrames_ = inSegment;
    releaseFrames_ = released;
    filtered_ = filtered;
    envelope_ = envelope;
  }

  // True when the table stepped (the increment changed).
  bool StepModulationUnit()
  {
    modulationAccumulator_ += modulationStep_;
    if (modulationAccumulator_ < 1.0f) return false;
    while (modulationAccumulator_ >= 1.0f)
    {
      modulationAccumulator_ -= 1.0f;
      // The chip's table has 64 steps: each of the wave's 32 values twice.
      modulationCounter_ = StepModulation(modulationCounter_, modulationTable_[modulationIndex_ >> 1]);
      modulationIndex_ = (modulationIndex_ + 1) & 63u;
    }
    modulationRatio_ = ChipPitchRatio(modulationCounter_, modulationDepth_);
    ApplyModulation();
    return true;
  }

  const Pack* pack_ = nullptr;
  const std::uint8_t* record_ = nullptr;  // the pack's wave record in wave_
  bool active_ = false;
  bool releasing_ = false;
  bool fixedPitch_ = false;
  std::uint8_t pitchMode_ = 0;
  std::uint8_t pitchValue_ = 0;
  std::uint8_t modulationDepth_ = 0;
  int note_ = 60;
  float velocity_ = 1.0f;
  std::uint16_t entry_ = 0;
  std::uint16_t firstSegment_ = 0;
  std::uint16_t segmentCount_ = 0;
  std::uint16_t segment_ = 0;
  unsigned phonemeAttackMs_ = 0;
  float phonemeGain_ = 1.0f;
  float segmentGain_ = 1.0f;
  float volume_ = 1.0f;
  std::uint32_t phase_ = 0;
  std::uint32_t increment_ = 1;
  float reciprocal_ = 1.0f;
  float noteIncrement_ = 1.0f;
  float baseIncrement_ = 1.0f;
  float pitchRatio_ = 1.0f;
  float slideStart_ = 1.0f;
  float slideTarget_ = 1.0f;
  std::uint32_t slideFrame_ = 0;
  std::uint32_t slideTotal_ = 0;
  float vibratoPhase_ = 0.0f;
  std::uint32_t control_ = 0;
  std::uint32_t age_ = 0;
  std::uint32_t frames_ = 0;  // since the pronunciation began
  std::uint32_t segmentFrames_ = 0;
  std::uint32_t segmentDuration_ = 0;
  std::uint32_t attackFrames_ = 1;
  std::uint32_t releaseFrames_ = 0;
  std::uint32_t releaseTotal_ = 1;
  float envelope_ = 0.0f;
  float attackStart_ = 0.0f;
  float releaseEnvelope_ = 0.0f;
  float filtered_ = 0.0f;
  float modulationStep_ = 0.0f;
  float modulationAccumulator_ = 0.0f;
  float modulationRatio_ = 1.0f;
  std::uint32_t modulationDelay_ = 0;
  std::uint32_t modulationIndex_ = 0;
  int modulationCounter_ = 0;
  std::uint8_t modulationTable_[kModulationSteps]{};
  std::int8_t wave_[kWaveSamples]{};
};

}  // namespace synth::vocal

#include "Library/Synth/Audio/Braids/BraidsVoice.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

#include "Library/Memory/Memory.h"
#include "Library/Synth/Audio/Braids/BraidsModels.h"

// Braids' headers bring stmlib's macros (CLIP, CONSTRAIN, ...) with them: last, and only here.
#include "braids/macro_oscillator.h"

namespace synth::braids {

namespace {

constexpr float kSampleRate = 44100.0f;  // What the tables are made for (resources/generate.py).
// Braids renders 24 samples at a time on the module; its scratch buffers hold no more. The envelope,
// the LFO and the macro controls move once a chunk (1837 times a second).
constexpr std::size_t kChunkFrames = 24;
constexpr std::size_t kControlFrames = 8;  // An AMY block is ten chunks and 16 frames: both multiples.
// A voice at full scale and amplitude 1, in AMY's units: about where a Juno or DX7 patch's voice is
// before PresetLevels.inc evens the patches out, so that the models' own table (ModelLevels.inc, the
// runtime's) corrects around zero. AMY's samples have eight bits above 1.
constexpr float kVoiceGain = 2.0f;
constexpr float kInt16ToSample = 256.0f;  // S8.23 from 16 bits
constexpr std::int32_t kMaxParameter = 32767;
constexpr std::int32_t kMaxPitch = 16383;
// What the delay lines leave in PSRAM for patterns and samples.
constexpr std::size_t kDelayLinesSlowReserveBytes = 160 * 1024;
const std::uint8_t kNoSync[kChunkFrames] = {};

// The sound of a track: the engine's values and what the voices read, worked out when one changes.
struct Params {
  int model = 0;
  int timbre = 64;
  int color = 64;
  int tune = 0;
  int fine = 0;
  int modAttack = 0;
  int modDecay = 48;
  int modTimbre = 0;
  int modColor = 0;
  int modPitch = 0;
  int lfoRate = 64;
  int lfoTimbre = 0;
  int lfoColor = 0;
  int lfoPitch = 0;

  ::braids::MacroOscillatorShape shape = ::braids::MACRO_OSC_SHAPE_CSAW;
  std::int32_t timbreBase = 0;
  std::int32_t colorBase = 0;
  std::int32_t pitchOffset = 0;
  bool modulated = false;      // Any envelope or LFO amount: otherwise neither is computed.
  float attackPerFrame = 1.0f; // The envelope rises by this a frame, to 1,
  float decayPerControl = 0.0f;  // then falls by this factor every kControlFrames.
  float envTimbre = 0.0f;      // Amounts at envelope 1 / LFO 1, in the units of what they move.
  float envColor = 0.0f;
  float envPitch = 0.0f;
  std::uint32_t lfoPerFrame = 0;
  float lfoToTimbre = 0.0f;
  float lfoToColor = 0.0f;
  float lfoToPitch = 0.0f;
};

float Unit(int value, int maximum)
{
  return static_cast<float>(std::clamp(value, -maximum, maximum)) / static_cast<float>(maximum);
}

void Derive(Params& p)
{
  p.shape = static_cast<::braids::MacroOscillatorShape>(std::clamp(p.model, 0, static_cast<int>(kModelCount) - 1));
  p.timbreBase = std::clamp(p.timbre, 0, 127) * kMaxParameter / 127;
  p.colorBase = std::clamp(p.color, 0, 127) * kMaxParameter / 127;
  p.pitchOffset = std::clamp(p.tune, -24, 24) * kPitchPerSemitone + std::clamp(p.fine, -50, 50) * kPitchPerSemitone / 100;

  // Attack 0 is at once, 127 is 4 s; the decay's time constant 5 ms to 6 s. Cubic: fine at the short end.
  const float attack = Unit(p.modAttack, 127);
  const float attackSeconds = 4.0f * attack * attack * attack;
  p.attackPerFrame = attackSeconds <= 0.0f ? 1.0f : (std::min)(1.0f, 1.0f / (attackSeconds * kSampleRate));
  const float decay = Unit(p.modDecay, 127);
  const float decaySeconds = 0.005f + 6.0f * decay * decay * decay;
  p.decayPerControl = std::exp(-static_cast<float>(kControlFrames) / (decaySeconds * kSampleRate));
  // The envelope moves TIMBRE and COLOR over their whole range, the pitch by up to three octaves
  // (squared: a semitone is still within reach).
  p.envTimbre = Unit(p.modTimbre, 63) * static_cast<float>(kMaxParameter);
  p.envColor = Unit(p.modColor, 63) * static_cast<float>(kMaxParameter);
  const float pitch = Unit(p.modPitch, 63);
  p.envPitch = pitch * std::fabs(pitch) * 36.0f * static_cast<float>(kPitchPerSemitone);

  // 0.05 Hz to 25 Hz; it swings TIMBRE and COLOR by up to half their range each way, the pitch by up
  // to an octave (squared: a vibrato of a few cents at the low end).
  const float hertz = 0.05f * std::exp2(Unit(p.lfoRate, 127) * 9.0f);
  p.lfoPerFrame = static_cast<std::uint32_t>(hertz / kSampleRate * 4294967296.0f);
  p.lfoToTimbre = Unit(p.lfoTimbre, 127) * 0.5f * static_cast<float>(kMaxParameter);
  p.lfoToColor = Unit(p.lfoColor, 127) * 0.5f * static_cast<float>(kMaxParameter);
  const float vibrato = Unit(p.lfoPitch, 127);
  p.lfoToPitch = vibrato * vibrato * 12.0f * static_cast<float>(kPitchPerSemitone);

  p.modulated = p.modTimbre != 0 || p.modColor != 0 || p.modPitch != 0 || p.lfoTimbre != 0 || p.lfoColor != 0 || p.lfoPitch != 0;
}

struct Voice {
  ::braids::MacroOscillator oscillator;
  float envelope = 0.0f;
  bool rising = false;
  std::uint32_t lfoPhase = 0;
};

// -1..1, a triangle starting at 0 on its way up.
float Triangle(std::uint32_t phase)
{
  const std::int32_t ramp = static_cast<std::int32_t>((phase + 0x40000000u) >> 16);  // 0..65535
  const std::int32_t folded = ramp < 32768 ? ramp : 65535 - ramp;                    // 0..32767
  return static_cast<float>(folded - 16384) * (1.0f / 16384.0f);
}

// The string and bore models sound sharp: their loops are longer than the delay the pitch asks for (the
// loop filters, the read-out), which shortens the period. On the module, at 96000 Hz, a bowed middle C
// is 9 cents sharp; at 44100 Hz the same samples are 20 cents, and twice that for every octave up. So
// the pitch asked for is that of a period as much longer. Measured against the pitch asked for, over
// the keyboard and the controls: the bowed string loses 1.95 samples, the reed 1.0, the flute 0.86 (its
// COLOR also overblows it, which is the model). The plucked string is a table of 1024, 512, ... samples
// read at the note's speed, and each pass of its damping moves the wave half a table sample: half a
// sample of the table's length, less often as TIMBRE lets the string ring (RenderPlucked).
std::int32_t DelayTuning(const Params& p, std::int32_t pitch)
{
  constexpr float kOctave = 12.0f * static_cast<float>(kPitchPerSemitone);
  const float cyclesPerSample = (440.0f / kSampleRate) * std::exp2((static_cast<float>(pitch) - 69.0f * static_cast<float>(kPitchPerSemitone)) / kOctave);
  float lost = 0.0f;  // of the period
  switch (p.shape)
  {
    case ::braids::MACRO_OSC_SHAPE_PLUCKED:
    {
      // The table is halved until the note reads at most two of its samples every other frame.
      float size = 1024.0f;
      for (float step = cyclesPerSample * 1024.0f; step > 1.0f && size > 2.0f; step *= 0.5f) size *= 0.5f;
      // How often a sample is averaged, of 65536.
      const std::int32_t averaged = p.timbreBase < 16384 ? 65536 : (std::min<std::int32_t>)(65536, 131072 - (p.timbreBase >> 3) * 31);
      lost = 0.5f * static_cast<float>(averaged) * (1.0f / 65536.0f) / size;
      break;
    }
    case ::braids::MACRO_OSC_SHAPE_BOWED: lost = 1.95f * cyclesPerSample; break;
    case ::braids::MACRO_OSC_SHAPE_BLOWN: lost = 1.0f * cyclesPerSample; break;
    case ::braids::MACRO_OSC_SHAPE_FLUTED: lost = 0.86f * cyclesPerSample; break;
    default: return 0;
  }
  return -static_cast<std::int32_t>(kOctave * std::log2(1.0f + lost));
}

// `frames` frames (kChunkFrames at most) of `oscillator` into `out`. Braids' loops count down in twos
// for half of its models: an odd count would never reach zero, so an odd tail (no AMY block has one)
// renders one frame more than it gives.
void RenderChunk(::braids::MacroOscillator& oscillator, std::int16_t* out, std::size_t frames)
{
  if ((frames & 1u) == 0)
  {
    oscillator.Render(kNoSync, out, frames);
    return;
  }
  std::int16_t scratch[kChunkFrames];
  oscillator.Render(kNoSync, scratch, frames + 1);
  std::memcpy(out, scratch, frames * sizeof(std::int16_t));
}

// The module's oscillator leaves most of its members as they come (its RAM starts at zero): all zero,
// then its own Init.
void Reset(::braids::MacroOscillator& oscillator)
{
  std::memset(static_cast<void*>(&oscillator), 0, sizeof(oscillator));
  oscillator.Init();
}

}  // namespace

class Track {
public:
  Track()
  {
    for (Voice& voice : voices) Reset(voice.oscillator);
    Derive(params);
  }
  ~Track() { ReleaseDelayLines(); }
  Track(const Track&) = delete;
  Track& operator=(const Track&) = delete;

  // The delay lines for every voice, or none: a chord with silent notes in it would be worse.
  bool TakeDelayLines()
  {
    if (delayLines[0] != nullptr) return true;
    if (!memory::SlowRoom(kVoiceCount * sizeof(::braids::DelayLines), kDelayLinesSlowReserveBytes)) return false;
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice)
    {
      delayLines[voice] = new (memory::slow) ::braids::DelayLines;
      if (delayLines[voice] == nullptr)
      {
        ReleaseDelayLines();
        return false;
      }
    }
    return true;
  }

  void ReleaseDelayLines()
  {
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice)
    {
      voices[voice].oscillator.set_delay_lines(nullptr);
      delete delayLines[voice];
      delayLines[voice] = nullptr;
    }
  }

  // After a change of model: the delay lines it needs, cleared (the comb filter starts on whatever is
  // there), or none.
  void FitDelayLines()
  {
    outOfMemory = false;
    if (!::braids::MacroOscillator::NeedsDelayLines(params.shape))
    {
      ReleaseDelayLines();
      return;
    }
    if (!TakeDelayLines())
    {
      outOfMemory = true;
      std::printf("braids: no memory for the %s model's delay lines (%u PSRAM free)\n", Model(params.model).name,
                  static_cast<unsigned>(memory::SlowFree()));
      return;
    }
    for (std::size_t voice = 0; voice < kVoiceCount; ++voice)
    {
      std::memset(static_cast<void*>(delayLines[voice]), 0, sizeof(::braids::DelayLines));
      voices[voice].oscillator.set_delay_lines(delayLines[voice]);
    }
  }

  Params params{};
  std::array<Voice, kVoiceCount> voices{};
  std::array<::braids::DelayLines*, kVoiceCount> delayLines{};
  bool outOfMemory = false;
};

void TrackDeleter::operator()(Track* track) const
{
  delete track;
}

TrackPtr CreateTrack()
{
  return TrackPtr(new (std::nothrow) Track());
}

void ResetParameters(Track& track)
{
  track.params = Params{};
  Derive(track.params);
  track.FitDelayLines();
}

bool SetParameter(Track& track, std::string_view id, int value)
{
  Params& p = track.params;
  const int model = p.model;
  if (id == "osc.model") p.model = std::clamp(value, 0, static_cast<int>(kModelCount) - 1);
  else if (id == "osc.timbre") p.timbre = value;
  else if (id == "osc.color") p.color = value;
  else if (id == "osc.tune") p.tune = value;
  else if (id == "osc.fine") p.fine = value;
  else if (id == "mod.attack") p.modAttack = value;
  else if (id == "mod.decay") p.modDecay = value;
  else if (id == "mod.timbre") p.modTimbre = value;
  else if (id == "mod.color") p.modColor = value;
  else if (id == "mod.pitch") p.modPitch = value;
  else if (id == "lfo.rate") p.lfoRate = value;
  else if (id == "lfo.timbre") p.lfoTimbre = value;
  else if (id == "lfo.color") p.lfoColor = value;
  else if (id == "lfo.pitch") p.lfoPitch = value;
  else return false;
  Derive(p);
  if (p.model != model) track.FitDelayLines();
  return true;
}

bool OutOfMemory(const Track& track)
{
  return track.outOfMemory;
}

void NoteOn(Track& track, std::size_t voiceIndex)
{
  if (voiceIndex >= track.voices.size()) return;
  Voice& voice = track.voices[voiceIndex];
  voice.oscillator.Strike();
  voice.rising = true;
  voice.envelope = 0.0f;
  voice.lfoPhase = 0;
}

std::int32_t Render(Track& track, std::size_t voiceIndex, std::int32_t pitch, float ampFrom, float ampTo, std::int32_t* out, std::size_t frames)
{
  if (voiceIndex >= track.voices.size() || frames == 0) return 0;
  Voice& voice = track.voices[voiceIndex];
  const Params& p = track.params;
  ::braids::MacroOscillator& oscillator = voice.oscillator;
  oscillator.set_shape(p.shape);

  constexpr float kToSample = kInt16ToSample * kVoiceGain;
  float gain = ampFrom * kToSample;
  // One division a block (the chip's FPU has none: a call into ROM).
  const float gainStep = (ampTo - ampFrom) * kToSample / static_cast<float>(frames);
  const std::int32_t tunedPitch = pitch + p.pitchOffset + DelayTuning(p, pitch + p.pitchOffset);
  std::int32_t peak = 0;
  std::int16_t chunk[kChunkFrames];
  for (std::size_t done = 0; done < frames;)
  {
    const std::size_t count = (std::min)(kChunkFrames, frames - done);
    std::int32_t timbre = p.timbreBase;
    std::int32_t color = p.colorBase;
    std::int32_t notePitch = tunedPitch;
    if (p.modulated)
    {
      if (voice.rising)
      {
        voice.envelope += p.attackPerFrame * static_cast<float>(count);
        if (voice.envelope >= 1.0f)
        {
          voice.envelope = 1.0f;
          voice.rising = false;
        }
      }
      else
      {
        for (std::size_t step = 0; step < count; step += kControlFrames) voice.envelope *= p.decayPerControl;
        if (voice.envelope < 1.0e-5f) voice.envelope = 0.0f;  // Never a denormal.
      }
      voice.lfoPhase += p.lfoPerFrame * static_cast<std::uint32_t>(count);
      const float envelope = voice.envelope;
      const float lfo = Triangle(voice.lfoPhase);
      timbre += static_cast<std::int32_t>(envelope * p.envTimbre + lfo * p.lfoToTimbre);
      color += static_cast<std::int32_t>(envelope * p.envColor + lfo * p.lfoToColor);
      notePitch += static_cast<std::int32_t>(envelope * p.envPitch + lfo * p.lfoToPitch);
    }
    oscillator.set_parameters(static_cast<std::int16_t>(std::clamp<std::int32_t>(timbre, 0, kMaxParameter)),
                              static_cast<std::int16_t>(std::clamp<std::int32_t>(color, 0, kMaxParameter)));
    oscillator.set_pitch(static_cast<std::int16_t>(std::clamp<std::int32_t>(notePitch, 0, kMaxPitch)));
    RenderChunk(oscillator, chunk, count);
    std::int32_t* destination = out + done;
    for (std::size_t frame = 0; frame < count; ++frame)
    {
      const std::int32_t sample = chunk[frame];
      peak = (std::max)(peak, sample < 0 ? -sample : sample);
      destination[frame] += static_cast<std::int32_t>(static_cast<float>(sample) * gain);
      gain += gainStep;
    }
    done += count;
  }
  const float loudest = (std::max)(std::fabs(ampFrom), std::fabs(ampTo)) * kToSample;
  return static_cast<std::int32_t>(static_cast<float>(peak) * loudest);
}

bool RenderPreview(int model, int timbre, int color, std::int8_t* out, std::size_t count)
{
  const auto shape = static_cast<::braids::MacroOscillatorShape>(std::clamp(model, 0, static_cast<int>(kModelCount) - 1));
  std::unique_ptr<::braids::MacroOscillator> oscillator(new (std::nothrow) ::braids::MacroOscillator);
  if (!oscillator) return false;
  Reset(*oscillator);
  std::unique_ptr<::braids::DelayLines> delayLines;
  if (::braids::MacroOscillator::NeedsDelayLines(shape))
  {
    if (!memory::SlowRoom(sizeof(::braids::DelayLines), kDelayLinesSlowReserveBytes)) return false;
    delayLines.reset(new (memory::slow) ::braids::DelayLines);
    if (!delayLines) return false;
    std::memset(static_cast<void*>(delayLines.get()), 0, sizeof(::braids::DelayLines));
    oscillator->set_delay_lines(delayLines.get());
  }
  // A period is half the picture: 96 frames, so the picture is the same whatever the note played.
  constexpr std::size_t kPeriodFrames = 96;
  const float semitones = 69.0f + 12.0f * std::log2(kSampleRate / static_cast<float>(kPeriodFrames) / 440.0f);
  oscillator->set_shape(shape);
  oscillator->set_parameters(static_cast<std::int16_t>(std::clamp(timbre, 0, 127) * kMaxParameter / 127),
                             static_cast<std::int16_t>(std::clamp(color, 0, 127) * kMaxParameter / 127));
  oscillator->set_pitch(static_cast<std::int16_t>(semitones * static_cast<float>(kPitchPerSemitone)));
  oscillator->Strike();
  std::int16_t chunk[kChunkFrames];
  // Past the first chunks: the macro controls glide from zero over the first, and a struck model shows
  // its body, not its click.
  for (int warm = 0; warm < 8; ++warm) RenderChunk(*oscillator, chunk, kChunkFrames);
  for (std::size_t done = 0; done < count;)
  {
    const std::size_t frames = (std::min)(kChunkFrames, count - done);
    RenderChunk(*oscillator, chunk, frames);
    for (std::size_t frame = 0; frame < frames; ++frame) out[done + frame] = static_cast<std::int8_t>(std::clamp(chunk[frame] >> 8, -127, 127));
    done += frames;
  }
  return true;
}

}  // namespace synth::braids

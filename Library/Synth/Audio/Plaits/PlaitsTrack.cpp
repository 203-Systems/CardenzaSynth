#include "Library/Synth/Audio/Plaits/PlaitsTrack.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <new>

#include "Library/Memory/Memory.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"

// Upstream's switch for building off the STM32 (stmlib/dsp/dsp.h has ARM instructions without it).
// The build sets it for the Plaits sources (cardenza_collect_plaits_cpp_sources), this file among them.
#if !defined(TEST)
#define TEST
#endif
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"

#if defined(_MSC_VER)
#define CARDENZA_PLAITS_NOINLINE __declspec(noinline)
#else
#define CARDENZA_PLAITS_NOINLINE __attribute__((noinline))
#endif

namespace synth::audio {

namespace {

// Plaits renders this many frames a call (more would change its envelopes' times, which count calls).
constexpr std::size_t kBlock = plaits::kBlockSize;
static_assert(kBlock <= plaits::kMaxBlockSize, "a block has to fit Plaits' buffers");
// What the module gives its models to share (plaits.cc: shared_buffer).
constexpr std::size_t kEngineRamBytes = 16384;
// A voice at full level in the runtime's mix units, as a sampler voice (kSamplerVoiceGain).
constexpr float kVoiceGain = 0.3f;

// "osc.model" (the order plaits::Voice registers its engines in).
constexpr int kModelCount = 24;
constexpr int kModelChiptune = 7;
constexpr int kModelString = 19;
constexpr int kModelModal = 20;
constexpr int kModelBassDrum = 21;  // then the snare drum and the hi-hat

// A key counts as down for at least this many blocks. Plaits delays TRIG (plaits::kTriggerDelay
// calls of Render) and reads LEVEL, the accent of the models with an envelope of their own, when
// the delayed edge arrives: a note let go before that would strike at no level at all (a sequenced
// note shorter than a render call has its note-off applied with its note-on).
constexpr std::uint8_t kMinGateBlocks = static_cast<std::uint8_t>(plaits::kTriggerDelay + 1);
// A voice let go ends once its output stayed this low (of 32767, -75 dB) for this many blocks (70 ms).
constexpr int kSilentPeak = 6;
constexpr std::uint16_t kSilentBlocks = 256;
// A damped voice ends below this.
constexpr float kAmpFloor = 0.0003f;
// Nothing rings for ever: a voice still sounding this long after its key went up (a 6-operator
// patch that never falls silent, a string at its longest) fades out. 12 seconds.
constexpr std::uint32_t kMaxReleaseBlocks = 12u * kSampleRate / static_cast<std::uint32_t>(kBlock);
// Output pinned at full scale for this long (half a second) is not sound but a model gone wrong
// (a NaN in its state): the voice is set up again.
constexpr int kStuckLevel = 30000;
constexpr std::uint16_t kStuckBlocks = 2048;

// PSRAM left for patterns, samples and the plain allocations that spill there; on a board without
// PSRAM, internal RAM left for everything else.
constexpr std::size_t kSlowReserveBytes = 96 * 1024;
constexpr std::size_t kFastFloorBytes = 48 * 1024;

// A voice's memory, zeroed as the module's is at power-on (its voice and buffer are static data).
// Out of line: zeroing a block right before an object is constructed in it is a store the compiler
// may drop when it sees both (GCC's -flifetime-dse).
CARDENZA_PLAITS_NOINLINE void* AllocateZeroed(std::size_t bytes)
{
  void* block = memory::AllocateSlow(bytes);
  if (block != nullptr) std::memset(block, 0, bytes);
  return block;
}

}  // namespace

struct PlaitsTrack::Voice {
  bool active = false;    // renders: held or ringing out
  bool gate = false;      // its key is down
  bool fading = false;    // fades out over the next Render call and stops
  bool lastHigh = false;  // TRIG as the last block sent it
  std::uint8_t note = 0;
  std::uint8_t lowBlocks = 0;      // blocks of TRIG low before the next strike (a voice played again)
  std::uint8_t minGateBlocks = 0;  // blocks the key still counts as down
  std::uint16_t silentBlocks = 0;
  std::uint16_t stuckBlocks = 0;
  std::uint32_t releasedBlocks = 0;
  std::uint32_t serial = 0;  // when its key went down, or up: the lowest is the oldest
  int loadedModel = -1;      // the model its last block rendered (-1: none, or to be set up again)
  int callPeak = 0;          // the loudest sample of the last Render call (of 32767)
  float level = 0.0f;        // from the note's velocity: LEVEL in GATE mode, the voice's gain in PING mode
  float levelNow = 0.0f;     // LEVEL as the last block sent it (rising by ATTACK)
  float amp = 1.0f;          // falls once the key is up, for models with no gate of their own
  float gainApplied = 1.0f;  // the gain the last block ended on
  std::size_t pendingRead = kBlock;  // frames of `pending` already given out
  float pending[kBlock] = {};
  plaits::Voice dsp;
  alignas(void*) char ram[kEngineRamBytes];
};

PlaitsTrack::PlaitsTrack()
{
  // Plaits computes its oscillators' frequencies for the rate its codec really runs at.
  pitchCorrection_ = 12.0f * std::log2(plaits::kCorrectedSampleRate / static_cast<float>(kSampleRate));
  UpdateDerived();
}

PlaitsTrack::~PlaitsTrack()
{
  for (std::size_t index = 0; index < voiceCount_; ++index)
  {
    Voice* voice = voices_[index];
    voice->~Voice();
    memory::FreeSlow(voice);
  }
}

std::unique_ptr<PlaitsTrack> PlaitsTrack::Create()
{
  std::unique_ptr<PlaitsTrack> track(new (std::nothrow) PlaitsTrack());
  if (!track) return nullptr;
  for (std::size_t index = 0; index < kMaxVoices; ++index)
  {
    if (!memory::SlowRoom(sizeof(Voice), kSlowReserveBytes, kFastFloorBytes)) break;
    void* block = AllocateZeroed(sizeof(Voice));
    if (block == nullptr) break;
    Voice* voice = new (block) Voice;
    track->voices_[track->voiceCount_++] = voice;
    track->SetUpModels(*voice);
  }
  return track;
}

// Every model set up, as the module does at power-on: they share the voice's buffer (zeroed).
void PlaitsTrack::SetUpModels(Voice& voice)
{
  stmlib::BufferAllocator allocator(voice.ram, sizeof(voice.ram));
  voice.dsp.Init(&allocator);
  voice.lastHigh = false;
  voice.loadedModel = -1;
}

// A voice gone wrong, from the start again.
void PlaitsTrack::Reinitialize(Voice& voice)
{
  std::memset(voice.ram, 0, sizeof(voice.ram));
  SetUpModels(voice);
}

void PlaitsTrack::UpdateDerived()
{
  const float scale = kVoiceGain * level_ * (1.0f / 32768.0f);
  outGain_ = (1.0f - aux_) * scale;
  auxGain_ = aux_ * scale;
  // These carry their own envelope and Plaits leaves its low-pass gate out for them: nothing of
  // theirs follows the key. The chiptune voices do not even end by themselves (their envelope is
  // TIMBRE ENV, off at 0), so a key let go always damps them.
  const bool ownEnvelope = model_ == kModelChiptune || model_ == kModelString || model_ == kModelModal || model_ >= kModelBassDrum;
  releaseDamps_ = model_ == kModelChiptune || (!ping_ && ownEnvelope);
  // As fast as the low-pass gate closes at this DECAY (voice.cc: short_decay), per block.
  releaseKeep_ = 1.0f - 0.05f * std::exp2(-8.0f * decay_);
  // ATTACK: up to two seconds, squared for a finer short end; 0 is at once.
  const float attackSeconds = 2.0f * attack_ * attack_;
  const float blockSeconds = static_cast<float>(kBlock) / static_cast<float>(kSampleRate);
  attackStep_ = attackSeconds > blockSeconds ? blockSeconds / attackSeconds : 1.0f;
}

void PlaitsTrack::SetParameter(std::string_view id, int value)
{
  const float unit = std::clamp(static_cast<float>(value) * (1.0f / 127.0f), 0.0f, 1.0f);
  const float bipolar = std::clamp(static_cast<float>(value) * (1.0f / 63.0f), -1.0f, 1.0f);
  if (id == "osc.model") model_ = std::clamp(value, 0, kModelCount - 1);
  else if (id == "osc.harmonics") harmonics_ = unit;
  else if (id == "osc.timbre") timbre_ = unit;
  else if (id == "osc.morph") morph_ = unit;
  else if (id == "osc.aux") aux_ = unit;
  else if (id == "osc.tune") tune_ = static_cast<float>(std::clamp(value, -24, 24));
  else if (id == "lpg.mode") ping_ = value != 0;
  else if (id == "lpg.attack") attack_ = unit;
  else if (id == "lpg.decay") decay_ = unit;
  else if (id == "lpg.colour") colour_ = unit;
  else if (id == "mod.frequency") frequencyEnv_ = bipolar;
  else if (id == "mod.timbre") timbreEnv_ = bipolar;
  else if (id == "mod.morph") morphEnv_ = bipolar;
  else if (id == "amp.level") level_ = unit;
  else return;
  UpdateDerived();
}

PlaitsTrack::Taken PlaitsTrack::NoteOn(std::uint8_t note, float velocity)
{
  if (voiceCount_ == 0) return Taken::NoVoice;
  Voice* chosen = nullptr;
  Taken taken = Taken::Free;
  // The same key again plays on its own voice, sounding or ringing out.
  for (std::size_t index = 0; index < voiceCount_ && chosen == nullptr; ++index)
    if (voices_[index]->active && voices_[index]->note == note) chosen = voices_[index];
  for (std::size_t index = 0; index < voiceCount_ && chosen == nullptr; ++index)
    if (!voices_[index]->active) chosen = voices_[index];
  if (chosen == nullptr)
  {
    // No free voice: one on its way out, then the one let go longest; a held note only when every
    // voice holds one, the oldest.
    const auto rank = [](const Voice& voice) { return voice.fading ? 0 : (voice.gate ? 2 : 1); };
    for (std::size_t index = 0; index < voiceCount_; ++index)
    {
      Voice* voice = voices_[index];
      if (chosen == nullptr || rank(*voice) < rank(*chosen) || (rank(*voice) == rank(*chosen) && voice->serial < chosen->serial))
        chosen = voice;
    }
    taken = rank(*chosen) == 2 ? Taken::Held : Taken::Released;
  }

  Voice& voice = *chosen;
  const bool wasActive = voice.active;
  // TRIG has to fall before it can strike again.
  voice.lowBlocks = voice.lastHigh ? 1 : 0;
  voice.active = true;
  voice.gate = true;
  voice.fading = false;
  voice.note = note;
  voice.minGateBlocks = kMinGateBlocks;
  voice.level = 0.2f + 0.8f * std::clamp(velocity, 0.0f, 1.0f);
  voice.amp = 1.0f;
  voice.silentBlocks = 0;
  voice.stuckBlocks = 0;
  voice.releasedBlocks = 0;
  voice.serial = ++serial_;
  if (!wasActive)
  {
    voice.gainApplied = ping_ ? voice.level : 1.0f;
    voice.levelNow = 0.0f;
    voice.pendingRead = kBlock;
    voice.callPeak = 0;
  }
  return taken;
}

void PlaitsTrack::NoteOff(std::uint8_t note)
{
  for (std::size_t index = 0; index < voiceCount_; ++index)
  {
    Voice& voice = *voices_[index];
    if (!voice.active || !voice.gate || voice.note != note) continue;
    voice.gate = false;
    voice.serial = ++serial_;
  }
}

void PlaitsTrack::AllNotesOff()
{
  for (std::size_t index = 0; index < voiceCount_; ++index)
  {
    Voice& voice = *voices_[index];
    if (!voice.active || !voice.gate) continue;
    voice.gate = false;
    voice.serial = ++serial_;
  }
}

std::size_t PlaitsTrack::SoundingVoices() const
{
  std::size_t count = 0;
  for (std::size_t index = 0; index < voiceCount_; ++index)
    if (voices_[index]->active) ++count;
  return count;
}

std::size_t PlaitsTrack::FadingVoices() const
{
  std::size_t count = 0;
  for (std::size_t index = 0; index < voiceCount_; ++index)
    if (voices_[index]->active && voices_[index]->fading) ++count;
  return count;
}

bool PlaitsTrack::HoldsNotes() const
{
  for (std::size_t index = 0; index < voiceCount_; ++index)
    if (voices_[index]->active && voices_[index]->gate) return true;
  return false;
}

void PlaitsTrack::AddHeldNotes(std::array<std::uint32_t, 4>& notes) const
{
  for (std::size_t index = 0; index < voiceCount_; ++index)
  {
    const Voice& voice = *voices_[index];
    if (voice.active && voice.gate && voice.note < 128) notes[voice.note / 32] |= 1u << (voice.note % 32);
  }
}

PlaitsTrack::Faded PlaitsTrack::FadeVoice()
{
  Voice* victim = nullptr;
  for (std::size_t index = 0; index < voiceCount_; ++index)
  {
    Voice* voice = voices_[index];
    if (!voice->active || voice->fading) continue;
    bool better = false;
    if (victim == nullptr) better = true;
    else if (voice->gate != victim->gate) better = !voice->gate;                                  // a let-go one first
    else if (!voice->gate) better = voice->serial < victim->serial;                               // the one let go longest
    else if (voice->callPeak != victim->callPeak) better = voice->callPeak < victim->callPeak;    // the quietest held one
    else better = voice->serial < victim->serial;                                                 // the oldest
    if (better) victim = voice;
  }
  if (victim == nullptr) return Faded::None;
  victim->fading = true;
  return victim->gate ? Faded::Held : Faded::Tail;
}

void PlaitsTrack::EndVoice(Voice& voice, bool resetModel)
{
  voice.active = false;
  voice.gate = false;
  voice.fading = false;
  voice.minGateBlocks = 0;
  voice.pendingRead = kBlock;
  // Stopped while its model still rang (a string, a drum): the model starts clean next time
  // (plaits::Voice resets it at its next block) instead of ringing on under the next note. Its
  // low-pass gate is not reset with it (upstream keeps that to itself): a voice stopped with the
  // gate open starts its next note from there, a millisecond of the new note before its strike.
  if (resetModel)
  {
    voice.dsp.ReloadUserData();
    voice.loadedModel = -1;
  }
}

// One block of the voice into its `pending`, and what that block means for the voice: still
// sounding, silent for long enough to end, or to be ended.
void PlaitsTrack::RenderBlock(Voice& voice)
{
  plaits::Patch patch;
  patch.note = static_cast<float>(voice.note) + tune_ + pitchCorrection_;
  patch.harmonics = harmonics_;
  patch.timbre = timbre_;
  patch.morph = morph_;
  patch.frequency_modulation_amount = frequencyEnv_;
  patch.timbre_modulation_amount = timbreEnv_;
  patch.morph_modulation_amount = morphEnv_;
  patch.engine = model_;
  patch.decay = decay_;
  patch.lpg_colour = colour_;

  bool high = false;
  if (voice.lowBlocks > 0)
  {
    --voice.lowBlocks;
  }
  else if (voice.gate || voice.minGateBlocks > 0)
  {
    high = true;
    if (voice.minGateBlocks > 0) --voice.minGateBlocks;
  }
  voice.lastHigh = high;

  // The keyboard on the module's jacks: TRIG, and in GATE mode LEVEL; the CV inputs stay unpatched,
  // so FM, TIMBRE and MORPH take the internal envelope by the amounts in the patch.
  plaits::Modulations modulations{};
  modulations.trigger = high ? 1.0f : 0.0f;
  modulations.trigger_patched = true;
  // LEVEL rises by ATTACK and falls at once: the release is the low-pass gate's own (DECAY).
  const float wanted = high ? voice.level : 0.0f;
  voice.levelNow = wanted > voice.levelNow ? (std::min)(wanted, voice.levelNow + attackStep_ * voice.level) : wanted;
  modulations.level = voice.levelNow;
  modulations.level_patched = !ping_;

  // plaits::Voice sets a model up in the block that first renders it (tables unpacked, delay lines
  // cleared): that block is not what a block costs.
  if (voice.loadedModel != model_)
  {
    voice.loadedModel = model_;
    modelSetUp_ = true;
  }
  plaits::Voice::Frame frames[kBlock];
  voice.dsp.Render(patch, modulations, frames, kBlock);

  const bool released = !voice.gate && voice.minGateBlocks == 0;
  if (released && releaseDamps_) voice.amp *= releaseKeep_;
  // In PING mode nothing in Plaits follows the velocity, so the voice's gain does.
  const float target = voice.amp * (ping_ ? voice.level : 1.0f);
  float gain = voice.gainApplied;
  const float gainStep = (target - gain) * (1.0f / static_cast<float>(kBlock));
  const bool outHeard = outGain_ != 0.0f;
  const bool auxHeard = auxGain_ != 0.0f;
  int peak = 0;
  int outLowest = 32767, outHighest = -32768;
  int auxLowest = 32767, auxHighest = -32768;
  for (std::size_t frame = 0; frame < kBlock; ++frame)
  {
    const int out = frames[frame].out;
    const int aux = frames[frame].aux;
    gain += gainStep;
    voice.pending[frame] = (static_cast<float>(out) * outGain_ + static_cast<float>(aux) * auxGain_) * gain;
    if (outHeard) peak = (std::max)(peak, std::abs(out));
    if (auxHeard) peak = (std::max)(peak, std::abs(aux));
    outLowest = (std::min)(outLowest, out);
    outHighest = (std::max)(outHighest, out);
    auxLowest = (std::min)(auxLowest, aux);
    auxHighest = (std::max)(auxHighest, aux);
  }
  voice.gainApplied = target;
  voice.callPeak = (std::max)(voice.callPeak, peak);

  // Either output held at one end of its range for the whole block.
  const bool pinned = outHighest <= -kStuckLevel || outLowest >= kStuckLevel || auxHighest <= -kStuckLevel || auxLowest >= kStuckLevel;
  voice.stuckBlocks = pinned ? static_cast<std::uint16_t>(voice.stuckBlocks + 1) : std::uint16_t{0};
  if (voice.stuckBlocks >= kStuckBlocks)
  {
    Reinitialize(voice);
    EndVoice(voice, false);
    return;
  }

  if (!released)
  {
    voice.silentBlocks = 0;
    voice.releasedBlocks = 0;
    return;
  }
  ++voice.releasedBlocks;
  voice.silentBlocks = peak <= kSilentPeak ? static_cast<std::uint16_t>(voice.silentBlocks + 1) : std::uint16_t{0};
  if (voice.silentBlocks >= kSilentBlocks) EndVoice(voice, false);
  else if (releaseDamps_ && voice.amp < kAmpFloor) EndVoice(voice, true);
  else if (voice.releasedBlocks >= kMaxReleaseBlocks) voice.fading = true;
}

void PlaitsTrack::Render(float* out, std::size_t frames)
{
  modelSetUp_ = false;
  if (frames == 0) return;
  for (std::size_t index = 0; index < voiceCount_; ++index)
  {
    Voice& voice = *voices_[index];
    if (!voice.active) continue;
    const bool fading = voice.fading;
    const float fadeStep = fading ? 1.0f / static_cast<float>(frames) : 0.0f;
    float fade = 1.0f;
    voice.callPeak = 0;
    std::size_t done = 0;
    while (done < frames)
    {
      if (voice.pendingRead >= kBlock)
      {
        RenderBlock(voice);
        // Ended with this block: it was silent, or as good as.
        if (!voice.active) break;
        voice.pendingRead = 0;
      }
      const std::size_t count = (std::min)(kBlock - voice.pendingRead, frames - done);
      const float* source = voice.pending + voice.pendingRead;
      float* target = out + done;
      if (fading)
      {
        for (std::size_t frame = 0; frame < count; ++frame)
        {
          target[frame] += source[frame] * fade;
          fade = (std::max)(0.0f, fade - fadeStep);
        }
      }
      else
      {
        for (std::size_t frame = 0; frame < count; ++frame) target[frame] += source[frame];
      }
      voice.pendingRead += count;
      done += count;
    }
    if (fading && voice.active) EndVoice(voice, true);
  }
}

}  // namespace synth::audio

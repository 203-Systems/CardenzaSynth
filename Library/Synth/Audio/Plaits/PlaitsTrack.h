#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace synth::audio {

// One track of the Plaits engine: up to four voices of Emilie Gillet's Plaits macro oscillator
// (Library/third_party/eurorack, unchanged), all on the model the track's settings choose, played
// from note events. The audio runtime owns one per Plaits track and calls it from the render thread
// only (under its render lock): nothing here locks.
//
// A voice is upstream's plaits::Voice as the module runs it, with the keyboard patched in: a note is
// the pitch, its key the TRIG input (held high while the key is down), and in GATE mode also the
// LEVEL input, so the low-pass gate stays open while the key is held (ATTACK is how fast LEVEL
// rises, DECAY the release). In
// PING mode LEVEL is left unpatched and the gate is struck at the note's start, as on the module
// with only TRIG patched. Models that carry their own envelope (strings, modal, drums, chiptune)
// ring out by themselves; in GATE mode letting the key go damps them, DECAY again the time.
//
// Plaits renders 12 frames at a time at 48 kHz; here it runs at the runtime's 44.1 kHz with the
// pitch corrected, in its own blocks of 12, whatever number of frames Render is asked for.
//
// Memory: about 28 KB a voice (plaits::Voice with every model's state, and the 16 KB the models
// share for delay lines and tables), in PSRAM: the playing model touches a few hundred bytes of it per
// block, and the delay lines in order. Allocated when the track is created, given back with it. A
// track that got fewer voices than kMaxVoices (memory short) plays with those; with none it is silent.
class PlaitsTrack {
public:
  static constexpr std::size_t kMaxVoices = 4;

  // Null when not even the track itself could be allocated.
  static std::unique_ptr<PlaitsTrack> Create();
  ~PlaitsTrack();
  PlaitsTrack(const PlaitsTrack&) = delete;
  PlaitsTrack& operator=(const PlaitsTrack&) = delete;

  std::size_t VoiceCount() const { return voiceCount_; }

  // An engine parameter of PlaitsSynthEngine ("osc.*", "lpg.*", "mod.*", "amp.level"); others are
  // ignored. Takes effect from the next block of 12 frames, sounding voices included.
  void SetParameter(std::string_view id, int value);

  // What the new note's voice was doing: nothing, ringing out a note let go, or holding another
  // note (taken from it: every voice was held). NoVoice: the track has none.
  enum class Taken : std::uint8_t { Free, Released, Held, NoVoice };
  Taken NoteOn(std::uint8_t note, float velocity);
  void NoteOff(std::uint8_t note);
  void AllNotesOff();

  // Voices that render: held or ringing out. FadingVoices: those of them on their way out (FadeVoice),
  // gone after the next Render call.
  std::size_t SoundingVoices() const;
  std::size_t FadingVoices() const;
  bool Sounding() const { return SoundingVoices() != 0; }
  bool HoldsNotes() const;
  // One bit per MIDI note held (key down), added to `notes`.
  void AddHeldNotes(std::array<std::uint32_t, 4>& notes) const;

  // Adds the voices' sound to `out`, in the runtime's mix units, for `frames` frames.
  void Render(float* out, std::size_t frames);
  // Whether a voice set its model up in the last Render call (its first block on a model, or after
  // it was stopped while ringing): that call took longer than rendering does.
  bool ModelSetUpInLastRender() const { return modelSetUp_; }

  // The voice missed least fades out over the next Render call and stops: a let-go one first (the
  // one let go longest), then the quietest held one. What it was, None when nothing sounds.
  enum class Faded : std::uint8_t { None, Tail, Held };
  Faded FadeVoice();

private:
  struct Voice;

  PlaitsTrack();
  void UpdateDerived();
  void RenderBlock(Voice& voice);
  void EndVoice(Voice& voice, bool resetModel);
  void SetUpModels(Voice& voice);
  void Reinitialize(Voice& voice);

  std::array<Voice*, kMaxVoices> voices_{};
  std::size_t voiceCount_ = 0;
  std::uint32_t serial_ = 0;
  bool modelSetUp_ = false;

  // The settings, as plaits::Patch takes them.
  int model_ = 8;
  float harmonics_ = 0.5f;
  float timbre_ = 0.5f;
  float morph_ = 0.5f;
  float aux_ = 0.0f;
  float tune_ = 0.0f;
  bool ping_ = false;
  float attack_ = 0.0f;
  float decay_ = 0.5f;
  float colour_ = 0.5f;
  float frequencyEnv_ = 0.0f;
  float timbreEnv_ = 0.0f;
  float morphEnv_ = 0.0f;
  float level_ = 100.0f / 127.0f;

  // Worked out when a setting changes (UpdateDerived).
  float pitchCorrection_ = 0.0f;  // semitones: Plaits tuned for its own sample rate, played at ours
  float outGain_ = 0.0f;          // int16 OUT to mix units
  float auxGain_ = 0.0f;          // int16 AUX to mix units
  bool releaseDamps_ = false;     // letting a key go fades its voice out (the model has no gate of its own)
  float releaseKeep_ = 1.0f;      // what stays of a damped voice after each block of 12 frames
  float attackStep_ = 1.0f;       // how much of its level a GATE voice's LEVEL gains per block (1: at once)
};

}  // namespace synth::audio

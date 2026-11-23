#pragma once

// The voices of one SoundFont track: which voice a note gets, what a key going up ends, and the
// SoundFont's rules between notes (an exclusive class cuts the notes of its class short; the same key
// played again lets the earlier note go quickly). Render-thread only; nothing here allocates.

#include <array>
#include <cstddef>
#include <cstdint>

#include "Library/Synth/Audio/SoundFont/Sf2Program.h"
#include "Library/Synth/Audio/SoundFont/Sf2Voice.h"

namespace synth::audio::sf2 {

class Track {
public:
  static constexpr std::size_t kVoices = 8;
  static constexpr std::size_t kMaxLayers = 2;  // regions one key at one velocity sounds
  // How fast a note cut short goes: about 30 ms (docs/sf2-engine-design.md, decision 8).
  static constexpr unsigned kQuickFadeSteps = 21;
  // How fast a voice the render budget gives up goes: four control steps, the runtime's block.
  static constexpr unsigned kBudgetFadeSteps = 4;

  // Every voice stops at once: call it with the track silent (Silence, then wait for Busy()).
  void SetProgram(const Program* program);
  const Program* CurrentProgram() const { return program_; }
  // For the notes that start from now on.
  void SetTone(const Tone& tone) { tone_ = tone; }

  void NoteOn(unsigned key, unsigned velocity);
  void NoteOff(unsigned key);
  void AllNotesOff();
  // Every voice fades out in one control step.
  void Silence();

  // Adds the track's voices to `accumulator`.
  void Render(std::int32_t* accumulator, std::size_t frames);

  bool Busy() const;
  // Voices that render: held, ringing out or fading. FadingVoices: those of them on their way out,
  // gone within kQuickFadeSteps control steps at most.
  std::size_t SoundingVoices() const;
  std::size_t FadingVoices() const;
  bool Holds(unsigned key) const;
  bool HoldsNotes() const;
  // Sets the bit of every key it holds (one bit per MIDI note).
  void AddHeldNotes(std::array<std::uint32_t, 4>& notes) const;
  // Held notes that had to give their voice to a newer one.
  std::uint32_t StolenHeld() const { return stolenHeld_; }

  // For the render budget: one voice fades out over the next block (kBudgetFadeSteps) and stops, the
  // quietest let-go one before the quietest held one; a voice already fading is left alone. What it
  // was, None when no voice was left to fade.
  enum class Faded : std::uint8_t { None, Tail, Held };
  Faded FadeVoice();

private:
  Voice& VoiceForNewNote();

  Voice voices_[kVoices];
  const Program* program_ = nullptr;
  Tone tone_;
  std::uint32_t order_ = 0;
  std::uint32_t stolenHeld_ = 0;
};

}  // namespace synth::audio::sf2

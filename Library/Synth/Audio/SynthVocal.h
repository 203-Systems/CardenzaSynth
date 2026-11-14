#pragma once

#include <cstddef>
#include <cstdint>

// The Vocal engine's vocabulary, shared by the audio runtime, the engine, the sequencer's host and
// the UI.
//
// A vocal track sings pronunciations of its voice pack (Library/Synth/Core/Engines/Vocal): MI, KU, A.
// A word is one of them, named by its place in the pack's list (0 for the first).
//   - A note sings the word of its step. A step says its word with a word key: a note of the step
//     that names a word and sounds nothing. Word keys are the MIDI notes 0..2 of the track, and the
//     key's velocity says which word: word = note * 127 + velocity - 1 (so 381 words fit; a pack has
//     at most 256). The sequencer keeps a step's key beside its notes (the WORD lane), and a
//     computer may send the same over USB.
//   - A note that comes without a word key sings the track's own word: the engine's "voice.word"
//     parameter (SING > WORD), which is also what the pads sing when played by hand.
//   - A word key under a note that is held changes the word on that note at once.
//   - A note that starts while another is held joins it: MONO glides to it without a new word (a
//     slur), POLY adds a voice singing the same word (a chord).
namespace synth::vocal {

// Notes below this are word keys on a vocal track.
inline constexpr std::uint8_t kWordKeyCount = 3;
// Words one key note names, one for each velocity 1..127.
inline constexpr int kWordsPerKey = 127;
// Voices of a vocal track: the notes of a chord in POLY, and in MONO the tail of the last word under
// the next one.
inline constexpr std::size_t kVoiceCount = 4;

constexpr bool IsWordKey(int note) { return note >= 0 && note < kWordKeyCount; }
// The word a key names, -1 for a velocity that names none (0 is a note-off).
constexpr int WordOfKey(int note, int velocity)
{
  return IsWordKey(note) && velocity >= 1 && velocity <= kWordsPerKey ? note * kWordsPerKey + velocity - 1 : -1;
}
constexpr std::uint8_t KeyNoteOfWord(int word) { return static_cast<std::uint8_t>(word / kWordsPerKey); }
constexpr std::uint8_t KeyVelocityOfWord(int word) { return static_cast<std::uint8_t>(word % kWordsPerKey + 1); }

// Attack, release and glide: 0..127 as 0..1000 ms, fine at the short end (8 is 4 ms, 22 is 30 ms).
constexpr unsigned TimeMs(int value)
{
  const int v = value < 0 ? 0 : (value > 127 ? 127 : value);
  return static_cast<unsigned>((v * v * 1000 + 8064) / 16129);
}

// Vibrato: depth 0..127 as 0..1 semitone each way, rate 0..127 as 2..10 Hz, delay 0..127 as 0..1.27 s.
constexpr float VibratoSemitones(int value) { return static_cast<float>(value < 0 ? 0 : (value > 127 ? 127 : value)) * (1.0f / 127.0f); }
constexpr float VibratoHz(int value) { return 2.0f + static_cast<float>(value < 0 ? 0 : (value > 127 ? 127 : value)) * (8.0f / 127.0f); }
constexpr unsigned VibratoDelayMs(int value) { return static_cast<unsigned>(value < 0 ? 0 : (value > 127 ? 127 : value)) * 10u; }

// What the runtime tells the UI about a vocal track, as of the last rendered block.
struct Status {
  std::int16_t singing = -1;   // the word a held note sings now, -1 for none
  std::uint32_t onsets = 0;    // counts every word started: tells two notes on one word apart
};

}  // namespace synth::vocal

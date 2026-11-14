#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

// The Braids engine's voices: Mutable Instruments' macro oscillator (Library/third_party/braids, MIT,
// its tables made for 44100 Hz) with what the module left to its patch cables: an AD envelope and an LFO
// on TIMBRE, COLOR and pitch. One Track is one synth track's voices and its sound; the audio runtime
// plays each voice as an AMY oscillator (wave CUSTOM), so notes, the amplitude envelope, the voice
// budget, the track's gain and its effects are AMY's and the runtime's, as for every other engine.
//
// Everything here runs on the render threads, except RenderPreview (the UI's). The models' names are in
// BraidsModels.h (the same namespace; ::braids is the module's own code). The module's random
// generator (stmlib::Random, noise and grains) is one for both render cores and the preview: an update
// lost between them is noise among noise.
namespace synth::braids {

// A new note takes the voice of an older one when all are busy, a let-go one first (AMY).
inline constexpr std::size_t kVoiceCount = 4;
// Braids' pitch: 128 a semitone, MIDI note 60 (middle C) at 60 * 128.
inline constexpr std::int32_t kPitchPerSemitone = 128;

class Track;
struct TrackDeleter {
  void operator()(Track* track) const;
};
using TrackPtr = std::unique_ptr<Track, TrackDeleter>;

// A track's voices (about 3 KB, internal RAM: read and written per sample). Null without memory.
TrackPtr CreateTrack();

// The sound back to the engine's defaults.
void ResetParameters(Track& track);
// One of the oscillator's parameters by its engine id ("osc.*", "mod.*", "lfo.*"); false for any other
// (the amplitude envelope and the level are AMY's). A model with delay lines (COMB and the physical
// models, 16 KB a voice) takes them from PSRAM here and gives them back with the next model.
bool SetParameter(Track& track, std::string_view id, int value);
// The current model needs delay lines that could not be had: its voices are silent.
bool OutOfMemory(const Track& track);

// A voice's note starts: the oscillator is struck, the envelope and the LFO start over.
void NoteOn(Track& track, std::size_t voice);
// `frames` frames (an AMY block) of one voice, added to `out` in AMY's S8.23, at an amplitude moving
// from `ampFrom` to `ampTo` (AMY's, 1 = full level). `pitch` is the note's, in Braids' units. Returns
// the largest magnitude written (never less than it).
std::int32_t Render(Track& track, std::size_t voice, std::int32_t pitch, float ampFrom, float ampTo, std::int32_t* out, std::size_t frames);

// For the OSC page (UI thread): two periods of a model at these TIMBRE and COLOR (0..127), `count`
// samples of -127..127. An oscillator of its own for the length of the call (and 16 KB of PSRAM for a
// model with delay lines). False without memory.
bool RenderPreview(int model, int timbre, int color, std::int8_t* out, std::size_t count);

}  // namespace synth::braids

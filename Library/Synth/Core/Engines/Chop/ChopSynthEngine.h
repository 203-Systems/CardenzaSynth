#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// The chop: one sample cut into slices, a pad a slice. A note is a slice, never a pitch: every slice
// plays at the file's own speed, from its start to its end, whatever the key does after (a one-shot).
inline constexpr std::size_t kChopSliceCount = 16;
// How many slices are in play: eight (a column of the pads each, the four rows its velocities, as a
// drum kit) or sixteen (a 4 x 4 block of the pads, a key each). The other eight keep their points.
inline constexpr std::size_t kChopFewSlices = 8;
// Slice n (from 0) is this note + n.
inline constexpr std::uint8_t kChopFirstNote = 36;
// A position in the file: 0 is its first frame, kChopPositionMax its end. Ten thousandths of the file
// (a millisecond in a ten second break) and within 16 bits, as preset values are stored.
inline constexpr int kChopPositionMax = 10000;
// How many slices sound at once (VOICES). A track has four voices, like the sampler's: the fourth is
// where a slice that is cut fades out (a few milliseconds), so a cut never clicks.
inline constexpr int kChopMaxVoices = 3;
// Mute groups: a slice in a group cuts the others of its group still sounding (an open hat closed by
// the closed one). 0 is none.
inline constexpr int kChopMuteGroups = 8;

// The slices' start and end points are one row in this order: start 1, end 1, start 2, end 2 ... Each
// point stays between its neighbours: a point moved onto one carries it along, in the direction it
// moves, so slices never overlap and never swap, and the gaps between them stay where they are.
inline constexpr std::size_t kChopPointCount = kChopSliceCount * 2;
constexpr std::size_t ChopPointIndex(std::size_t slice, bool end) { return slice * 2 + (end ? 1 : 0); }
using ChopPoints = std::array<int, kChopPointCount>;
// The row after the point at `index` was set to `value` (clamped to the file).
ChopPoints ChopChainAfterMove(const ChopPoints& points, std::size_t index, int value);
// `slices` slices of equal length from `first` to `last`, each ending where the next starts; the slices
// past `slices` wait at `last`.
ChopPoints ChopEvenPoints(int first, int last, std::size_t slices);

// What a slice parameter sets. Its id is "slice.NN.<field>", NN from 01.
enum class ChopField : std::uint8_t { Start, End, Fade, Cutoff, Mute };
std::string ChopParameterId(std::size_t slice, ChopField field);
// Reads a slice parameter's id without allocating (the audio side reads every one it is sent).
bool ParseChopParameterId(std::string_view id, std::size_t& slice, ChopField& field);
// The same field named without its slice ("slice.start"): what the INSTR page's cells hold, for the
// slice the pads picked.
bool ParseChopFieldAlias(std::string_view id, ChopField& field);

constexpr int ChopSliceNote(std::size_t slice) { return kChopFirstNote + static_cast<int>(slice); }
// The slice a note plays, -1 for a note that is none.
constexpr int ChopNoteSlice(int note)
{
  return note >= kChopFirstNote && note < kChopFirstNote + static_cast<int>(kChopSliceCount) ? note - kChopFirstNote : -1;
}

inline constexpr const char* kChopSlicesId = "chop.slices";  // 0: eight slices, 1: sixteen
inline constexpr const char* kChopVoicesId = "chop.voices";
inline constexpr const char* kChopGainId = "sample.gain";

// FADE (0..127): how long a slice fades out before its end, from 1 ms to 2 s, at most the slice.
float ChopFadeSeconds(int fade);
// CUTOFF (0..127): the slice's lowpass; 127 is none.
inline constexpr int kChopCutoffOpen = 127;

class ChopSynthEngine final : public PagedSynthEngine {
public:
  ChopSynthEngine();

  // A slice point moves through the row of points (ChopChainAfterMove): its neighbours go along.
  bool SetParameterValue(const std::string& parameterId, int value) override;
  // A file's points are put back in order, whatever it holds.
  bool RestoreState(const SynthEngineState& state) override;

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  // The runtime starts a chop from its own defaults: every value goes to it, as the sampler's do.
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const override;

private:
  ChopPoints Points() const;
  // Writes the points that differ from the parameters (each one a parameter of its own to the runtime).
  void ApplyPoints(const ChopPoints& points);
};

}  // namespace synth

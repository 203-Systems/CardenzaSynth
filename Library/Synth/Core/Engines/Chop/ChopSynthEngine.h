#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// A chopped sample: one file, sixteen slices, each with its own start and end point. The pads play
// slices rather than pitches, so a note is a slice index and the playback rate never changes.
// Monophonic: a new slice cuts the one before it, the way a chopped break is played by hand.
inline constexpr std::size_t kChopSliceCount = 16;
// How many slices may sound at once when "chop.voices" says so: a chopped break played as drums needs
// two or three hits to ring together, and four is as many as the runtime keeps sample voices for.
inline constexpr int kChopMaxVoices = 4;

// Mute groups, the way a drum machine has them: a slice in a group cuts whatever else in that group is
// still sounding, so an open hat is closed by the closed one. Group 0 is off - it cuts nothing and is
// never cut - and every slice has a group of its own.
inline constexpr int kChopMuteGroups = 8;

// MIDI note of slice 1: the slices run up from here, one semitone apart.
inline constexpr std::uint8_t kChopFirstNote = 36;
// The same slices again, kChopSliceCount semitones up: a chop never plays these, and the SAMPLE page
// sends one to hear only a slice's last second while its end is being placed.
inline constexpr std::uint8_t kChopTailFirstNote =
    static_cast<std::uint8_t>(kChopFirstNote + kChopSliceCount);
// How many slices the pads show: sixteen (4x4) or eight (4x2). The file is cut the same way either
// time; the count only decides how many pads and how many slice parameters are in play.
inline constexpr std::size_t kChopMinSlices = 8;

// A position in the file: a two-thousandth of it a step, so 0.05% - fine enough to place a cut by ear
// at the page's 4x magnifier, which is where the last of the hunting happens. (It started at 0..127
// like the sampler's trim, 0.79% a step, and was 0..1000 in between.)
inline constexpr int kChopEndOfFile = 2000;
// The two scales projects and presets were written on before that.
inline constexpr int kChopPreviousEndOfFile = 1000;
inline constexpr int kChopOriginalEndOfFile = 127;
// Project files from this version on hold positions on the 0..kChopEndOfFile scale; anything older is
// widened when it is read.
inline constexpr int kChopScaleProjectVersion = 10;

// A slice is two points now: where it starts and where it stops. All of them - two per slice, in
// order - are one row of trolleys, so a point pushes whichever one it meets, of either kind, in
// either direction: push slice 2's start back and it pushes slice 1's end, then slice 1's start.
// Points may share a place (a slice with no length, or two slices meeting), never cross.
std::string ChopStartParameterId(std::size_t slice);  // slice is 0-based
std::string ChopEndParameterId(std::size_t slice);
std::string ChopStartLabel(std::size_t slice);
// The point at either end of a slice: `end` picks which of the two.
std::string ChopPointParameterId(std::size_t slice, bool end);
// The mute group a slice is in, 0 (off) to kChopMuteGroups.
std::string ChopMuteParameterId(std::size_t slice);
// A slice's own tone: its lowpass cutoff and how long its fade takes. Slices come from different
// places in the file, so one of them may want to be darker or stop shorter than its neighbours.
std::string ChopCutoffParameterId(std::size_t slice);
std::string ChopReleaseParameterId(std::size_t slice);

// The trim every slice lives inside, over the whole file: the SETTING page moves it, the SAMPLE page
// shows only what is inside it, and moving it over slice points pushes them along.
inline constexpr const char* kChopSampleStartId = "sample.start";
inline constexpr const char* kChopSampleEndId = "sample.end";
// Where slice `slice` starts, and where it ends by default, in a file cut into `count` equal slices.
int ChopDefaultStart(std::size_t slice, std::size_t count);
int ChopDefaultEnd(std::size_t slice, std::size_t count);
// Where a default chop splits the file: sixteen equal slices, each ending where the next starts.
std::pair<int, int> ChopDefaultSlice(std::size_t slice);

// The points as one row of trolleys, slice by slice: start, end, start, end ... They read left to
// right and never cross. A point moves freely until it touches a neighbour; after that the two travel
// together, and when the second touches a third the three do, and so on - in the direction it is
// pushed, so a point pushed back carries the ones in front of it with it. Two points may therefore
// share a place, which is what "pushed all the way" looks like rather than a point that refuses.
inline constexpr std::size_t kChopPointCount = kChopSliceCount * 2;
// Index of a slice's start or end in that row.
constexpr std::size_t ChopPointIndex(std::size_t slice, bool end) { return slice * 2 + (end ? 1 : 0); }

std::array<std::uint16_t, kChopPointCount> ChopChainAfterMove(
    const std::array<std::uint16_t, kChopPointCount>& current, std::size_t index, int value);

class ChopSynthEngine final : public PagedSynthEngine {
public:
  ChopSynthEngine();

  // A slice point is written through the chain, so moving one carries its neighbours.
  bool SetParameterValue(const std::string& parameterId, int value) override;
  bool RestoreState(const SynthEngineState& state) override;

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo&) const override { return true; }
  void OnPresetLoaded(const std::string& presetId) override;

private:
  std::size_t SliceCount() const;
  // A value that came from a file (a preset, a project) may be out of order: the chain is rebuilt as
  // the last step, so what the pads and the waveform read is always ascending.
  void NormalizeChain();
  // A project saved before the position scale was refined holds its points on the older one, and is
  // widened once on the way in. `version` is the file's, 0 for anything that did not come from one.
  void MigrateScale(int version);
  // A project from before slices had an end of their own: each one stopped where the next began, and
  // the last at the end of the file.
  void DeriveEnds();
  // Where the trim is, 0..kChopEndOfFile; `end` picks which side.
  int TrimPoint(bool end) const;
  // The trim moved: every slice point outside it is pushed onto it, the way a point pushed against
  // another one is.
  void ClampChainToTrim();
};

}  // namespace synth

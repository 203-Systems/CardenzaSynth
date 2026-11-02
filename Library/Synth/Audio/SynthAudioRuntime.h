#pragma once

#include "Library/Synth/Audio/SampleStream.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "Library/Synth/Audio/SynthFx.h"
#include "Library/Synth/Core/SynthTypes.h"

namespace synth::audio {

inline constexpr std::uint32_t kSampleRate = 44100;

// A published sample is immutable; replacing it only transfers shared ownership
// at the audio boundary, without allocating or copying PCM in the render loop.
// Short samples are kept in RAM (`frames`); longer ones play from the card (`stream`).
struct SampleBuffer {
  std::unique_ptr<std::int16_t[]> frames;
  std::size_t frameCount = 0;
  std::shared_ptr<SampleStream> stream;
};
using SharedSampleBuffer = std::shared_ptr<const SampleBuffer>;

void EnsureEngine(std::size_t engineIndex, SynthEngineKind kind);
void SetEnginePreset(std::size_t engineIndex, std::string_view presetId);
void SetEngineParameter(std::size_t engineIndex, std::string_view parameterId, int value);
void SetSamplerFrames(std::size_t engineIndex, SharedSampleBuffer frames, std::uint32_t sampleRate);
// Sound from outside the synth (a computer playing over USB), mixed in before the output stage at
// unity gain: interleaved stereo int16 frames, the number given back. Null for none.
using LineInput = std::size_t (*)(std::int16_t* stereoFrames, std::size_t frameCount);
void SetLineInput(LineInput input);
// Stops the voice that will be missed least, whole, as rendering does when it cannot keep up (RenderLoad).
bool ShedVoice();
// Voices stopped so far because the audio core could not keep up (0 in normal use).
std::uint32_t ShedVoiceCount();
// The notes a track holds, one bit per MIDI note, as of the last rendered block: played and not let
// go, tails not counted. A drum track holds nothing, its hits end by themselves.
using NoteSet = std::array<std::uint32_t, 4>;
NoteSet HeldNotes(std::size_t engineIndex);
// Voices AMY kept sounding although their track held no note (each one is a bug somewhere); they get
// released, so nothing rings on for ever.
std::uint32_t ReleasedOrphanVoiceCount();
void NoteOn(std::size_t engineIndex, std::uint8_t note, std::uint8_t velocity);
void NoteOff(std::size_t engineIndex, std::uint8_t note);
void AllNotesOff(std::size_t engineIndex);
void WarmUp();
std::size_t RenderFrames(std::int16_t* destination, std::size_t frameCount);

// Track mixer (mono output, so there is no pan). The state belongs to the track: it survives
// engine switches and preset loads. Setters are queued like every other call above and take
// effect from the next rendered block, ramping over about four AMY blocks (~23 ms) to avoid
// clicks; getters return the last requested value immediately. Out-of-range tracks are ignored
// (getters then report unity gain, unmuted, silent).
//
// Gain is linear and clamped to 0..1; 1.0 is the default and renders exactly as before the
// mixer existed. It scales held notes and new notes alike, and composes with each engine's own
// level parameters (Juno vca.level, Drums master.level, Sampler amp.level) instead of replacing
// them.
void SetTrackGain(std::size_t engineIndex, float gain);
float TrackGain(std::size_t engineIndex);
// Muting renders the track at zero gain and drops its new note-ons. Note-offs and all-notes-off
// still pass, so nothing hangs; unmuting restores the gain, and notes still held come back.
void SetTrackMuted(std::size_t engineIndex, bool muted);
bool TrackMuted(std::size_t engineIndex);
// Cheap 0..1 "this track is sounding" indicator for UI meters, not a level measurement: it is
// bumped by each note-on (velocity x effective gain), held at half that while keys are down, and
// decays with a ~300 ms time constant per rendered block. True per-track RMS is not available
// because AMY renders every AMY-based track (DX7, Juno, Drums) into one shared mixed buffer.
float TrackActivity(std::size_t engineIndex);
// Decaying (~300 ms) peak of the final mono output after the soft clipper, 0..1 of full scale.
float OutputPeak();

// Effects (see SynthFx.h). Track inserts and sends arrive as "fx.*" engine parameters through
// SetEngineParameter; they reset to their defaults when a track changes engine or loads a preset.
// The master bus settings belong to the project. Values are clamped to the parameter range and
// queued like every other call; the getter returns the last requested value.
void SetMasterFx(fx::MasterParam param, int value);
int MasterFx(fx::MasterParam param);
// Sequencer tempo for the synced delay time.
void SetTempo(float bpm);
// Decaying 0..1 level of a bus return (fx::kBusDelay, fx::kBusReverb), for UI meters.
float FxReturnLevel(std::size_t bus);
// False after the bus could not get its delay memory; the bus then stays silent.
bool FxBusAvailable(std::size_t bus);

}  // namespace synth::audio

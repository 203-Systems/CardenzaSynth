#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "Library/Synth/Audio/SynthAudioRuntime.h"

namespace synth {

// A track's connections to a computer on USB: project data, the same on every engine (the INSTR ROUTE
// page), kept when the track changes engine or preset. The defaults are what every track did before the
// page existed: notes in and out on the track's own channel, the computer's sound not taken, the track
// in what the computer records. The MIDI values count the page's choices.
struct TrackRouting {
  // MIDI from the computer that plays the track: kMidiInOff, kMidiInAuto (its own channel, track n on
  // channel n, and while it is the current track every channel no track listens to), kMidiInAll, or a
  // channel from kMidiInFirstChannel (channel 1).
  std::uint8_t midiIn = 1;
  // MIDI the track sends: kMidiOutOff, kMidiOutAuto (track n on channel n), or a channel from
  // kMidiOutFirstChannel.
  std::uint8_t midiOut = 1;
  // What the track takes of the computer's sound, through its fader and effects.
  audio::LineChannels audioIn = audio::LineChannels::Off;
  std::int8_t inputGainDb = 0;
  // In what the computer records from the device.
  bool audioOut = true;

  bool operator==(const TrackRouting& other) const
  {
    return midiIn == other.midiIn && midiOut == other.midiOut && audioIn == other.audioIn && inputGainDb == other.inputGainDb &&
           audioOut == other.audioOut;
  }
  bool operator!=(const TrackRouting& other) const { return !(*this == other); }
};

inline constexpr std::uint8_t kMidiInOff = 0;
inline constexpr std::uint8_t kMidiInAuto = 1;
inline constexpr std::uint8_t kMidiInAll = 2;
inline constexpr std::uint8_t kMidiInFirstChannel = 3;
inline constexpr std::uint8_t kMidiInChoices = kMidiInFirstChannel + 16;
inline constexpr std::uint8_t kMidiOutOff = 0;
inline constexpr std::uint8_t kMidiOutAuto = 1;
inline constexpr std::uint8_t kMidiOutFirstChannel = 2;
inline constexpr std::uint8_t kMidiOutChoices = kMidiOutFirstChannel + 16;
inline constexpr int kAudioInChoices = 4;  // audio::LineChannels
inline constexpr int kMinInputGainDb = -24;
inline constexpr int kMaxInputGainDb = 12;

inline bool ValidRouting(const TrackRouting& routing)
{
  return routing.midiIn < kMidiInChoices && routing.midiOut < kMidiOutChoices && static_cast<int>(routing.audioIn) < kAudioInChoices &&
         routing.inputGainDb >= kMinInputGainDb && routing.inputGainDb <= kMaxInputGainDb;
}

// The MIDI channel (0..15) the track sends on, -1 for none.
inline int MidiOutChannel(const TrackRouting& routing, std::size_t track)
{
  if (routing.midiOut == kMidiOutOff) return -1;
  if (routing.midiOut == kMidiOutAuto) return static_cast<int>(track & 0x0F);
  return routing.midiOut - kMidiOutFirstChannel;
}

// Whether the track listens to MIDI channel `channel` (0..15) by name: its own (AUTO), every one (ALL)
// or the one chosen. AUTO's catch-all for the current track is the app's to decide.
inline bool MidiInListens(const TrackRouting& routing, std::size_t track, int channel)
{
  if (routing.midiIn == kMidiInOff) return false;
  if (routing.midiIn == kMidiInAll) return true;
  if (routing.midiIn == kMidiInAuto) return channel == static_cast<int>(track);
  return channel == routing.midiIn - kMidiInFirstChannel;
}

inline float InputGain(const TrackRouting& routing)
{
  return std::pow(10.0f, static_cast<float>(routing.inputGainDb) / 20.0f);
}

}  // namespace synth

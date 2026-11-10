#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "Library/Synth/Core/PagedSynthEngine.h"

namespace synth {

// The empty instrument: a track without a sound of its own. What it plays goes out over USB MIDI (the
// track's routing, every engine has it) to a synth on the computer, whose sound can come back on the
// track (its audio input) through the track's effects, fader and sends. Its parameters are that synth's
// patch: a program change with its bank, and eight controllers. The app sends them (GrooveApp); none
// reaches the audio runtime, which renders nothing for this engine.
namespace empty {

inline constexpr int kOff = -1;  // a program, bank or controller value not sent
inline constexpr std::size_t kControllerCount = 8;
inline constexpr int kMaxControllerNumber = 119;  // 120..127 are channel mode messages
inline constexpr std::array<const char*, kControllerCount> kControllerValueIds{{
    "cc1.value", "cc2.value", "cc3.value", "cc4.value", "cc5.value", "cc6.value", "cc7.value", "cc8.value"}};
inline constexpr std::array<const char*, kControllerCount> kControllerNumberIds{{
    "cc1.number", "cc2.number", "cc3.number", "cc4.number", "cc5.number", "cc6.number", "cc7.number", "cc8.number"}};
// What most synths answer to: cutoff, resonance, attack, release, modulation, volume, pan, reverb.
inline constexpr std::array<std::uint8_t, kControllerCount> kDefaultControllerNumbers{{74, 71, 73, 72, 1, 7, 10, 91}};
inline constexpr const char* kProgramId = "prog.number";
inline constexpr const char* kBankMsbId = "prog.bank_msb";
inline constexpr const char* kBankLsbId = "prog.bank_lsb";

// The usual meaning of a controller number (General MIDI and common practice), or empty.
std::string_view ControllerName(int number);

}  // namespace empty

class EmptySynthEngine final : public PagedSynthEngine {
public:
  EmptySynthEngine();

protected:
  std::string FormatParameterValue(const SynthParameterInfo& parameter) const override;
  // Only the track effects reach the audio runtime: the rest goes out as MIDI from the app.
  bool ShouldSyncParameterToRuntime(const SynthParameterInfo& parameter) const override;
};

}  // namespace synth

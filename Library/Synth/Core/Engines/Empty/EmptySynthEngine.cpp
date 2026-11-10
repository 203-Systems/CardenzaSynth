#include "Library/Synth/Core/Engines/Empty/EmptySynthEngine.h"

#include <map>
#include <string>

#include "Library/Synth/Audio/SynthFx.h"

namespace synth {

namespace empty {

std::string_view ControllerName(int number)
{
  switch (number)
  {
    case 0: return "BANK";
    case 1: return "MOD";
    case 2: return "BREATH";
    case 4: return "FOOT";
    case 5: return "GLIDE";
    case 7: return "VOLUME";
    case 8: return "BALANCE";
    case 10: return "PAN";
    case 11: return "EXPR";
    case 32: return "BANK LSB";
    case 64: return "SUSTAIN";
    case 65: return "PORTA";
    case 66: return "SOSTENUTO";
    case 67: return "SOFT";
    case 68: return "LEGATO";
    case 71: return "RESO";
    case 72: return "RELEASE";
    case 73: return "ATTACK";
    case 74: return "CUTOFF";
    case 75: return "DECAY";
    case 76: return "VIB RATE";
    case 77: return "VIB DEPTH";
    case 78: return "VIB DELAY";
    case 84: return "PORTA CTL";
    case 91: return "REVERB";
    case 92: return "TREMOLO";
    case 93: return "CHORUS";
    case 94: return "DETUNE";
    case 95: return "PHASER";
    default: return {};
  }
}

}  // namespace empty

namespace {

// General MIDI programs, numbered from 0 here (1 on screen): a synth that follows General MIDI (the
// computer's own, or most sound modules) answers to them at once.
struct GmPreset {
  const char* id;
  const char* name;
  int program;
};
constexpr std::array<GmPreset, 8> kGmPresets{{
    {"empty.gm_piano", "GM Piano", 0},
    {"empty.gm_epiano", "GM E Piano", 4},
    {"empty.gm_organ", "GM Organ", 16},
    {"empty.gm_bass", "GM Bass", 33},
    {"empty.gm_strings", "GM Strings", 48},
    {"empty.gm_brass", "GM Brass", 61},
    {"empty.gm_lead", "GM Lead", 80},
    {"empty.gm_pad", "GM Pad", 88},
}};

}  // namespace

EmptySynthEngine::EmptySynthEngine()
    : PagedSynthEngine({SynthEngineKind::Empty, "empty", "Empty"})
{
  ReserveParameters(3 + 2 * empty::kControllerCount);
  RegisterParameter({empty::kProgramId, "Program", "MIDI", SynthParameterKind::Continuous, empty::kOff, 127, 1, empty::kOff, empty::kOff});
  RegisterParameter({empty::kBankMsbId, "Bank MSB", "MIDI", SynthParameterKind::Continuous, empty::kOff, 127, 1, empty::kOff, empty::kOff});
  RegisterParameter({empty::kBankLsbId, "Bank LSB", "MIDI", SynthParameterKind::Continuous, empty::kOff, 127, 1, empty::kOff, empty::kOff});
  static constexpr std::array<const char*, empty::kControllerCount> kValueLabels{{"CC A", "CC B", "CC C", "CC D", "CC E", "CC F", "CC G", "CC H"}};
  static constexpr std::array<const char*, empty::kControllerCount> kNumberLabels{{"CC A #", "CC B #", "CC C #", "CC D #", "CC E #", "CC F #", "CC G #", "CC H #"}};
  for (std::size_t index = 0; index < empty::kControllerCount; ++index)
    RegisterParameter({empty::kControllerValueIds[index], kValueLabels[index], "CC", SynthParameterKind::Continuous, empty::kOff, 127, 1, empty::kOff,
                       empty::kOff});
  for (std::size_t index = 0; index < empty::kControllerCount; ++index)
  {
    const int number = empty::kDefaultControllerNumbers[index];
    RegisterParameter({empty::kControllerNumberIds[index], kNumberLabels[index], "CC MAP", SynthParameterKind::Continuous, 0, empty::kMaxControllerNumber, 1,
                       number, number});
  }
  RegisterTrackFxParameters();
  // Presets name the patch on the other side; the values they leave out are their defaults.
  RegisterPreset("empty.init", "Init", {});
  for (const GmPreset& preset : kGmPresets) RegisterPreset(preset.id, preset.name, {{empty::kProgramId, preset.program}});
  (void)LoadPreset("empty.init");
}

std::string EmptySynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (fx::IsTrackParameter(parameter.id)) return PagedSynthEngine::FormatParameterValue(parameter);
  if (parameter.group == "CC MAP") return std::to_string(parameter.value);
  if (parameter.value == empty::kOff) return "OFF";
  // Programs count from 1 on screen, as synths and the General MIDI list number them.
  if (parameter.id == empty::kProgramId) return std::to_string(parameter.value + 1);
  return std::to_string(parameter.value);
}

bool EmptySynthEngine::ShouldSyncParameterToRuntime(const SynthParameterInfo& parameter) const
{
  return fx::IsTrackParameter(parameter.id);
}

}  // namespace synth

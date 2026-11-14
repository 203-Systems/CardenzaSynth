#include "Library/Synth/Core/Engines/Braids/BraidsFactoryPresets.h"

#include <algorithm>

namespace synth {
namespace {

// Model numbers: Library/Synth/Audio/Braids/BraidsModels.h.
struct Osc {
  int model;
  int timbre;
  int color;
  int tune = 0;
};
struct Amp {
  int attack = 0;
  int decay = 64;
  int sustain = 127;
  int release = 24;
  int level = 100;
};
struct Mod {
  int attack = 0;
  int decay = 48;
  int timbre = 0;
  int color = 0;
  int pitch = 0;
};
struct Lfo {
  int rate = 64;
  int timbre = 0;
  int color = 0;
  int pitch = 0;
};

constexpr BraidsFactoryPreset P(std::string_view id, std::string_view name, Osc osc, Amp amp = {}, Mod mod = {}, Lfo lfo = {})
{
  const auto v = [](int value) { return static_cast<std::int8_t>(value); };
  return {id, name,
          {{v(osc.model), v(osc.timbre), v(osc.color), v(osc.tune), v(0),
            v(amp.attack), v(amp.decay), v(amp.sustain), v(amp.release), v(amp.level),
            v(mod.attack), v(mod.decay), v(mod.timbre), v(mod.color), v(mod.pitch),
            v(lfo.rate), v(lfo.timbre), v(lfo.color), v(lfo.pitch)}}};
}

// The struck and plucked models end by themselves: their amplitude envelope stays open (sustain 127)
// and only the release cuts them. TIMBRE and COLOR of the three-oscillator models are intervals: 64 is
// unison, 62 / 66 a few cents either side, 84 a fifth up, 96 an octave up, 32 an octave down.
constexpr std::array<BraidsFactoryPreset, kBraidsFactoryPresetCount> kPresets{{
    P("braids.saw", "CS Saw", {0, 40, 64}),
    P("braids.morph", "Morph Lead", {1, 46, 96}, {0, 70, 104, 30}, {0, 52, 22, 0, 0}),
    P("braids.pwm", "PWM Pad", {2, 64, 104}, {58, 80, 112, 72}, {}, {52, 44, 0, 0}),
    P("braids.fold", "Fold Bass", {3, 48, 0, -12}, {0, 60, 96, 20}, {0, 44, 36, 0, 0}),
    P("braids.sub", "Sub Bass", {5, 34, 84, -12}, {0, 70, 104, 18}),
    P("braids.sync", "Sync Lead", {8, 58, 30}, {0, 64, 112, 30}, {0, 60, 40, 0, 0}),
    P("braids.unison", "Unison Saws", {9, 62, 66}, {4, 72, 112, 40}),
    P("braids.fifths", "Fifth Saws", {9, 84, 96}, {10, 76, 108, 48}),
    P("braids.swarm", "Saw Swarm", {14, 52, 18}, {24, 80, 116, 64}),
    P("braids.ring", "Ring Chime", {13, 84, 52}, {0, 84, 0, 60}),
    P("braids.comb", "Comb Sweep", {15, 60, 96}, {6, 70, 112, 44}, {}, {40, 56, 0, 0}),
    P("braids.toy", "Toy Box", {16, 84, 24}, {0, 60, 100, 20}),
    P("braids.zlpf", "Z Sweep", {17, 28, 64}, {0, 70, 96, 30}, {0, 56, 46, 0, 0}),
    P("braids.vowel", "Vowels", {22, 60, 52}, {12, 64, 120, 40}, {}, {34, 62, 0, 0}),
    P("braids.choir", "FOF Choir", {23, 44, 72}, {52, 80, 116, 72}, {}, {24, 30, 0, 6}),
    P("braids.organ", "Drawbars", {24, 34, 24}, {0, 64, 127, 20}),
    P("braids.fm", "FM Keys", {25, 30, 64}, {0, 78, 40, 42}, {0, 58, 34, 0, 0}),
    P("braids.metal", "FM Metal", {26, 70, 92}, {0, 72, 0, 52}, {0, 50, 30, 0, 0}),
    P("braids.pluck", "Plucked", {28, 64, 52}, {0, 127, 127, 60}),
    P("braids.bowed", "Bowed", {29, 60, 52}, {28, 64, 127, 44}, {}, {79, 0, 0, 12}),
    P("braids.reed", "Reed", {30, 70, 60}, {14, 64, 127, 30}),
    P("braids.flute", "Flute", {31, 62, 64}, {18, 64, 127, 34}, {}, {78, 0, 0, 9}),
    P("braids.bell", "Bell", {32, 84, 60}, {0, 127, 127, 92}),
    P("braids.drum", "Steel Drum", {33, 72, 60}, {0, 127, 127, 60}),
    P("braids.kick", "808 Kick", {34, 56, 44, -24}, {0, 127, 127, 40}),
    P("braids.snare", "808 Snare", {36, 64, 64}, {0, 127, 127, 40}),
    P("braids.cymbal", "Cymbal", {35, 84, 84}, {0, 54, 0, 34}),
    P("braids.wavetable", "Wave Scan", {37, 24, 40}, {5, 64, 127, 40}, {}, {36, 72, 0, 0}),
    P("braids.wavemap", "Wave Map", {38, 40, 60}, {0, 70, 110, 36}, {0, 72, 30, -20, 0}),
    P("braids.chord", "Wave Chord", {40, 40, 50}, {30, 80, 120, 60}),
    P("braids.wind", "Wind", {41, 60, 40}, {70, 64, 127, 72}, {}, {26, 0, 52, 0}),
    P("braids.cloud", "Grain Cloud", {44, 72, 40}, {60, 64, 127, 84}),
}};

}  // namespace

const std::array<BraidsFactoryPreset, kBraidsFactoryPresetCount>& BraidsFactoryPresets()
{
  return kPresets;
}

const BraidsFactoryPreset* FindBraidsFactoryPreset(std::string_view presetId)
{
  const auto found = std::find_if(kPresets.begin(), kPresets.end(), [&](const BraidsFactoryPreset& preset) { return preset.id == presetId; });
  return found == kPresets.end() ? nullptr : &*found;
}

}  // namespace synth

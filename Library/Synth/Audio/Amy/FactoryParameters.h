#pragma once

#include <string_view>

#include "Library/Synth/Audio/Amy/FactoryData.h"

namespace synth::factory {

// Offsets and oscillator order follow the vendored amy/fm.py and amy/juno.py.
inline int Dx7Offset(std::string_view id)
{
  if (id == "global.algorithm") return 134;
  if (id == "global.feedback") return 135;
  if (id == "global.lfo_speed") return 137;
  if (id == "global.lfo_pitch") return 139;
  if (id == "global.lfo_amp") return 140;
  if (id == "global.transpose") return 144;
  if (id.size() < 5 || id[0] != 'o' || id[1] != 'p' || id[2] < '1' || id[2] > '6' || id[3] != '.') return -1;
  const int base = (6 - (id[2] - '0')) * 21;
  const auto field = id.substr(4);
  if (field == "output") return base + 16;
  if (field == "coarse") return base + 18;
  if (field == "fine") return base + 19;
  if (field == "detune") return base + 20;
  if (field == "attack") return base;
  if (field == "decay") return base + 1;
  if (field == "sustain") return base + 6;
  if (field == "release") return base + 3;
  if (field == "amp_mod") return base + 14;
  return -1;
}

inline int Dx7Value(const std::uint8_t* data, std::string_view id)
{
  const int offset = Dx7Offset(id);
  if (offset < 0) return 0;
  const int value = data[offset];
  if (id == "global.algorithm") return value + 1;
  if (id == "global.transpose") return value - 24;
  if (id.size() > 4 && id.substr(4) == "detune") return value - 7;
  if (id.size() > 4 && id.substr(4) == "amp_mod") return value != 0;
  return value;
}

inline int JunoOffset(std::string_view id)
{
  constexpr std::string_view fields[] = {
    "lfo.rate", "lfo.delay", "dco.lfo", "dco.pwm", "dco.noise", "vcf.cutoff", "vcf.resonance", "vcf.env",
    "vcf.lfo", "vcf.keyboard", "vca.level", "env.attack", "env.decay", "env.sustain", "env.release", "dco.sub"
  };
  for (int i = 0; i < 16; ++i) if (fields[i] == id) return i;
  return -1;
}

inline int JunoValue(const std::uint8_t* data, std::string_view id)
{
  const int offset = JunoOffset(id);
  if (offset >= 0) return data[offset];
  if (id == "dco.saw") return (data[16] & 16) != 0;
  if (id == "dco.range") return (data[16] & 1) ? 0 : ((data[16] & 4) ? 2 : 1);
  return 0;
}

}  // namespace synth::factory

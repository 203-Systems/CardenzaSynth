#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Effects shared by the audio runtime, the engines and the UI.
//
// Every track has an insert chain and two sends. They are engine parameters ("fx.*", registered
// by every engine, stored in presets and projects) so the INSTR pages edit them like any other
// sound parameter:
//   fx.drive      0..127  soft saturation, 0 = off
//   fx.crush      0..127  bit depth and sample-rate reduction, 0 = off
//   fx.filter     0..127  DJ filter: 64 = off, below = low-pass, above = high-pass
//   fx.resonance  0..127  filter resonance
//   fx.chorus     0..3    OFF / I / II / I+II (Juno-style chorus insert)
//   fx.delay      0..127  send to the master delay
//   fx.reverb     0..127  send to the master reverb
// Signal order: engine -> drive -> crush -> filter -> chorus -> track fader -> sends.
//
// The master bus (delay, reverb, compressor, EQ, tape saturation) belongs to the project.
namespace synth::fx {

inline constexpr std::array<const char*, 7> kTrackParameterIds{{
    "fx.drive", "fx.crush", "fx.filter", "fx.resonance", "fx.chorus", "fx.delay", "fx.reverb"}};

inline constexpr int kFilterCenter = 64;
inline constexpr std::array<const char*, 4> kChorusModes{{"OFF", "I", "II", "I+II"}};

inline bool IsTrackParameter(std::string_view id)
{
  return id.size() > 3 && id.substr(0, 3) == "fx.";
}

inline int TrackParameterDefault(std::string_view id)
{
  if (id == "fx.filter") return kFilterCenter;
  if (id == "fx.resonance") return 20;
  return 0;
}

enum class MasterParam : std::uint8_t {
  DelayTime,      // index into kDelayDivisions, synced to the sequencer tempo
  DelayFeedback,  // 0..127
  DelayTone,      // 0..127, dark to bright (low-pass in the feedback loop)
  DelayLevel,     // 0..127 return level
  ReverbSize,     // 0..127 decay time
  ReverbDamp,     // 0..127 high-frequency damping
  ReverbLowCut,   // 0..127 low cut before the reverb
  ReverbLevel,    // 0..127 return level
  Compressor,     // 0..127 bus compressor amount, 0 = off
  EqLow,          // 0..127 low shelf, 64 = flat
  EqHigh,         // 0..127 high shelf, 64 = flat
  Tape,           // 0..127 master saturation, 0 = off
};
inline constexpr std::size_t kMasterParamCount = 12;

struct MasterParamInfo {
  const char* label;
  int minValue;
  int maxValue;
  int defaultValue;
};

inline constexpr std::array<const char*, 12> kDelayDivisions{{
    "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/2T", "1/4D", "1/2"}};
// Length of each division in sixteenth notes.
inline constexpr std::array<float, 12> kDelaySixteenths{{
    0.5f, 2.0f / 3.0f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 8.0f / 3.0f, 3.0f, 4.0f, 16.0f / 3.0f, 6.0f, 8.0f}};

inline constexpr std::array<MasterParamInfo, kMasterParamCount> kMasterParams{{
    {"TIME", 0, 11, 7},
    {"FDBK", 0, 127, 56},
    {"TONE", 0, 127, 72},
    {"LEVEL", 0, 127, 100},
    {"SIZE", 0, 127, 76},
    {"DAMP", 0, 127, 52},
    {"LOCUT", 0, 127, 36},
    {"LEVEL", 0, 127, 100},
    {"COMP", 0, 127, 0},
    {"LOW", 0, 127, 64},
    {"HIGH", 0, 127, 64},
    {"TAPE", 0, 127, 0},
}};

inline constexpr std::size_t kBusDelay = 0;
inline constexpr std::size_t kBusReverb = 1;
inline constexpr std::size_t kBusCount = 2;

}  // namespace synth::fx

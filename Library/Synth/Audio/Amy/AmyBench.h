#pragma once

// Test builds only (CARDENZA_AMY_BENCH: `make DEVICE=Cardenza stress AMY_BENCH=1`, never shipped): the
// sound sources AMY has and the firmware has no engine for yet, to hear on the board and to measure
// there (the stress test's `amy:` scenarios) before any of them becomes an engine.
//
// The sources take the place of the first Juno presets (A11 ...): a Juno track plays them, with its
// chords, arpeggiator and effects; the Juno engine's own parameters do nothing to them. The Drums
// engine gets AMY's Gamma9001 kits as presets. Their samples (3.7 MB) are linked into the app, which
// then no longer fits the Launcher's 4 MB slot: the build has its own partition table
// (Devices/Cardenza/partitions.bench.csv) and is flashed whole, over the Launcher.

#include <array>
#include <cstddef>
#include <cstdint>

namespace synth::bench {

struct Source {
  const char* name;   // the preset's name on screen, numbered so the browser keeps them together
  int romPatch;       // an AMY ROM patch (256: the piano), or -1 for `wire`
  int wavetable;      // which of AMY's wavetables `wire` names with its %d, or -1
  const char* wire;   // an AMY patch string, oscillators numbered within the voice
  int voices;
  int levelDb;        // evens it out with the factory presets (amy_bench_check prints it)
};

// A wavetable scanned by EG1 and a slow triangle (osc 1), through the 24 dB lowpass.
inline constexpr char kWavetableWire[] =
    "v1w4a1,,0,1f0.25,0,,,,,0A50,1,10000,0Z"
    "v0w19p%dd0.05,,,,0.85,0.08L1G4F600,0.5,,,3.5R1.2A8,1,300,0.8,350,0B0,1,2500,0.15,500,0Z";
// Seven saws over +/-14 cents on one oscillator (AMY's unison), through the 24 dB lowpass.
inline constexpr char kSupersawWire[] =
    "v0w2C7,0.004,-0.012,0.6G4F700,0.5,,,3.5R1A15,1,600,0.8,450,0B0,1,1800,0.3,450,0Z";
// A sine into AMY's wavefolder, the fold depth from the velocity and EG1.
inline constexpr char kFoldWire[] = "v0w0GF1GD1,,0.5,,2.5A4,1,700,0.5,350,0B0,1,900,0.1,350,0Z";
// Karplus-Strong strings: plucked near the bridge, and a longer one at a quarter of the string.
inline constexpr char kPluckWire[] = "v0w6b0.996d0.42A0,1,400,0Z";
inline constexpr char kHarpWire[] = "v0w6b0.999d0.25G1F3000,0.5R0.7A0,1,1500,0Z";
// Nine sine partials under one parent (BYO_PARTIALS), set like an organ's drawbars: 16', 5 1/3', 8',
// 4', 2 2/3', 2', 1 3/5', 1 1/3', 1'.
inline constexpr char kOrganWire[] =
    "v0w10p9A3,1,90,0Z"
    "v1w9f220a0.8A0,1,90,0Zv2w9f660a0.5A0,1,90,0Zv3w9f440a1A0,1,90,0Zv4w9f880a0.6A0,1,90,0Z"
    "v5w9f1320a0.3A0,1,90,0Zv6w9f1760a0.25A0,1,90,0Zv7w9f2200a0.15A0,1,90,0Zv8w9f2640a0.1A0,1,90,0Z"
    "v9w9f3520a0.3A0,1,90,0Z";

inline constexpr std::array<Source, 11> kSources{{
    {"01 WT 111", -1, 0, kWavetableWire, 6, 0},
    {"02 WT BRAIDS", -1, 1, kWavetableWire, 6, 4},
    {"03 WT PPG", -1, 2, kWavetableWire, 6, 4},
    {"04 WT SINE2SAW", -1, 3, kWavetableWire, 6, -2},
    {"05 WT VIRAL", -1, 4, kWavetableWire, 6, 3},
    {"06 SUPERSAW", -1, -1, kSupersawWire, 6, 2},
    {"07 FOLD", -1, -1, kFoldWire, 6, -1},
    {"08 KS PLUCK", -1, -1, kPluckWire, 6, 8},
    {"09 KS HARP", -1, -1, kHarpWire, 6, 9},
    {"10 PIANO", 256, -1, nullptr, 4, -8},
    {"11 ORGAN", -1, -1, kOrganWire, 6, -5},
}};

// The Juno preset a source replaces: juno.a11 for the first, juno.a12 ... (eight to a bank).
inline std::array<char, 9> SourcePresetId(std::size_t index)
{
  return {'j', 'u', 'n', 'o', '.', 'a', static_cast<char>('1' + (index / 8) % 8), static_cast<char>('1' + index % 8), '\0'};
}

// A pad of a kit: AMY's PCM preset, the note it plays at (its own pitch, or a tuned one) and its gain.
struct DrumPad {
  std::uint16_t preset;
  std::uint8_t note;
  float gain;
};
struct DrumKit {
  const char* id;
  const char* name;
  std::array<DrumPad, 8> pads;  // BD SD CH OH CP TM CB MA (kDrumNotes)
};

// What AMY's own kit patches (384-390) play for our pads' GM notes, with their gains. Presets below 256
// are the TR-808 bank built into a GAMMA9001 build; the others read the Gamma9001 blob.
inline constexpr std::array<DrumKit, 7> kDrumKits{{
    {"drums.808", "808 Kit", {{{2, 60, 3.25f}, {12, 45, 5.57f}, {9, 60, 12.74f}, {10, 60, 7.69f}, {3, 60, 5.30f}, {16, 59, 3.66f}, {8, 60, 5.21f}, {11, 62, 11.45f}}}},
    {"drums.909", "909 Kit", {{{256, 60, 5.90f}, {268, 60, 5.70f}, {260, 60, 9.03f}, {263, 60, 7.24f}, {258, 60, 9.22f}, {271, 60, 3.30f}, {275, 60, 4.22f}, {353, 60, 6.28f}}}},
    {"drums.linn", "LINN 9000 Kit", {{{273, 60, 6.79f}, {280, 60, 3.77f}, {277, 60, 8.35f}, {278, 60, 4.66f}, {3, 60, 5.30f}, {282, 58, 4.29f}, {275, 60, 4.22f}, {353, 60, 6.28f}}}},
    {"drums.mr12", "MR-12 Kit", {{{285, 60, 5.81f}, {286, 60, 10.45f}, {283, 60, 15.00f}, {284, 60, 8.56f}, {286, 63, 10.45f}, {285, 66, 4.11f}, {275, 60, 4.22f}, {353, 60, 6.28f}}}},
    {"drums.tokyo", "TOKYO Kit", {{{298, 60, 3.59f}, {289, 60, 15.00f}, {306, 60, 15.00f}, {301, 60, 15.00f}, {292, 60, 7.76f}, {305, 58, 15.00f}, {290, 60, 10.88f}, {353, 60, 6.28f}}}},
    {"drums.power", "POWER Kit", {{{312, 36, 8.74f}, {314, 38, 3.91f}, {318, 42, 8.92f}, {322, 46, 4.41f}, {315, 39, 6.76f}, {319, 43, 5.45f}, {330, 56, 6.82f}, {353, 60, 6.28f}}}},
    {"drums.perc", "PERC Kit", {{{350, 63, 7.37f}, {351, 60, 5.41f}, {352, 60, 7.29f}, {353, 60, 6.28f}, {364, 60, 4.88f}, {369, 58, 3.98f}, {275, 60, 4.22f}, {353, 60, 6.28f}}}},
}};

}  // namespace synth::bench

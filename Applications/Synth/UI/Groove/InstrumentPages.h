#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "Applications/Synth/UI/Groove/GrooveTypes.h"

namespace groove {

enum class SlotAction : std::uint8_t {
  None,
  Engine,
  Preset,
  SavePreset,
  SavePresetAs,
  SampleFile,
  SampleRecord,
  SamplePreview,
};

// A GRID slot on an INSTR page: an engine parameter id, a "keys.*" keyboard setting, or an action.
struct Slot {
  const char* id = nullptr;
  const char* label = nullptr;
  SlotAction action = SlotAction::None;

  constexpr bool Empty() const { return id == nullptr && action == SlotAction::None; }
  constexpr bool IsAction() const { return action != SlotAction::None; }
  constexpr bool IsParameter() const { return id != nullptr && action == SlotAction::None; }
};

enum class PageGraphic : std::uint8_t {
  None,
  Voice,
  Algorithm,
  OperatorLevels,    // DX7: the six operator outputs, carriers vs modulators
  OperatorEnvelope,  // DX7: one operator's rate / level envelope
  Adsr,
  Filter,
  Oscillator,        // Juno DCO scope
  Lfo,               // LFO shape, rate and delay
  Waveform,
  DrumPads,
  FxChain,           // track insert chain and sends
  SetupKeys,         // FN KEYS: the scale on a keyboard
  SetupChord,        // FN CHORD: the chord one key plays, on a keyboard
  SetupArp,          // FN ARP: the order the arpeggiator plays a chord in
  SetupProject,      // FN PROJECT: name, saved state, patterns and song
  SetupSystem,       // FN SYSTEM: storage, memory and audio status
};

struct GraphicArea {
  std::uint8_t row = 0;
  std::uint8_t column = 0;
  std::uint8_t rows = 0;
  std::uint8_t columns = 0;
};

struct InstrumentPage {
  const char* name = "";  // short description for the CTRL page map (7 characters fit)
  const char* tab = "";   // short page tab label
  std::array<Slot, kGridCount> slots{};
  PageGraphic graphic = PageGraphic::None;
  GraphicArea area{};
  std::uint8_t op = 0;  // DX7 operator 1..6 for operator pages
};

struct InstrumentPageList {
  const InstrumentPage* pages = nullptr;
  std::size_t count = 0;
};

// VOICE, the engine's parameter pages, then FX. At most 16 pages, so the CTRL page map fits the GRID.
InstrumentPageList InstrumentPagesFor(synth::SynthEngineKind kind);

// The FN panel: 0 KEYS, 1 CHORD, 2 ARP, 3 PROJECT, 4 SYSTEM (FN + 1..5 picks one).
inline constexpr std::size_t kSetupPageCount = 5;
const InstrumentPage& SetupPage(std::size_t page);

}  // namespace groove

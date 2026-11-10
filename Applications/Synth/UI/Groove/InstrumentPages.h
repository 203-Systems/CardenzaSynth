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
  Chord,             // INSTR CHORD: the chord one key plays, on a keyboard
  Arp,               // INSTR ARP: the order the track's arpeggiator plays a chord in
  Keys,              // SETUP KEYS: the scale on a keyboard
  System,            // SETUP SYSTEM: storage, memory and audio status
  Route,             // INSTR ROUTE: the track and the computer on USB, a lane per connection
  Controllers,       // EMPTY MIDI: the eight controllers' values, by number
  ControllerMap,     // EMPTY CC: which controller each one is, by name
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

// In signal order: VOICE (the preset), the track's MIDI effects CHORD and ARP (not on drums), the
// engine's parameter pages, FX, then ROUTE (USB). At most 16 pages, so the CTRL page map fits the GRID.
InstrumentPageList InstrumentPagesFor(synth::SynthEngineKind kind);

// The settings pages of the SETUP scene: 0 KEYS, 1 SYSTEM (scene pages kSetupKeys..).
inline constexpr std::size_t kSettingsPageCount = 2;
static_assert(kSetupKeys + kSettingsPageCount == kSetupPageCount);
const InstrumentPage& SettingsPage(std::size_t page);

}  // namespace groove

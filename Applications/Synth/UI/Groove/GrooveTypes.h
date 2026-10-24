#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Library/Synth/Core/SynthTypes.h"

namespace groove {

inline constexpr std::size_t kTrackCount = synth::kSynthTrackCount;
inline constexpr std::size_t kRows = synth::kKeyboardMatrixRows;
inline constexpr std::size_t kColumns = synth::kKeyboardMatrixColumns;
inline constexpr std::size_t kKeyCount = kRows * kColumns;
inline constexpr std::size_t kPadColumns = synth::kHomePerformanceColumnCount;
inline constexpr std::size_t kPadFirstColumn = synth::kHomePerformanceFirstColumn;
inline constexpr std::size_t kGridFirstColumn = synth::kSequencerStepFirstColumn;
inline constexpr std::size_t kGridColumns = synth::kSequencerStepColumnCount;
inline constexpr std::size_t kGridCount = kRows * kGridColumns;

// Three main screens: the instrument, the sequencer and the tape (song arrangement, mix and project).
enum class Scene : std::uint8_t {
  Instrument = 0,
  Sequence = 1,
  Tape = 2,
};
inline constexpr std::size_t kSceneCount = 3;

inline constexpr std::array<const char*, kSceneCount> kSceneNames{{"INSTR", "SEQ", "TAPE"}};

// TAPE sub-pages. PROJECT, FILES and SYSTEM stay consecutive (one renderer draws them).
inline constexpr std::uint8_t kTapeSong = 0;
inline constexpr std::uint8_t kTapeMix = 1;
inline constexpr std::uint8_t kTapeSends = 2;
inline constexpr std::uint8_t kTapeFx = 3;
inline constexpr std::uint8_t kTapeProject = 4;
inline constexpr std::uint8_t kTapeFiles = 5;
inline constexpr std::uint8_t kTapeSystem = 6;
inline constexpr std::size_t kTapePageCount = 7;

// TAPE SENDS rows: the per-track effect each GRID row edits (columns are the tracks).
inline constexpr std::array<const char*, 4> kSendIds{{"fx.filter", "fx.chorus", "fx.delay", "fx.reverb"}};
inline constexpr std::array<const char*, 4> kSendTags{{"FLT", "CHO", "DLY", "REV"}};

enum class KeyRole : std::uint8_t {
  None,
  Esc,
  Tab,
  Fn,
  Ctrl,
  Sub,
  Add,
  Enter,
  Play,
  Pad,
  Grid,
};

constexpr KeyRole RoleFor(std::size_t row, std::size_t column)
{
  if (row >= kRows || column >= kColumns) return KeyRole::None;
  if (column == 0)
  {
    constexpr std::array<KeyRole, kRows> kLeft{{KeyRole::Esc, KeyRole::Tab, KeyRole::Fn, KeyRole::Ctrl}};
    return kLeft[row];
  }
  if (column == kColumns - 1)
  {
    // Top to bottom: ADD (+), SUB (-), ENTER, PLAY.
    constexpr std::array<KeyRole, kRows> kRight{{KeyRole::Add, KeyRole::Sub, KeyRole::Enter, KeyRole::Play}};
    return kRight[row];
  }
  return column < kGridFirstColumn ? KeyRole::Pad : KeyRole::Grid;
}

constexpr std::size_t GridIndex(std::size_t row, std::size_t column)
{
  return row * kGridColumns + (column - kGridFirstColumn);
}

constexpr std::size_t KeyIndex(std::size_t row, std::size_t column)
{
  return row * kColumns + column;
}

// Key-cap characters used for text entry and Fn shortcuts. 0 = no character.
inline constexpr std::array<std::array<char, kColumns>, kRows> kLegend{{
    {{'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0}},
    {{0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '[', ']', '\\'}},
    {{0, 0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', '\'', 0}},
    {{0, 0, 0, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '/', ' '}},
}};

constexpr char LegendChar(std::size_t row, std::size_t column)
{
  return row < kRows && column < kColumns ? kLegend[row][column] : 0;
}

enum class KeyboardLayout : std::uint8_t {
  Piano = 0,
  Scale = 1,
  Chromatic = 2,
};

struct ScaleDefinition {
  const char* label;
  std::uint16_t mask;  // bit n = semitone n
};

inline constexpr std::array<ScaleDefinition, 8> kScales{{
    {"MAJOR", 0b101010110101},
    {"MINOR", 0b010110101101},
    {"DORIAN", 0b011010101101},
    {"MIXOLYD", 0b011010110101},
    {"MIN PENT", 0b010010101001},
    {"MAJ PENT", 0b001010010101},
    {"BLUES", 0b010011101001},
    {"HARM MIN", 0b100110101101},
}};

inline constexpr std::array<const char*, 12> kNoteNames{{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}};
inline constexpr std::array<const char*, 3> kLayoutNames{{"PIANO", "SCALE", "CHROMA"}};
inline constexpr std::array<std::uint8_t, 8> kDrumNotes{{36, 38, 42, 46, 39, 43, 56, 70}};
inline constexpr std::array<const char*, 8> kDrumNames{{"BD", "SD", "CH", "OH", "CP", "TM", "CB", "MA"}};

struct KeyboardSettings {
  int octave = 0;           // -3..+3
  int root = 0;             // 0..11
  std::size_t scale = 1;    // index into kScales
  KeyboardLayout layout = KeyboardLayout::Scale;
};

// Maps a pad (row, column) to a MIDI note for the given engine. Returns false for gaps (Piano layout).
bool PadNote(const KeyboardSettings& settings, synth::SynthEngineKind kind, std::size_t row, std::size_t column, int& note);

std::string_view EngineShortName(synth::SynthEngineKind kind);
// Three-letter engine code for narrow columns.
std::string_view EngineCode(synth::SynthEngineKind kind);
// Drum name for GM drum notes on the Drums engine, empty otherwise.
std::string_view DrumName(int note);
std::string NoteName(int note);

}  // namespace groove

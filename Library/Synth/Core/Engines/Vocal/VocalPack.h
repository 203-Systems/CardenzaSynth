// SPDX-License-Identifier: BSD-3-Clause
// A voice pack (.vocal): the waves, phonemes and pronunciations of one singer. The file format is
// the NullPerator tracker's (sources/Application/Instruments/VocalPack.*, ours) for packs converted
// from Famicaloid: the chip's own waves. That tracker plays them through a chip core; this firmware
// plays the same data with its own oscillator (VocalVoice.h).
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace synth::vocal {

// The file: a 128-byte header, then waves, phonemes, zones, pronunciations and segments, all little
// endian and of fixed record sizes.
//   wave           64 six-bit samples of one period, stored as (sample - 32) * 4; then its volume,
//                  and its pitch modulation: depth, rate, delay and a 32-step table
//   phoneme        its zones (a wave for each stretch of the keyboard), how its pitch follows the
//                  note, its gain and attack
//   pronunciation  an id, a name and its segments: phonemes in turn, each for a time, the last one
//                  for as long as the note is held (MI: 40 ms of M, then I)
inline constexpr std::size_t kHeaderBytes = 128;
inline constexpr std::size_t kMaxPackBytes = 128 * 1024;
// The number the header's format field holds.
inline constexpr unsigned kFormat = 2;
inline constexpr unsigned kWaveSamples = 64;
inline constexpr unsigned kWaveBytes = 104;
inline constexpr unsigned kPhonemeBytes = 48;
inline constexpr unsigned kZoneBytes = 8;
inline constexpr unsigned kEntryBytes = 24;
inline constexpr unsigned kSegmentBytes = 8;
inline constexpr unsigned kModulationSteps = 32;
// A pack has at most this many pronunciations.
inline constexpr unsigned kMaxEntries = 256;
// A pronunciation's name: up to 15 printable ASCII characters.
inline constexpr unsigned kEntryNameBytes = 16;

inline unsigned U16(const std::uint8_t* p) { return p[0] | static_cast<unsigned>(p[1]) << 8; }
inline std::uint32_t U32(const std::uint8_t* p) { return U16(p) | static_cast<std::uint32_t>(U16(p + 2)) << 16; }
inline int Signed(std::uint8_t value) { return value < 128 ? value : static_cast<int>(value) - 256; }

// Record fields a voice reads (offsets into a wave, a phoneme, a segment).
namespace field {
inline constexpr unsigned kWaveVolume = 64;         // 0..32
inline constexpr unsigned kWaveModDepth = 65;       // 0..63
inline constexpr unsigned kWaveModRate = 66;        // u16, 0..4095
inline constexpr unsigned kWaveModDelayMs = 68;     // u16
inline constexpr unsigned kWaveModTable = 72;       // 32 steps, each 0..7
inline constexpr unsigned kPhonemePitchMode = 4;    // 0 follows the note, 1 note + offset, 2 a note of its own
inline constexpr unsigned kPhonemePitchValue = 5;
inline constexpr unsigned kPhonemeGain = 6;         // 0..255
inline constexpr unsigned kPhonemeKind = 11;        // always 1: a phoneme of the chip's waves
inline constexpr unsigned kPhonemeAttackMs = 12;    // u16
inline constexpr unsigned kSegmentPhoneme = 0;      // u16
inline constexpr unsigned kSegmentDurationMs = 2;   // u16, 0 = held until the note is let go
inline constexpr unsigned kSegmentGain = 4;
}  // namespace field

// Immutable once loaded: the render thread reads it while the UI lists its names.
class Pack {
public:
  // Everything a voice later reads unchecked: the format, sizes, checksum, every reference and range.
  static bool Validate(const std::uint8_t* bytes, std::size_t size);
  // A copy of a valid file's bytes (in PSRAM where there is one). Null for an invalid file or without
  // memory.
  static std::shared_ptr<const Pack> Load(const std::uint8_t* bytes, std::size_t size);
  // The pack in the firmware (VocalBuiltinPack.h): always there, never copied.
  static std::shared_ptr<const Pack> Builtin();
  static std::uint32_t Checksum(const std::uint8_t* bytes, std::size_t size);

  ~Pack();
  Pack(const Pack&) = delete;
  Pack& operator=(const Pack&) = delete;

  const std::uint8_t* Bytes() const { return data_; }
  std::size_t Size() const { return size_; }
  const char* Name() const { return reinterpret_cast<const char*>(data_ + 32); }
  const char* Author() const { return reinterpret_cast<const char*>(data_ + 64); }
  const char* Id() const { return reinterpret_cast<const char*>(data_ + 96); }
  unsigned WaveCount() const { return U16(data_ + 20); }
  unsigned PhonemeCount() const { return U16(data_ + 22); }
  unsigned ZoneCount() const { return U16(data_ + 24); }
  unsigned EntryCount() const { return U16(data_ + 26); }
  unsigned SegmentCount() const { return U16(data_ + 28); }

  // Pronunciations, in the pack's order (sorted by id): the firmware names one by its place in this
  // list, its word. A record: id, first segment, segment count, name.
  const std::uint8_t* Entry(unsigned index) const { return entries_ + index * kEntryBytes; }
  unsigned EntryId(unsigned index) const { return U16(Entry(index)); }
  const char* EntryNameAt(unsigned index) const { return reinterpret_cast<const char*>(Entry(index) + 8); }
  // Index of the pronunciation with this id, -1 when the pack has none.
  int Find(unsigned id) const;
  // The word a note sings when nothing else is chosen: the pack's default pronunciation.
  unsigned DefaultEntry() const;
  // Whether it starts with a short phoneme before the one it holds (MI: M, then I).
  bool EntryHasOnset(unsigned index) const { return U16(Entry(index) + 4) > 1; }
  const std::uint8_t* Segment(unsigned index) const { return segments_ + index * kSegmentBytes; }
  const std::uint8_t* Phoneme(unsigned index) const { return phonemes_ + index * kPhonemeBytes; }
  // The wave record of a phoneme's zone nearest to `note`: a zone that covers the note before one that
  // does not, then the nearest root.
  const std::uint8_t* SelectWave(unsigned phoneme, int note) const;

private:
  Pack(const std::uint8_t* data, std::size_t size, std::uint8_t* owned);

  const std::uint8_t* data_ = nullptr;
  std::size_t size_ = 0;
  std::uint8_t* owned_ = nullptr;  // from memory::AllocateSlowArray, or null for the built-in pack
  const std::uint8_t* phonemes_ = nullptr;
  const std::uint8_t* zones_ = nullptr;
  const std::uint8_t* entries_ = nullptr;
  const std::uint8_t* segments_ = nullptr;
};

using PackRef = std::shared_ptr<const Pack>;

}  // namespace synth::vocal

#pragma once

// The SoundFont 2 file format as this engine reads it (SoundFont 2.04 specification; the design and
// what is left out are in docs/sf2-engine-design.md).

#include <cstddef>
#include <cstdint>

namespace synth::audio::sf2 {

// Generator operators (specification, 8.1.2).
enum Gen : std::uint16_t {
  kStartAddrsOffset = 0,
  kEndAddrsOffset = 1,
  kStartloopAddrsOffset = 2,
  kEndloopAddrsOffset = 3,
  kStartAddrsCoarseOffset = 4,
  kModLfoToPitch = 5,
  kVibLfoToPitch = 6,
  kModEnvToPitch = 7,
  kInitialFilterFc = 8,
  kInitialFilterQ = 9,
  kModLfoToFilterFc = 10,
  kModEnvToFilterFc = 11,
  kEndAddrsCoarseOffset = 12,
  kModLfoToVolume = 13,
  kChorusEffectsSend = 15,
  kReverbEffectsSend = 16,
  kPan = 17,
  kDelayModLfo = 21,
  kFreqModLfo = 22,
  kDelayVibLfo = 23,
  kFreqVibLfo = 24,
  kDelayModEnv = 25,
  kAttackModEnv = 26,
  kHoldModEnv = 27,
  kDecayModEnv = 28,
  kSustainModEnv = 29,
  kReleaseModEnv = 30,
  kKeynumToModEnvHold = 31,
  kKeynumToModEnvDecay = 32,
  kDelayVolEnv = 33,
  kAttackVolEnv = 34,
  kHoldVolEnv = 35,
  kDecayVolEnv = 36,
  kSustainVolEnv = 37,
  kReleaseVolEnv = 38,
  kKeynumToVolEnvHold = 39,
  kKeynumToVolEnvDecay = 40,
  kInstrument = 41,
  kKeyRange = 43,
  kVelRange = 44,
  kStartloopAddrsCoarseOffset = 45,
  kKeynum = 46,
  kVelocity = 47,
  kInitialAttenuation = 48,
  kEndloopAddrsCoarseOffset = 50,
  kCoarseTune = 51,
  kFineTune = 52,
  kSampleId = 53,
  kSampleModes = 54,
  kScaleTuning = 56,
  kExclusiveClass = 57,
  kOverridingRootKey = 58,
  kGenCount = 61,
};

// Record sizes of the nine preset-data tables, in file order.
enum Table : std::uint8_t { kPhdr, kPbag, kPmod, kPgen, kInst, kIbag, kImod, kIgen, kShdr, kTableCount };
inline constexpr std::uint8_t kRecordBytes[kTableCount] = {38, 4, 10, 4, 22, 4, 10, 4, 46};
inline constexpr char kTableTags[kTableCount][5] = {"phdr", "pbag", "pmod", "pgen", "inst", "ibag", "imod", "igen", "shdr"};

// Sample types (specification, 7.10).
inline constexpr std::uint16_t kSampleTypeRight = 2;
inline constexpr std::uint16_t kSampleTypeCompressed = 0x10;  // SF3: Ogg Vorbis, not read
inline constexpr std::uint16_t kSampleTypeRom = 0x8000;

// What a file may ask of this device. A file past one of these is refused, never cut short.
inline constexpr std::size_t kMaxPresets = 4096;
inline constexpr std::size_t kMaxRegions = 512;       // of one preset
inline constexpr std::size_t kMaxSampleSlots = 256;   // of one preset
inline constexpr std::size_t kMaxZoneGenerators = 128;
inline constexpr std::uint32_t kMinSampleRate = 400;
inline constexpr std::uint32_t kMaxSampleRate = 192000;

inline constexpr std::uint32_t kOutputRate = 44100;
inline constexpr std::size_t kControlFrames = 64;  // envelopes, LFOs and the filter move once in this many frames

enum class Status : std::uint8_t {
  Ok,
  ReadError,     // the card
  NotSoundFont,  // not a RIFF sfbk file
  Unsupported,   // a SoundFont this engine does not play: SF3, no 16-bit samples
  Corrupt,       // tables that contradict each other
  TooLarge,      // more presets, regions or samples than kMax...
  NotFound,      // no such preset
  TooBig,        // the preset's samples do not fit the memory it may take
  NoMemory,
  Empty,         // the preset has no region that can sound
};

}  // namespace synth::audio::sf2

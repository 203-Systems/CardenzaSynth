#pragma once

// A SoundFont file as it lies on the card: where its tables and its samples are, and its presets in
// bank and program order. Nothing else is kept in memory; a preset's regions are read from the file
// when it is chosen (Sf2Program). The file is somebody else's data: every count and index is checked.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "Library/Synth/Audio/SoundFont/Sf2Format.h"
#include "Library/Synth/Audio/SoundFont/Sf2Source.h"

namespace synth::audio::sf2 {

struct PresetEntry {
  std::uint16_t bank = 0;
  std::uint16_t program = 0;
  std::uint16_t header = 0;  // its record in the file's preset table
};

struct SampleHeader {
  std::uint32_t start = 0;  // frames from the start of the sample data
  std::uint32_t end = 0;    // one past the last frame
  std::uint32_t loopStart = 0;
  std::uint32_t loopEnd = 0;  // one past the last frame of the loop
  std::uint32_t rate = 0;
  std::uint8_t originalPitch = 60;
  std::int8_t pitchCorrection = 0;
  std::uint16_t link = 0;
  std::uint16_t type = 0;
};

// The generators of one zone (one "bag") of a preset or an instrument.
struct Zone {
  std::int16_t value[kGenCount] = {};
  std::uint64_t present = 0;  // one bit per generator the zone sets
  std::uint8_t keyLow = 0;
  std::uint8_t keyHigh = 127;
  std::uint8_t velLow = 0;
  std::uint8_t velHigh = 127;
  bool hasKeyRange = false;
  bool hasVelRange = false;
  bool hasTarget = false;  // it names an instrument (preset zone) or a sample (instrument zone)
  std::uint16_t target = 0;

  bool Has(unsigned generator) const { return ((present >> generator) & 1u) != 0; }
};

class File {
public:
  // Reads the chunk layout and the list of presets. The reader has to outlive the file.
  Status Open(Reader& reader);

  std::size_t PresetCount() const { return presetCount_; }
  const PresetEntry& Preset(std::size_t position) const { return presets_[position]; }
  // The position of bank / program in the list, -1 when the file has no such preset.
  int FindPreset(unsigned bank, unsigned program) const;
  // The preset's name, at most 20 characters, read from the file (lists keep no names in memory).
  bool PresetName(std::size_t position, char name[21]);

  // The tables, for Sf2Program.
  std::uint32_t RecordCount(Table table) const { return counts_[table]; }
  bool ReadRecord(Table table, std::uint32_t index, std::uint8_t* bytes);
  // The generators of bag `bag` of a bag table: the range between its index and the next bag's.
  Status BagGenerators(Table bagTable, std::uint32_t bag, std::uint32_t& first, std::uint32_t& count);
  // One zone's generators, up to and with the one that ends it (`terminal`: kInstrument or kSampleId).
  Status ReadZone(Table generatorTable, std::uint32_t first, std::uint32_t count, std::uint16_t terminal, Zone& zone);
  bool ReadSampleHeader(std::uint32_t index, SampleHeader& header);
  std::uint32_t SampleFrames() const { return sampleFrames_; }
  bool ReadSampleFrames(std::uint32_t firstFrame, std::int16_t* destination, std::uint32_t count);

private:
  Status ReadLayout();
  Status ReadPresets();

  Reader* reader_ = nullptr;
  std::uint64_t offsets_[kTableCount] = {};
  std::uint32_t counts_[kTableCount] = {};
  std::uint64_t sampleOffset_ = 0;
  std::uint32_t sampleFrames_ = 0;
  std::unique_ptr<PresetEntry[]> presets_;
  std::size_t presetCount_ = 0;
};

}  // namespace synth::audio::sf2

#include "Library/Synth/Audio/SoundFont/Sf2Program.h"

#include <algorithm>
#include <cstdint>

#include "Library/Memory/Memory.h"

namespace synth::audio::sf2 {

namespace {

// A generator's default and the range it is held in after the preset's value is added to the
// instrument's (specification, 8.1.3). `additive`: a preset zone may carry it.
struct GenRule {
  std::int16_t initial;
  std::int16_t low;
  std::int16_t high;
  bool additive;
};

constexpr GenRule kUnused{0, 0, 0, false};
constexpr GenRule kOffset{0, -32768, 32767, false};
constexpr GenRule kCents{0, -12000, 12000, true};
constexpr GenRule kDelay{-12000, -12000, 5000, true};
constexpr GenRule kTime{-12000, -12000, 8000, true};
constexpr GenRule kKeyScale{0, -1200, 1200, true};
constexpr GenRule kLfoFreq{0, -16000, 4500, true};

constexpr GenRule kRules[kGenCount] = {
    kOffset,                      // 0 startAddrsOffset
    kOffset,                      // 1 endAddrsOffset
    kOffset,                      // 2 startloopAddrsOffset
    kOffset,                      // 3 endloopAddrsOffset
    kOffset,                      // 4 startAddrsCoarseOffset
    kCents,                       // 5 modLfoToPitch
    kCents,                       // 6 vibLfoToPitch
    kCents,                       // 7 modEnvToPitch
    {13500, 1500, 13500, true},   // 8 initialFilterFc
    {0, 0, 960, true},            // 9 initialFilterQ
    kCents,                       // 10 modLfoToFilterFc
    kCents,                       // 11 modEnvToFilterFc
    kOffset,                      // 12 endAddrsCoarseOffset
    {0, -960, 960, true},         // 13 modLfoToVolume
    kUnused,                      // 14
    {0, 0, 1000, true},           // 15 chorusEffectsSend
    {0, 0, 1000, true},           // 16 reverbEffectsSend
    {0, -500, 500, true},         // 17 pan
    kUnused,                      // 18
    kUnused,                      // 19
    kUnused,                      // 20
    kDelay,                       // 21 delayModLFO
    kLfoFreq,                     // 22 freqModLFO
    kDelay,                       // 23 delayVibLFO
    kLfoFreq,                     // 24 freqVibLFO
    kDelay,                       // 25 delayModEnv
    kTime,                        // 26 attackModEnv
    kDelay,                       // 27 holdModEnv
    kTime,                        // 28 decayModEnv
    {0, 0, 1000, true},           // 29 sustainModEnv
    kTime,                        // 30 releaseModEnv
    kKeyScale,                    // 31 keynumToModEnvHold
    kKeyScale,                    // 32 keynumToModEnvDecay
    kDelay,                       // 33 delayVolEnv
    kTime,                        // 34 attackVolEnv
    kDelay,                       // 35 holdVolEnv
    kTime,                        // 36 decayVolEnv
    {0, 0, 1440, true},           // 37 sustainVolEnv
    kTime,                        // 38 releaseVolEnv
    kKeyScale,                    // 39 keynumToVolEnvHold
    kKeyScale,                    // 40 keynumToVolEnvDecay
    kUnused,                      // 41 instrument
    kUnused,                      // 42
    kUnused,                      // 43 keyRange
    kUnused,                      // 44 velRange
    kOffset,                      // 45 startloopAddrsCoarseOffset
    {-1, -1, 127, false},         // 46 keynum
    {-1, -1, 127, false},         // 47 velocity
    {0, 0, 1440, true},           // 48 initialAttenuation
    kUnused,                      // 49
    kOffset,                      // 50 endloopAddrsCoarseOffset
    {0, -120, 120, true},         // 51 coarseTune
    {0, -99, 99, true},           // 52 fineTune
    kUnused,                      // 53 sampleID
    {0, 0, 3, false},             // 54 sampleModes
    kUnused,                      // 55
    {100, 0, 1200, true},         // 56 scaleTuning
    {0, 0, 127, false},           // 57 exclusiveClass
    {-1, -1, 127, false},         // 58 overridingRootKey
    kUnused,                      // 59
    kUnused,                      // 60
};

// The zones a region is made of. A preset or an instrument may begin with a global zone, whose
// generators stand in wherever a zone of its own sets none.
struct Layers {
  const Zone* presetGlobal = nullptr;
  const Zone* preset = nullptr;
  const Zone* instrumentGlobal = nullptr;
  const Zone* instrument = nullptr;

  // The instrument's value (its zone, else its global zone, else the default) plus what the preset
  // adds (its zone, else its global zone), held in the generator's range.
  int Value(unsigned generator) const
  {
    const GenRule& rule = kRules[generator];
    int value = rule.initial;
    if (instrument->Has(generator)) value = instrument->value[generator];
    else if (instrumentGlobal != nullptr && instrumentGlobal->Has(generator)) value = instrumentGlobal->value[generator];
    if (rule.additive)
    {
      if (preset->Has(generator)) value += preset->value[generator];
      else if (presetGlobal != nullptr && presetGlobal->Has(generator)) value += presetGlobal->value[generator];
    }
    return (std::clamp)(value, static_cast<int>(rule.low), static_cast<int>(rule.high));
  }
};

struct Range {
  int low = 0;
  int high = 127;
};

Range KeyRange(const Zone* zone, const Zone* global)
{
  if (zone->hasKeyRange) return {zone->keyLow, zone->keyHigh};
  if (global != nullptr && global->hasKeyRange) return {global->keyLow, global->keyHigh};
  return {};
}

Range VelRange(const Zone* zone, const Zone* global)
{
  if (zone->hasVelRange) return {zone->velLow, zone->velHigh};
  if (global != nullptr && global->hasVelRange) return {global->velLow, global->velHigh};
  return {};
}

// What is known of a region before its sample has a slot.
struct Draft {
  std::uint16_t sample = 0;
  std::uint16_t link = 0;
  bool right = false;
  bool dropped = false;
  std::uint32_t fileStart = 0;  // frames in the file's sample data
  std::uint32_t fileEnd = 0;    // the last frame it can reach, plus one
};

Envelope ReadEnvelope(const Layers& layers, unsigned delay, unsigned keyToHold)
{
  Envelope envelope;
  envelope.delay = static_cast<std::int16_t>(layers.Value(delay));
  envelope.attack = static_cast<std::int16_t>(layers.Value(delay + 1));
  envelope.hold = static_cast<std::int16_t>(layers.Value(delay + 2));
  envelope.decay = static_cast<std::int16_t>(layers.Value(delay + 3));
  envelope.sustain = static_cast<std::int16_t>(layers.Value(delay + 4));
  envelope.release = static_cast<std::int16_t>(layers.Value(delay + 5));
  envelope.keyToHold = static_cast<std::int16_t>(layers.Value(keyToHold));
  envelope.keyToDecay = static_cast<std::int16_t>(layers.Value(keyToHold + 1));
  return envelope;
}

// False: the region cannot sound (a sample that is not there, in ROM or of no length) and is left out.
bool FillRegion(File& file, const Layers& layers, Region& region, Draft& draft, Status& status)
{
  const Range instrumentKeys = KeyRange(layers.instrument, layers.instrumentGlobal);
  const Range presetKeys = KeyRange(layers.preset, layers.presetGlobal);
  const Range instrumentVels = VelRange(layers.instrument, layers.instrumentGlobal);
  const Range presetVels = VelRange(layers.preset, layers.presetGlobal);
  const int keyLow = (std::max)(instrumentKeys.low, presetKeys.low);
  const int keyHigh = (std::min)(instrumentKeys.high, presetKeys.high);
  const int velLow = (std::max)(instrumentVels.low, presetVels.low);
  const int velHigh = (std::min)(instrumentVels.high, presetVels.high);
  if (keyLow > keyHigh || velLow > velHigh) return false;

  SampleHeader header;
  if (!file.ReadSampleHeader(layers.instrument->target, header)) return false;
  if ((header.type & kSampleTypeCompressed) != 0)
  {
    status = Status::Unsupported;
    return false;
  }
  if ((header.type & kSampleTypeRom) != 0) return false;
  if (header.rate < kMinSampleRate || header.rate > kMaxSampleRate) return false;
  if (header.start >= header.end || header.end > file.SampleFrames()) return false;

  // The address offsets move the points inside the sample, never out of it.
  const std::int64_t first = header.start;
  const std::int64_t last = header.end;
  std::int64_t start = first + layers.Value(kStartAddrsOffset) + 32768 * std::int64_t{layers.Value(kStartAddrsCoarseOffset)};
  std::int64_t end = last + layers.Value(kEndAddrsOffset) + 32768 * std::int64_t{layers.Value(kEndAddrsCoarseOffset)};
  std::int64_t loopStart = std::int64_t{header.loopStart} + layers.Value(kStartloopAddrsOffset) + 32768 * std::int64_t{layers.Value(kStartloopAddrsCoarseOffset)};
  std::int64_t loopEnd = std::int64_t{header.loopEnd} + layers.Value(kEndloopAddrsOffset) + 32768 * std::int64_t{layers.Value(kEndloopAddrsCoarseOffset)};
  start = (std::clamp)(start, first, last);
  end = (std::clamp)(end, start, last);
  if (end - start < 2) return false;
  if (loopStart > loopEnd) std::swap(loopStart, loopEnd);
  loopStart = (std::clamp)(loopStart, start, end);
  loopEnd = (std::clamp)(loopEnd, start, end);
  int mode = layers.Value(kSampleModes);
  if (mode == 2 || loopEnd - loopStart < 2) mode = 0;  // 2 is unused by the specification; a loop needs two frames

  region.keyLow = static_cast<std::uint8_t>(keyLow);
  region.keyHigh = static_cast<std::uint8_t>(keyHigh);
  region.velLow = static_cast<std::uint8_t>(velLow);
  region.velHigh = static_cast<std::uint8_t>(velHigh);
  region.mode = static_cast<std::uint8_t>(mode);
  region.exclusiveClass = static_cast<std::uint8_t>(layers.Value(kExclusiveClass));
  region.fixedKey = static_cast<std::int8_t>(layers.Value(kKeynum));
  region.fixedVelocity = static_cast<std::int8_t>(layers.Value(kVelocity));
  const int rootKey = layers.Value(kOverridingRootKey);
  region.rootKey = static_cast<std::uint8_t>(rootKey >= 0 ? rootKey : (header.originalPitch <= 127 ? header.originalPitch : 60));
  region.scaleTuning = static_cast<std::int16_t>(layers.Value(kScaleTuning));
  region.tuneCents = static_cast<std::int16_t>(layers.Value(kCoarseTune) * 100 + layers.Value(kFineTune) + header.pitchCorrection);
  region.rateRatioQ24 = static_cast<std::uint32_t>((std::uint64_t{header.rate} << 24) / kOutputRate);
  // The attenuation counts for 0.4 of what the file says, as the synthesizer the banks are
  // auditioned on has it (docs/sf2-engine-design.md).
  region.attenuationCb = static_cast<std::int16_t>(layers.Value(kInitialAttenuation) * 2 / 5);
  region.filterFc = static_cast<std::int16_t>(layers.Value(kInitialFilterFc));
  region.filterQ = static_cast<std::int16_t>(layers.Value(kInitialFilterQ));
  region.modLfoToPitch = static_cast<std::int16_t>(layers.Value(kModLfoToPitch));
  region.vibLfoToPitch = static_cast<std::int16_t>(layers.Value(kVibLfoToPitch));
  region.modEnvToPitch = static_cast<std::int16_t>(layers.Value(kModEnvToPitch));
  region.modLfoToFilter = static_cast<std::int16_t>(layers.Value(kModLfoToFilterFc));
  region.modEnvToFilter = static_cast<std::int16_t>(layers.Value(kModEnvToFilterFc));
  region.modLfoToVolume = static_cast<std::int16_t>(layers.Value(kModLfoToVolume));
  region.modLfoDelay = static_cast<std::int16_t>(layers.Value(kDelayModLfo));
  region.modLfoFreq = static_cast<std::int16_t>(layers.Value(kFreqModLfo));
  region.vibLfoDelay = static_cast<std::int16_t>(layers.Value(kDelayVibLfo));
  region.vibLfoFreq = static_cast<std::int16_t>(layers.Value(kFreqVibLfo));
  region.modEnv = ReadEnvelope(layers, kDelayModEnv, kKeynumToModEnvHold);
  region.volEnv = ReadEnvelope(layers, kDelayVolEnv, kKeynumToVolEnvHold);

  // In the file's frames for now; the slot's first frame is taken off once the slots are known.
  region.start = static_cast<std::uint32_t>(start);
  region.end = static_cast<std::uint32_t>(end);
  region.loopStart = static_cast<std::uint32_t>(loopStart);
  region.loopEnd = static_cast<std::uint32_t>(loopEnd);

  draft.sample = layers.instrument->target;
  draft.link = header.link;
  draft.right = (header.type & kSampleTypeRight) != 0;
  draft.fileStart = region.start;
  // A loop that never ends needs nothing after it.
  draft.fileEnd = mode == 1 ? region.loopEnd : region.end;
  return true;
}

bool SameRanges(const Region& a, const Region& b) { return a.keyLow == b.keyLow && a.keyHigh == b.keyHigh && a.velLow == b.velLow && a.velHigh == b.velHigh; }

}  // namespace

Program::Result Program::Build(File& file, std::size_t position, std::size_t budgetBytes, RoomCheck roomFor)
{
  Result result;
  if (position >= file.PresetCount())
  {
    result.status = Status::NotFound;
    return result;
  }
  const std::uint32_t headerIndex = file.Preset(position).header;
  std::uint8_t header[38];
  std::uint8_t nextHeader[38];
  if (!file.ReadRecord(kPhdr, headerIndex, header) || !file.ReadRecord(kPhdr, headerIndex + 1, nextHeader))
  {
    result.status = Status::ReadError;
    return result;
  }
  const std::uint32_t bagFirst = U16(header + 24);
  const std::uint32_t bagEnd = U16(nextHeader + 24);
  if (bagEnd < bagFirst)
  {
    result.status = Status::Corrupt;
    return result;
  }

  struct SlotDraft {
    std::uint16_t sample = 0;
    std::uint32_t start = 0;
    std::uint32_t end = 0;
  };
  const auto hasRoom = [roomFor](std::uint64_t bytes) { return roomFor == nullptr || roomFor(bytes); };
  constexpr std::size_t scratchBytes = kMaxRegions * (sizeof(Region) + sizeof(Draft)) + kMaxSampleSlots * sizeof(SlotDraft);
  if (!hasRoom(scratchBytes))
  {
    result.status = Status::NoMemory;
    return result;
  }
  std::unique_ptr<Region[]> regions(new (memory::slow) Region[kMaxRegions]);
  std::unique_ptr<Draft[]> drafts(new (memory::slow) Draft[kMaxRegions]);
  if (!regions || !drafts)
  {
    result.status = Status::NoMemory;
    return result;
  }
  std::size_t count = 0;

  Zone presetGlobal;
  Zone presetZone;
  Zone instrumentGlobal;
  Zone instrumentZone;
  bool hasPresetGlobal = false;
  for (std::uint32_t bag = bagFirst; bag < bagEnd; ++bag)
  {
    std::uint32_t first = 0;
    std::uint32_t generators = 0;
    result.status = file.BagGenerators(kPbag, bag, first, generators);
    if (result.status == Status::Ok) result.status = file.ReadZone(kPgen, first, generators, kInstrument, presetZone);
    if (result.status != Status::Ok) return result;
    if (!presetZone.hasTarget)
    {
      // Only the first zone may be the global one; any other without an instrument says nothing.
      if (bag == bagFirst)
      {
        presetGlobal = presetZone;
        hasPresetGlobal = true;
      }
      continue;
    }
    std::uint8_t instrument[22];
    std::uint8_t nextInstrument[22];
    // The last record of the instrument table only ends it.
    if (presetZone.target + 1u >= file.RecordCount(kInst)) continue;
    if (!file.ReadRecord(kInst, presetZone.target, instrument) || !file.ReadRecord(kInst, presetZone.target + 1u, nextInstrument))
    {
      result.status = Status::ReadError;
      return result;
    }
    const std::uint32_t zoneFirst = U16(instrument + 20);
    const std::uint32_t zoneEnd = U16(nextInstrument + 20);
    if (zoneEnd < zoneFirst)
    {
      result.status = Status::Corrupt;
      return result;
    }
    bool hasInstrumentGlobal = false;
    for (std::uint32_t zone = zoneFirst; zone < zoneEnd; ++zone)
    {
      result.status = file.BagGenerators(kIbag, zone, first, generators);
      if (result.status == Status::Ok) result.status = file.ReadZone(kIgen, first, generators, kSampleId, instrumentZone);
      if (result.status != Status::Ok) return result;
      if (!instrumentZone.hasTarget)
      {
        if (zone == zoneFirst)
        {
          instrumentGlobal = instrumentZone;
          hasInstrumentGlobal = true;
        }
        continue;
      }
      if (count >= kMaxRegions)
      {
        result.status = Status::TooLarge;
        return result;
      }
      Layers layers;
      layers.presetGlobal = hasPresetGlobal ? &presetGlobal : nullptr;
      layers.preset = &presetZone;
      layers.instrumentGlobal = hasInstrumentGlobal ? &instrumentGlobal : nullptr;
      layers.instrument = &instrumentZone;
      regions[count] = Region{};
      drafts[count] = Draft{};
      if (FillRegion(file, layers, regions[count], drafts[count], result.status)) ++count;
      else if (result.status != Status::Ok) return result;
    }
  }

  // The output is mono: of a stereo pair on the same keys only the left sample plays.
  for (std::size_t index = 0; index < count; ++index)
  {
    if (!drafts[index].right) continue;
    for (std::size_t other = 0; other < count; ++other)
    {
      if (other == index || drafts[other].right || drafts[other].sample != drafts[index].link) continue;
      if (!SameRanges(regions[index], regions[other])) continue;
      drafts[index].dropped = true;
      break;
    }
  }

  // A slot per sample: from the first frame any of its regions starts at to the last one reaches.
  std::unique_ptr<SlotDraft[]> slotDrafts(new (memory::slow) SlotDraft[kMaxSampleSlots]);
  if (!slotDrafts)
  {
    result.status = Status::NoMemory;
    return result;
  }
  std::size_t slotCount = 0;
  std::size_t kept = 0;
  for (std::size_t index = 0; index < count; ++index)
  {
    if (drafts[index].dropped) continue;
    ++kept;
    std::size_t slot = 0;
    while (slot < slotCount && slotDrafts[slot].sample != drafts[index].sample) ++slot;
    if (slot == slotCount)
    {
      if (slotCount >= kMaxSampleSlots)
      {
        result.status = Status::TooLarge;
        return result;
      }
      slotDrafts[slot] = SlotDraft{drafts[index].sample, drafts[index].fileStart, drafts[index].fileEnd};
      ++slotCount;
    }
    else
    {
      slotDrafts[slot].start = (std::min)(slotDrafts[slot].start, drafts[index].fileStart);
      slotDrafts[slot].end = (std::max)(slotDrafts[slot].end, drafts[index].fileEnd);
    }
    regions[index].slot = static_cast<std::uint16_t>(slot);
  }
  if (kept == 0)
  {
    result.status = Status::Empty;
    return result;
  }

  // Counted in 64 bits: a large font's samples overflow a 32-bit size.
  std::uint64_t bytes = sizeof(Program) + kept * sizeof(Region) + slotCount * sizeof(Slot);
  for (std::size_t slot = 0; slot < slotCount; ++slot) bytes += (std::uint64_t{slotDrafts[slot].end - slotDrafts[slot].start} + 1) * sizeof(std::int16_t);
  result.neededBytes = bytes > SIZE_MAX ? SIZE_MAX : static_cast<std::size_t>(bytes);
  // Check live space with all three scratch tables allocated. Old or unread patches still own
  // their memory, including those on other tracks.
  if (bytes > budgetBytes || !hasRoom(bytes))
  {
    result.status = Status::TooBig;
    return result;
  }

  std::unique_ptr<Program> program(new (memory::slow) Program);
  if (program)
  {
    program->regions_.reset(new (memory::slow) Region[kept]);
    program->slots_.reset(new (memory::slow) Slot[slotCount]);
  }
  if (!program || !program->regions_ || !program->slots_)
  {
    result.status = Status::NoMemory;
    return result;
  }
  program->bytes_ = static_cast<std::size_t>(bytes);
  program->slotCount_ = slotCount;
  for (std::size_t slot = 0; slot < slotCount; ++slot)
  {
    const std::uint32_t frames = slotDrafts[slot].end - slotDrafts[slot].start;
    program->slots_[slot].frames = frames;
    const std::size_t count = std::size_t{frames} + 1;
    if (hasRoom(count * sizeof(std::int16_t))) program->slots_[slot].data.reset(new (memory::slow) std::int16_t[count]);
    if (!program->slots_[slot].data || !hasRoom(0))
    {
      result.status = Status::NoMemory;
      return result;
    }
    if (!file.ReadSampleFrames(slotDrafts[slot].start, program->slots_[slot].data.get(), frames))
    {
      result.status = Status::ReadError;
      return result;
    }
    // The frame after the last: a voice reads one frame ahead of where it is.
    program->slots_[slot].data[frames] = 0;
  }
  for (std::size_t index = 0; index < count; ++index)
  {
    if (drafts[index].dropped) continue;
    Region& region = program->regions_[program->regionCount_++];
    region = regions[index];
    const std::uint32_t base = slotDrafts[region.slot].start;
    region.start -= base;
    region.end -= base;
    region.loopStart -= base;
    region.loopEnd -= base;
  }
  result.program = std::move(program);
  return result;
}

std::size_t Program::Find(unsigned key, unsigned velocity, const Region** regions, std::size_t capacity) const
{
  std::size_t found = 0;
  for (std::size_t index = 0; index < regionCount_ && found < capacity; ++index)
    if (regions_[index].Sounds(key, velocity)) regions[found++] = &regions_[index];
  return found;
}

}  // namespace synth::audio::sf2

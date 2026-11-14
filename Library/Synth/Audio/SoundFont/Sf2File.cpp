#include "Library/Synth/Audio/SoundFont/Sf2File.h"

#include <algorithm>
#include <cstring>

#include "Library/Memory/Memory.h"

namespace synth::audio::sf2 {

namespace {

bool Tag(const std::uint8_t* bytes, const char* tag) { return std::memcmp(bytes, tag, 4) == 0; }

constexpr unsigned kMaxChunks = 64;  // of one list: a file with more is not one of ours to walk

}  // namespace

Status File::Open(Reader& reader)
{
  reader_ = &reader;
  presets_.reset();
  presetCount_ = 0;
  sampleOffset_ = 0;
  sampleFrames_ = 0;
  for (auto& offset : offsets_) offset = 0;
  for (auto& count : counts_) count = 0;
  const Status layout = ReadLayout();
  if (layout != Status::Ok) return layout;
  return ReadPresets();
}

Status File::ReadLayout()
{
  std::uint8_t head[12];
  if (reader_->Size() < sizeof(head)) return Status::NotSoundFont;
  if (!reader_->Read(0, head, sizeof(head))) return Status::ReadError;
  if (!Tag(head, "RIFF") || !Tag(head + 8, "sfbk")) return Status::NotSoundFont;
  const std::uint64_t fileEnd = (std::min)(reader_->Size(), std::uint64_t{8} + U32(head + 4));

  bool haveSamples = false;
  bool haveTable[kTableCount] = {};
  std::uint64_t at = 12;
  for (unsigned chunk = 0; chunk < kMaxChunks && at + 8 <= fileEnd; ++chunk)
  {
    std::uint8_t header[12];
    if (!reader_->Read(at, header, 8)) return Status::ReadError;
    const std::uint64_t size = U32(header + 4);
    const std::uint64_t next = at + 8 + size + (size & 1u);
    if (at + 8 + size > fileEnd) return Status::Corrupt;
    if (Tag(header, "LIST") && size >= 4)
    {
      if (!reader_->Read(at + 8, header + 8, 4)) return Status::ReadError;
      const bool info = Tag(header + 8, "INFO");
      const bool samples = Tag(header + 8, "sdta");
      const bool presets = Tag(header + 8, "pdta");
      const std::uint64_t listEnd = at + 8 + size;
      std::uint64_t inner = at + 12;
      for (unsigned part = 0; (info || samples || presets) && part < kMaxChunks && inner + 8 <= listEnd; ++part)
      {
        std::uint8_t sub[8];
        if (!reader_->Read(inner, sub, 8)) return Status::ReadError;
        const std::uint64_t subSize = U32(sub + 4);
        if (inner + 8 + subSize > listEnd) return Status::Corrupt;
        if (info && Tag(sub, "ifil") && subSize >= 4)
        {
          std::uint8_t version[4];
          if (!reader_->Read(inner + 8, version, 4)) return Status::ReadError;
          if (U16(version) != 2) return Status::Unsupported;  // 3 is SF3, compressed samples
        }
        else if (samples && Tag(sub, "smpl"))
        {
          sampleOffset_ = inner + 8;
          sampleFrames_ = static_cast<std::uint32_t>(subSize / 2);
          haveSamples = true;
        }
        else if (presets)
        {
          for (unsigned table = 0; table < kTableCount; ++table)
          {
            if (!Tag(sub, kTableTags[table])) continue;
            if (subSize % kRecordBytes[table] != 0 || subSize == 0) return Status::Corrupt;
            offsets_[table] = inner + 8;
            counts_[table] = static_cast<std::uint32_t>(subSize / kRecordBytes[table]);
            haveTable[table] = true;
          }
        }
        inner += 8 + subSize + (subSize & 1u);
      }
    }
    at = next;
  }
  for (bool have : haveTable)
    if (!have) return Status::Corrupt;
  if (!haveSamples || sampleFrames_ == 0) return Status::Unsupported;
  return Status::Ok;
}

Status File::ReadPresets()
{
  // The last record of each table only ends it.
  const std::uint32_t count = counts_[kPhdr] - 1;
  if (count == 0) return Status::Empty;
  if (count > kMaxPresets) return Status::TooLarge;
  presets_.reset(new (memory::slow) PresetEntry[count]());
  if (!presets_) return Status::NoMemory;
  for (std::uint32_t index = 0; index < count; ++index)
  {
    std::uint8_t record[38];
    if (!ReadRecord(kPhdr, index, record)) return Status::ReadError;
    presets_[index].program = U16(record + 20);
    presets_[index].bank = U16(record + 22);
    presets_[index].header = static_cast<std::uint16_t>(index);
  }
  presetCount_ = count;
  std::sort(presets_.get(), presets_.get() + count, [](const PresetEntry& a, const PresetEntry& b) {
    if (a.bank != b.bank) return a.bank < b.bank;
    if (a.program != b.program) return a.program < b.program;
    return a.header < b.header;
  });
  return Status::Ok;
}

int File::FindPreset(unsigned bank, unsigned program) const
{
  for (std::size_t position = 0; position < presetCount_; ++position)
    if (presets_[position].bank == bank && presets_[position].program == program) return static_cast<int>(position);
  return -1;
}

bool File::PresetName(std::size_t position, char name[21])
{
  name[0] = 0;
  if (position >= presetCount_) return false;
  std::uint8_t record[38];
  if (!ReadRecord(kPhdr, presets_[position].header, record)) return false;
  std::size_t length = 0;
  for (; length < 20 && record[length] != 0; ++length)
  {
    // Names are ASCII by the specification; anything else would not draw.
    name[length] = record[length] >= 32 && record[length] < 127 ? static_cast<char>(record[length]) : '?';
  }
  while (length > 0 && name[length - 1] == ' ') --length;
  name[length] = 0;
  return true;
}

bool File::ReadRecord(Table table, std::uint32_t index, std::uint8_t* bytes)
{
  if (reader_ == nullptr || index >= counts_[table]) return false;
  return reader_->Read(offsets_[table] + std::uint64_t{index} * kRecordBytes[table], bytes, kRecordBytes[table]);
}

Status File::BagGenerators(Table bagTable, std::uint32_t bag, std::uint32_t& first, std::uint32_t& count)
{
  if (bag + 1 >= counts_[bagTable]) return Status::Corrupt;
  std::uint8_t records[8];
  if (!reader_->Read(offsets_[bagTable] + std::uint64_t{bag} * 4, records, sizeof(records))) return Status::ReadError;
  first = U16(records);
  const std::uint32_t end = U16(records + 4);
  const Table generators = bagTable == kPbag ? kPgen : kIgen;
  if (end < first || end > counts_[generators]) return Status::Corrupt;
  count = end - first;
  return Status::Ok;
}

Status File::ReadZone(Table generatorTable, std::uint32_t first, std::uint32_t count, std::uint16_t terminal, Zone& zone)
{
  zone = Zone{};
  if (count > kMaxZoneGenerators) return Status::TooLarge;
  if (first + count > counts_[generatorTable]) return Status::Corrupt;
  for (std::uint32_t index = 0; index < count; ++index)
  {
    // A record at a time: the reader's window makes it a copy, and the loading task's stack is small.
    std::uint8_t record[4];
    if (!reader_->Read(offsets_[generatorTable] + std::uint64_t{first + index} * 4, record, sizeof(record))) return Status::ReadError;
    const std::uint16_t generator = U16(record);
    if (generator == kKeyRange || generator == kVelRange)
    {
      const std::uint8_t low = (std::min)(record[2], std::uint8_t{127});
      const std::uint8_t high = (std::min)(record[3], std::uint8_t{127});
      (generator == kKeyRange ? zone.keyLow : zone.velLow) = (std::min)(low, high);
      (generator == kKeyRange ? zone.keyHigh : zone.velHigh) = (std::max)(low, high);
      (generator == kKeyRange ? zone.hasKeyRange : zone.hasVelRange) = true;
    }
    else if (generator == terminal)
    {
      // What follows the instrument or the sample in a zone is ignored (specification, 8.1.2).
      zone.hasTarget = true;
      zone.target = U16(record + 2);
      break;
    }
    else if (generator < kGenCount)
    {
      zone.value[generator] = S16(record + 2);
      zone.present |= std::uint64_t{1} << generator;
    }
  }
  return Status::Ok;
}

bool File::ReadSampleHeader(std::uint32_t index, SampleHeader& header)
{
  std::uint8_t record[46];
  // The last record only ends the table.
  if (index + 1 >= counts_[kShdr] || !ReadRecord(kShdr, index, record)) return false;
  header.start = U32(record + 20);
  header.end = U32(record + 24);
  header.loopStart = U32(record + 28);
  header.loopEnd = U32(record + 32);
  header.rate = U32(record + 36);
  header.originalPitch = record[40];
  header.pitchCorrection = static_cast<std::int8_t>(record[41]);
  header.link = U16(record + 42);
  header.type = U16(record + 44);
  return true;
}

bool File::ReadSampleFrames(std::uint32_t firstFrame, std::int16_t* destination, std::uint32_t count)
{
  if (reader_ == nullptr || firstFrame > sampleFrames_ || count > sampleFrames_ - firstFrame) return false;
  if (count == 0) return true;
  auto* bytes = reinterpret_cast<std::uint8_t*>(destination);
  if (!reader_->Read(sampleOffset_ + std::uint64_t{firstFrame} * 2, bytes, std::size_t{count} * 2)) return false;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  for (std::uint32_t frame = 0; frame < count; ++frame) destination[frame] = S16(bytes + frame * 2);
#endif
  return true;
}

}  // namespace synth::audio::sf2

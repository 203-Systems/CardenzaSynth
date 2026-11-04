#include "Applications/Synth/Runtime/SessionStore.h"

#include "Library/Memory/Memory.h"

#include <algorithm>
#include <cstdlib>
#include <new>
#include <utility>

#include "Applications/Synth/Runtime/SynthStorage.h"
#include "OS/Storage/Storage.h"

namespace synth {

namespace {

constexpr std::uint8_t kFormat = 1;
constexpr std::size_t kGeneral = 0;
constexpr std::size_t kSong = 1;
constexpr std::size_t kEngines = 2;
constexpr std::size_t kTracks = kEngines + kSynthTrackCount;
constexpr std::size_t kPatterns = kTracks + kSynthTrackCount;
constexpr const char* kBootKey = "as_boot";
constexpr int kMaxRestoreTries = 3;
// The flash holds other records too, and a replaced record needs room next to the old one until it is
// complete. In NVS entries of 32 bytes, three of them going to the bookkeeping of each record.
constexpr std::size_t kMaxEntries = 400;
constexpr std::size_t kEntryBytes = 32;
constexpr std::size_t kRecordOverheadEntries = 3;

std::size_t PatternSection(std::size_t track, std::size_t pattern)
{
  return kPatterns + track * kSequencerMaxPatternCount + pattern;
}

std::string SectionKey(std::size_t section)
{
  if (section == kGeneral) return "as_g";
  if (section == kSong) return "as_s";
  if (section < kTracks) return "as_e" + std::to_string(section - kEngines);
  if (section < kPatterns) return "as_t" + std::to_string(section - kTracks);
  const std::size_t index = section - kPatterns;
  return "as_p" + std::to_string(index / kSequencerMaxPatternCount) + "_" + std::to_string(index % kSequencerMaxPatternCount);
}

std::uint32_t HashOf(const std::string& bytes)
{
  std::uint32_t hash = 2166136261u;
  for (const char byte : bytes) hash = (hash ^ static_cast<unsigned char>(byte)) * 16777619u;
  return hash | 1u;  // never 0, which means no record
}

class Writer {
public:
  void Reserve(std::size_t bytes) { bytes_.reserve(bytes); }
  void Byte(int value) { bytes_.push_back(static_cast<char>(value)); }
  void Number(std::uint32_t value)
  {
    while (value >= 0x80)
    {
      Byte(static_cast<int>(value & 0x7F) | 0x80);
      value >>= 7;
    }
    Byte(static_cast<int>(value));
  }
  void Signed(int value) { Number((static_cast<std::uint32_t>(value) << 1) ^ static_cast<std::uint32_t>(value >> 31)); }
  void Text(const std::string& text)
  {
    Number(static_cast<std::uint32_t>(text.size()));
    bytes_ += text;
  }
  std::string Take() { return std::move(bytes_); }

private:
  std::string bytes_;
};

class Reader {
public:
  explicit Reader(const std::string& bytes) : bytes_(bytes) {}
  bool Good() const { return good_; }
  bool AtEnd() const { return position_ >= bytes_.size(); }
  int Byte()
  {
    if (position_ >= bytes_.size())
    {
      good_ = false;
      return 0;
    }
    return static_cast<unsigned char>(bytes_[position_++]);
  }
  std::uint32_t Number()
  {
    std::uint32_t value = 0;
    for (int shift = 0; shift < 35; shift += 7)
    {
      const int byte = Byte();
      value |= static_cast<std::uint32_t>(byte & 0x7F) << shift;
      if ((byte & 0x80) == 0) return value;
    }
    good_ = false;
    return 0;
  }
  int Signed()
  {
    const std::uint32_t value = Number();
    return static_cast<int>(value >> 1) ^ -static_cast<int>(value & 1);
  }
  std::string Text()
  {
    const std::size_t length = Number();
    if (!good_ || length > bytes_.size() - position_)
    {
      good_ = false;
      return {};
    }
    std::string text = bytes_.substr(position_, length);
    position_ += length;
    return text;
  }

private:
  const std::string& bytes_;
  std::size_t position_ = 0;
  bool good_ = true;
};

std::string EncodeGeneral(const SessionInfo& info, const ProjectData& project)
{
  const auto& sequence = *project.sequence;
  Writer out;
  out.Byte(kFormat);
  out.Byte(info.dirty ? 1 : 0);
  out.Text(info.name);
  out.Byte(static_cast<int>(project.activeTrack));
  out.Signed(project.octave);
  out.Byte(project.root);
  out.Byte(static_cast<int>(project.scale));
  out.Byte(project.layout);
  out.Byte(project.lock);
  out.Byte(project.velocity);
  out.Number(sequence.bpm);
  out.Byte(sequence.swing);
  int muted = 0;
  int soloed = 0;
  for (std::size_t track = 0; track < kSynthTrackCount; ++track)
  {
    out.Byte(project.mixer.level[track]);
    muted |= project.mixer.muted[track] ? 1 << track : 0;
    soloed |= project.mixer.soloed[track] ? 1 << track : 0;
  }
  out.Byte(muted);
  out.Byte(soloed);
  out.Number(static_cast<std::uint32_t>(project.masterFx.size()));
  for (const std::uint8_t value : project.masterFx) out.Byte(value);
  for (const auto& play : project.play)
  {
    out.Byte(play.chord);
    out.Byte(play.voicing);
    out.Byte(static_cast<int>(play.arp.mode));
    out.Byte(play.arp.rate);
    out.Byte(play.arp.octaves);
    out.Byte(play.arp.gate);
    out.Byte(play.arp.latch ? 1 : 0);
  }
  // Last, so a record from before it reads as generation 0.
  out.Number(info.generation);
  return out.Take();
}

bool DecodeGeneral(const std::string& bytes, SessionInfo& info, ProjectData& project)
{
  Reader in(bytes);
  auto& sequence = *project.sequence;
  if (in.Byte() != kFormat) return false;
  info.dirty = in.Byte() != 0;
  info.name = in.Text();
  project.activeTrack = std::min<std::size_t>(static_cast<std::size_t>(in.Byte()), kSynthTrackCount - 1);
  project.octave = in.Signed();
  project.root = in.Byte();
  project.scale = static_cast<std::size_t>(in.Byte());
  project.layout = in.Byte();
  project.lock = in.Byte();
  project.velocity = in.Byte();
  sequence.bpm = static_cast<std::uint16_t>(in.Number());
  sequence.swing = static_cast<std::uint8_t>(in.Byte());
  for (auto& level : project.mixer.level) level = static_cast<std::uint8_t>(in.Byte());
  const int muted = in.Byte();
  const int soloed = in.Byte();
  for (std::size_t track = 0; track < kSynthTrackCount; ++track)
  {
    project.mixer.muted[track] = (muted >> track) & 1;
    project.mixer.soloed[track] = (soloed >> track) & 1;
  }
  const std::size_t effects = in.Number();
  for (std::size_t index = 0; index < effects && in.Good(); ++index)
  {
    const int value = in.Byte();
    if (index < project.masterFx.size()) project.masterFx[index] = static_cast<std::uint8_t>(value);
  }
  for (auto& play : project.play)
  {
    play.chord = in.Byte();
    play.voicing = in.Byte();
    play.arp.mode = static_cast<ArpMode>(in.Byte());
    play.arp.rate = static_cast<std::uint8_t>(in.Byte());
    play.arp.octaves = static_cast<std::uint8_t>(in.Byte());
    play.arp.gate = static_cast<std::uint8_t>(in.Byte());
    play.arp.latch = in.Byte() != 0;
  }
  info.generation = in.AtEnd() ? 0 : in.Number();
  project.hasPlay = true;
  return in.Good();
}

std::string EncodeSong(const SynthSequencer::ProjectState& sequence)
{
  // A song is rare: no record at all until there is one.
  if (sequence.songLength == 0 && sequence.songLoop) return {};
  Writer out;
  out.Number(static_cast<std::uint32_t>(sequence.songLength));
  out.Byte(sequence.songLoop ? 1 : 0);
  for (std::size_t row = 0; row < sequence.songLength; ++row)
  {
    for (const std::int8_t pattern : sequence.song[row].patterns) out.Byte(pattern + 1);
    out.Byte(sequence.song[row].repeats);
  }
  return out.Take();
}

bool DecodeSong(const std::string& bytes, SynthSequencer::ProjectState& sequence)
{
  Reader in(bytes);
  const std::size_t length = in.Number();
  sequence.songLoop = in.Byte() != 0;
  if (!in.Good() || length > kSongMaxRows) return false;
  sequence.songLength = length;
  for (std::size_t row = 0; row < length; ++row)
  {
    for (auto& pattern : sequence.song[row].patterns) pattern = static_cast<std::int8_t>(in.Byte() - 1);
    sequence.song[row].repeats = static_cast<std::uint8_t>(in.Byte());
  }
  return in.Good();
}

std::string EncodeEngine(SynthEngineKind kind, const SynthEngineState& state, const std::string& sample)
{
  Writer out;
  out.Byte(static_cast<int>(kind));
  out.Text(state.basePresetId);
  out.Text(state.presetId == state.basePresetId ? std::string() : state.presetId);
  out.Text(state.presetName);
  out.Text(sample);
  out.Number(static_cast<std::uint32_t>(state.values.size()));
  for (const auto& entry : state.values)
  {
    out.Text(entry.first);
    out.Signed(entry.second);
  }
  return out.Take();
}

bool DecodeEngine(const std::string& bytes, SynthEngineKind& kind, SynthEngineState& state, std::string& sample)
{
  Reader in(bytes);
  kind = static_cast<SynthEngineKind>(in.Byte());
  state.basePresetId = in.Text();
  state.presetId = in.Text();
  if (state.presetId.empty()) state.presetId = state.basePresetId;
  state.presetName = in.Text();
  sample = in.Text();
  const std::size_t count = in.Number();
  for (std::size_t index = 0; index < count && in.Good(); ++index)
  {
    std::string name = in.Text();
    const int value = in.Signed();
    if (in.Good()) state.values.emplace(std::move(name), value);
  }
  return in.Good();
}

std::string EncodeTrack(const SynthSequencer::TrackData& track)
{
  Writer out;
  out.Number(static_cast<std::uint32_t>(track.patternCount));
  out.Number(static_cast<std::uint32_t>(track.activePattern));
  out.Byte(track.defaultVelocity);
  out.Number(track.defaultLengthPulses);
  out.Byte((track.enabled ? 1 : 0) | (track.muted ? 2 : 0));
  for (std::size_t pattern = 0; pattern < track.patternCount; ++pattern) out.Number(track.patternSteps[pattern]);
  return out.Take();
}

bool DecodeTrack(const std::string& bytes, SynthSequencer::TrackData& track)
{
  Reader in(bytes);
  const std::size_t count = in.Number();
  if (!in.Good() || count < 1 || count > kSequencerMaxPatternCount) return false;
  track.patternCount = count;
  track.activePattern = in.Number();
  track.defaultVelocity = static_cast<std::uint8_t>(in.Byte());
  track.defaultLengthPulses = static_cast<std::uint16_t>(in.Number());
  const int flags = in.Byte();
  track.enabled = (flags & 1) != 0;
  track.muted = (flags & 2) != 0;
  for (std::size_t pattern = 0; pattern < count; ++pattern) track.patternSteps[pattern] = static_cast<std::uint16_t>(in.Number());
  return in.Good();
}

// Notes only, 5 to 6 bytes each: a step that holds some, how far it is from the last one, and its notes.
std::size_t CountNotes(const SynthSequencer::PatternData& pattern)
{
  std::size_t notes = 0;
  for (std::size_t step = pattern.steps.NextInUse(0); step < pattern.steps.size(); step = pattern.steps.NextInUse(step + 1))
  {
    const auto& stepNotes = pattern.steps[step].notes;
    notes += static_cast<std::size_t>(std::count_if(stepNotes.begin(), stepNotes.end(), [](const auto& note) { return note.active; }));
  }
  return notes;
}

std::string EncodePattern(const SynthSequencer::PatternData& pattern, std::size_t notes)
{
  Writer out;
  // One block of about the right size: growing doubles, and needs both blocks for a moment.
  out.Reserve(notes * 7);
  std::size_t previous = 0;
  for (std::size_t step = pattern.steps.NextInUse(0); step < pattern.steps.size(); step = pattern.steps.NextInUse(step + 1))
  {
    const auto& notes = pattern.steps[step].notes;
    const auto count = static_cast<std::uint32_t>(std::count_if(notes.begin(), notes.end(), [](const auto& note) { return note.active; }));
    if (count == 0) continue;
    out.Number(static_cast<std::uint32_t>(step - previous));
    out.Byte(static_cast<int>(count));
    previous = step;
    for (const auto& note : notes)
    {
      if (!note.active) continue;
      out.Byte(note.note);
      out.Byte(note.velocity);
      out.Number(note.lengthPulses);
      out.Byte(note.microstep);
    }
  }
  return out.Take();
}

bool DecodePattern(const std::string& bytes, SynthSequencer::PatternData& pattern)
{
  Reader in(bytes);
  std::size_t step = 0;
  for (;;)
  {
    const std::size_t delta = in.Number();
    if (!in.Good()) return true;  // the end
    step += delta;
    const std::size_t count = static_cast<std::size_t>(in.Byte());
    if (!in.Good() || step >= pattern.steps.size() || count == 0 || count > 8) return false;
    auto* data = pattern.steps.TryEdit(step);
    if (data == nullptr) return false;
    for (std::size_t index = 0; index < count; ++index)
    {
      auto& note = data->notes[index];
      note.note = static_cast<std::uint8_t>(in.Byte());
      note.velocity = static_cast<std::uint8_t>(in.Byte());
      note.lengthPulses = static_cast<std::uint16_t>(in.Number());
      note.microstep = static_cast<std::uint8_t>(in.Byte());
      note.active = true;
    }
    if (!in.Good()) return false;
  }
}

}  // namespace

bool SessionStore::Store(std::size_t section, const std::string& bytes)
{
  const std::string key = SectionKey(section);
  if (bytes.empty())
  {
    if (stored_[section] == 0) return true;
    if (!os::storage::EraseInternalRecord(key)) return false;
    stored_[section] = 0;
    return true;
  }
  const std::uint32_t hash = HashOf(bytes);
  if (stored_[section] == hash) return true;
  if (!os::storage::WriteInternalRecord(key, bytes)) return false;
  stored_[section] = hash;
  return true;
}

SessionStore::SaveResult SessionStore::Save(const SessionInfo& info, const ProjectData& project)
{
  if (!project.sequence) return SaveResult::Failed;
  const auto& sequence = *project.sequence;
  std::size_t entries = 0;
  bool ok = true;
  bool partial = false;
  const auto store = [&](std::size_t section, const std::string& bytes) {
    if (!bytes.empty()) entries += bytes.size() / kEntryBytes + kRecordOverheadEntries;
    ok = Store(section, bytes) && ok;
  };
  store(kGeneral, EncodeGeneral(info, project));
  store(kSong, EncodeSong(sequence));
  for (std::size_t track = 0; track < kSynthTrackCount; ++track)
  {
    store(kEngines + track, EncodeEngine(project.kinds[track], project.engines[track], project.samples[track]));
    store(kTracks + track, EncodeTrack(sequence.tracks[track]));
  }
  // What plays first: when there is too much, the others stay out.
  const auto storePattern = [&](std::size_t track, std::size_t pattern) {
    const auto& data = sequence.tracks[track];
    const bool present = pattern < data.patternCount && data.patterns.Allocated(pattern);
    const std::size_t notes = present ? CountNotes(data.patterns[pattern]) : 0;
    // Too long for a record: not even built. Notes take five bytes at the very least.
    const bool fits = notes * 5 <= os::storage::kInternalRecordMaxBytes;
    if (!fits) partial = true;
    std::string bytes = present && fits ? EncodePattern(data.patterns[pattern], notes) : std::string();
    const std::size_t cost = bytes.size() / kEntryBytes + kRecordOverheadEntries;
    if (!bytes.empty() && (bytes.size() > os::storage::kInternalRecordMaxBytes || entries + cost > kMaxEntries))
    {
      partial = true;
      bytes.clear();
    }
    store(PatternSection(track, pattern), bytes);
  };
  for (std::size_t track = 0; track < kSynthTrackCount; ++track) storePattern(track, sequence.tracks[track].activePattern);
  for (std::size_t track = 0; track < kSynthTrackCount; ++track)
    for (std::size_t pattern = 0; pattern < kSequencerMaxPatternCount; ++pattern)
      if (pattern != sequence.tracks[track].activePattern) storePattern(track, pattern);
  if (!ok) return SaveResult::Failed;
  return partial ? SaveResult::Partial : SaveResult::Ok;
}

bool SessionStore::Load(SessionInfo& info, ProjectData& project)
{
  stored_.fill(0);
  std::string bytes;
  // What is there is known first: even if it cannot be used, the next save replaces it record by record.
  const auto read = [&](std::size_t section) {
    bytes.clear();
    if (!os::storage::ReadInternalRecord(SectionKey(section), bytes) || bytes.empty()) return false;
    stored_[section] = HashOf(bytes);
    return true;
  };
  project.sequence.reset(new (memory::slow) SynthSequencer::ProjectState);
  if (!project.sequence) return false;
  auto& sequence = *project.sequence;
  bool valid = read(kGeneral) && DecodeGeneral(bytes, info, project);
  if (read(kSong)) valid = DecodeSong(bytes, sequence) && valid;
  for (std::size_t track = 0; track < kSynthTrackCount; ++track)
  {
    valid = read(kEngines + track) && DecodeEngine(bytes, project.kinds[track], project.engines[track], project.samples[track]) && valid;
    valid = read(kTracks + track) && DecodeTrack(bytes, sequence.tracks[track]) && valid;
    for (std::size_t pattern = 0; pattern < kSequencerMaxPatternCount; ++pattern)
    {
      if (!read(PatternSection(track, pattern))) continue;
      valid = sequence.tracks[track].patterns.Ensure(pattern) && DecodePattern(bytes, sequence.tracks[track].patterns[pattern]) && valid;
    }
  }
  return valid && SynthSequencer::ValidateState(sequence);
}

// Records outlive firmware updates, and Save only touches the keys this firmware knows: one that an
// earlier firmware wrote (another format, a pattern number that is gone) would take up the flash for good.
void SessionStore::EraseUnknownRecords()
{
  for (const std::string& key : os::storage::ListInternalRecords())
  {
    if (key.rfind("as_", 0) != 0 || key == kBootKey) continue;
    bool known = false;
    for (std::size_t section = 0; section < kSectionCount && !known; ++section) known = key == SectionKey(section);
    if (!known) (void)os::storage::EraseInternalRecord(key);
  }
}

bool SessionStore::BeginRestore()
{
  EraseUnknownRecords();
  std::string value;
  const int tries = os::storage::ReadInternalRecord(kBootKey, value) ? std::atoi(value.c_str()) : 0;
  if (tries >= kMaxRestoreTries)
  {
    FinishRestore();
    return false;
  }
  (void)os::storage::WriteInternalRecord(kBootKey, std::to_string(tries + 1));
  return true;
}

void SessionStore::FinishRestore()
{
  (void)os::storage::EraseInternalRecord(kBootKey);
}

namespace {
std::string CardProjectPath() { return os::storage::JoinPath(storage::SessionDirectory(), "autosave.ccproj"); }
std::string CardInfoPath() { return os::storage::JoinPath(storage::SessionDirectory(), "autosave.txt"); }
}  // namespace

bool SessionStore::SaveCard(const SessionInfo& info, const ProjectData& project)
{
  // The project first, then the text that names its generation: cut in between, the start takes the
  // flash copy, which is older but whole.
  if (SaveProjectFileAt(CardProjectPath(), project) != ProjectResult::Ok) return false;
  const std::string text = "generation=" + std::to_string(info.generation) + "\nname=" + info.name + "\ndirty=" + (info.dirty ? "1" : "0") + "\n";
  return os::storage::WriteFileAtomic(CardInfoPath(), text);
}

bool SessionStore::ReadCardInfo(SessionInfo& info)
{
  std::string text;
  if (!os::storage::ReadFileWithBackup(CardInfoPath(), text, 512)) return false;
  SessionInfo read;
  bool generation = false;
  std::size_t start = 0;
  while (start < text.size())
  {
    std::size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(start, end - start);
    start = end + 1;
    const std::size_t equals = line.find('=');
    if (equals == std::string::npos) continue;
    const std::string key = line.substr(0, equals);
    const std::string value = line.substr(equals + 1);
    if (key == "generation")
    {
      read.generation = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
      generation = read.generation != 0;
    }
    else if (key == "name") read.name = value;
    else if (key == "dirty") read.dirty = value == "1";
  }
  if (!generation) return false;
  info = read;
  return true;
}

bool SessionStore::InFlash()
{
  std::string bytes;
  return os::storage::ReadInternalRecord(SectionKey(kGeneral), bytes);
}

bool SessionStore::LoadCard(SessionInfo& info, ProjectData& project)
{
  return ReadCardInfo(info) && LoadProjectFileAt(CardProjectPath(), project) == ProjectResult::Ok;
}

}  // namespace synth

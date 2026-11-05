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
// Every key of an older firmware's autosave in internal flash starts so (its start counter was "as_boot").
constexpr const char* kFlashPrefix = "as_";
constexpr int kMaxRestoreTries = 3;

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

std::string CardProjectPath() { return os::storage::JoinPath(storage::SessionDirectory(), "autosave.ccproj"); }
std::string CardInfoPath() { return os::storage::JoinPath(storage::SessionDirectory(), "autosave.txt"); }
std::string CardRestorePath() { return os::storage::JoinPath(storage::SessionDirectory(), "restore.txt"); }

}  // namespace

bool SessionStore::SaveCard(const SessionInfo& info, const ProjectData& project)
{
  // The project first, then the text that names its generation: cut in between, the start finds the
  // text of the one before beside a newer project, which reads as that one.
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

bool SessionStore::LoadCard(SessionInfo& info, ProjectData& project)
{
  return ReadCardInfo(info) && LoadProjectFileAt(CardProjectPath(), project) == ProjectResult::Ok;
}

bool SessionStore::InFlash()
{
  const auto keys = os::storage::ListInternalRecords();
  return std::any_of(keys.begin(), keys.end(), [](const std::string& key) { return key.rfind(kFlashPrefix, 0) == 0; });
}

bool SessionStore::LoadFlash(SessionInfo& info, ProjectData& project)
{
  std::string bytes;
  const auto read = [&](std::size_t section) {
    bytes.clear();
    return os::storage::ReadInternalRecord(SectionKey(section), bytes) && !bytes.empty();
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

void SessionStore::EraseFlash()
{
  for (const std::string& key : os::storage::ListInternalRecords())
    if (key.rfind(kFlashPrefix, 0) == 0) (void)os::storage::EraseInternalRecord(key);
}

bool SessionStore::BeginRestore()
{
  // Without a card there is nothing to count on (and nothing to restore but an older firmware's flash
  // copy, which comes back uncounted).
  if (!os::storage::CardMounted()) return true;
  std::string value;
  const int tries = os::storage::ReadFileWithBackup(CardRestorePath(), value, 64) ? std::atoi(value.c_str()) : 0;
  if (tries >= kMaxRestoreTries)
  {
    FinishRestore();
    return false;
  }
  (void)os::storage::WriteFileAtomic(CardRestorePath(), std::to_string(tries + 1) + "\n");
  return true;
}

void SessionStore::FinishRestore()
{
  if (os::storage::CardMounted()) (void)os::storage::WriteFileAtomic(CardRestorePath(), "0\n");
}

}  // namespace synth

#include "Applications/Synth/Runtime/ProjectStorage.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <new>
#include <set>
#include <sys/stat.h>
#if defined(ESP_PLATFORM)
#include <dirent.h>
#else
#include <filesystem>
#endif

#include "Devices/Device.h"

namespace synth {
namespace {

constexpr std::size_t kMaxProjectBytes = 1024 * 1024;

std::string ProjectPath(const std::string& name) {
  return Device::Storage::ProjectDirectoryPath() + "/" + name + ".ccproj";
}

bool Exists(const std::string& path) {
  struct stat info{};
  return stat(path.c_str(), &info) == 0;
}

bool ValidText(const std::string& text, std::size_t maximum) {
  return text.size() <= maximum && std::all_of(text.begin(), text.end(), [](unsigned char c) { return c >= 32 && c < 127; });
}

bool IsProjectEngineKind(int kind) {
  switch (kind) {
    case static_cast<int>(SynthEngineKind::Dx7):
    case static_cast<int>(SynthEngineKind::Juno):
    case static_cast<int>(SynthEngineKind::Sampler):
    case static_cast<int>(SynthEngineKind::Drums):
      return true;
    default:
      return false;
  }
}

bool ValidMetadata(const ProjectData& project) {
  if (!project.sequence || !SynthSequencer::ValidateState(*project.sequence) || project.activeTrack >= kSynthTrackCount ||
      project.octave < -3 || project.octave > 3 || project.root < 0 || project.root > 11 || project.scale >= 8 ||
      project.layout < 0 || project.layout > 2 || project.ledBrightness < 0 || project.ledBrightness > 15) return false;
  for (std::size_t track = 0; track < kSynthTrackCount; ++track) {
    const auto& state = project.engines[track];
    if (!IsProjectEngineKind(static_cast<int>(project.kinds[track])) ||
        state.basePresetId.empty() || !ValidText(state.basePresetId, 128) || !ValidText(state.presetId, 128) ||
        !ValidText(state.presetName, 64) || state.values.size() > 128) return false;
    const auto& sample = project.samples[track];
    if (!sample.empty() && (project.kinds[track] != SynthEngineKind::Sampler || !ValidText(sample, 96) ||
        sample == "." || sample == ".." || sample.find_first_of("/\\:\r\n") != std::string::npos)) return false;
    for (const auto& parameter : state.values)
      if (parameter.first.empty() || !ValidText(parameter.first, 64)) return false;
  }
  return true;
}

bool ReadExpected(std::istream& file, const char* expected) {
  char token[32]{};
  return static_cast<bool>(file >> std::setw(sizeof(token)) >> token) && std::string(token) == expected;
}

// Bound fields before allocation, including corrupt files on constrained firmware heaps.
bool ReadQuoted(std::istream& file, std::string& value, std::size_t maximum) {
  file >> std::ws;
  if (file.get() != '"') return false;
  value.clear();
  for (;;) {
    int c = file.get();
    if (c == '"') return true;
    if (c == '\\') c = file.get();
    if (c < 32 || c >= 127 || value.size() >= maximum) return false;
    value.push_back(static_cast<char>(c));
  }
}

}  // namespace

bool IsProjectNameValid(const std::string& name) {
  if (name.empty() || name.size() > 24 || name.front() == ' ' || name.back() == ' ') return false;
  for (const unsigned char c : name)
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-')) return false;
  std::string upper = name;
  std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
      (upper.size() == 4 && (upper.compare(0, 3, "COM") == 0 || upper.compare(0, 3, "LPT") == 0) && upper[3] >= '1' && upper[3] <= '9')) return false;
  return true;
}

std::vector<std::string> ListProjects() {
  std::vector<std::string> result;
  const std::string directory = Device::Storage::ProjectDirectoryPath();
  auto add = [&](const std::string& filename) {
    std::string candidate = filename;
    if (candidate.size() > 11 && candidate.substr(candidate.size() - 11) == ".ccproj.bak") candidate.resize(candidate.size() - 4);
    if (candidate.size() > 7 && candidate.substr(candidate.size() - 7) == ".ccproj") {
      const std::string name = candidate.substr(0, candidate.size() - 7);
      if (IsProjectNameValid(name)) result.push_back(name);
    }
  };
#if defined(ESP_PLATFORM)
  if (DIR* dir = opendir(directory.c_str())) {
    while (auto* entry = readdir(dir)) add(entry->d_name);
    closedir(dir);
  }
#else
  std::error_code error;
  for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
    if (it->is_regular_file(error)) add(it->path().filename().string());
  }
#endif
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

ProjectResult SaveProjectFile(const std::string& name, const ProjectData& project, bool overwrite) {
  if (!IsProjectNameValid(name) || !ValidMetadata(project)) return ProjectResult::Invalid;
  if (!Device::Storage::EnsureDirectoryLayout()) return ProjectResult::StorageError;
  const std::string path = ProjectPath(name);
  const bool existing = Exists(path);
  if ((existing || Exists(path + ".bak")) && !overwrite) return ProjectResult::Exists;
  const std::string temporary = path + ".tmp";
  const std::string backup = path + ".bak";
  std::ofstream file(temporary, std::ios::trunc);
  if (!file) return ProjectResult::StorageError;
  file << "CCOS_PROJECT 1\nBUILD ComputeCardOS 1\nUI " << project.activeTrack << ' ' << project.octave << ' '
       << project.root << ' ' << project.scale << ' ' << project.layout << ' ' << project.ledBrightness << '\n';
  for (std::size_t track = 0; track < kSynthTrackCount; ++track) {
    const auto& state = project.engines[track];
    file << "TRACK " << track << ' ' << static_cast<int>(project.kinds[track]) << ' ' << std::quoted(state.basePresetId) << ' '
         << std::quoted(state.presetId) << ' ' << std::quoted(state.presetName) << ' ' << std::quoted(project.samples[track]) << '\n';
    file << "PARAM " << state.values.size() << '\n';
    for (const auto& parameter : state.values) file << std::quoted(parameter.first) << ' ' << parameter.second << '\n';
  }
  file << "SEQ " << project.sequence->bpm << '\n';
  for (const auto& track : project.sequence->tracks) {
    file << "T " << track.patternCount << ' ' << track.activePattern << ' ' << static_cast<int>(track.defaultVelocity) << ' '
         << track.defaultLengthPulses << ' ' << track.enabled << ' ' << track.muted << '\n';
    std::size_t count = 0;
    for (std::size_t p = 0; p < track.patternCount; ++p)
      for (const auto& step : track.patterns[p].steps) for (const auto& note : step.notes) count += note.active;
    file << "NOTES " << count << '\n';
    for (std::size_t p = 0; p < track.patternCount; ++p)
      for (std::size_t s = 0; s < kSequencerStepCount; ++s)
        for (const auto& note : track.patterns[p].steps[s].notes)
          if (note.active) file << p << ' ' << s << ' ' << static_cast<int>(note.note) << ' ' << static_cast<int>(note.velocity)
                               << ' ' << note.lengthPulses << ' ' << static_cast<int>(note.microstep) << '\n';
  }
  file << "END\n";
  file.flush();
  const bool written = file.good();
  file.close();
  if (!written || file.fail()) { std::remove(temporary.c_str()); return ProjectResult::StorageError; }
  // FAT and Windows cannot replace an existing file with rename. Preserve a recoverable backup.
  if (existing) {
    if (Exists(backup) && std::remove(backup.c_str()) != 0) return ProjectResult::StorageError;
    if (std::rename(path.c_str(), backup.c_str()) != 0) return ProjectResult::StorageError;
  }
  if (std::rename(temporary.c_str(), path.c_str()) != 0) {
    if (existing) std::rename(backup.c_str(), path.c_str());
    return ProjectResult::StorageError;
  }
  if (existing) std::remove(backup.c_str());
  return ProjectResult::Ok;
}

ProjectResult LoadProjectFile(const std::string& name, ProjectData& destination) {
  if (!IsProjectNameValid(name)) return ProjectResult::Invalid;
  const std::string path = ProjectPath(name);
  // A backup is recoverable after interruption between the two rename operations.
  const std::string source = Exists(path) ? path : path + ".bak";
  struct stat info{};
  if (stat(source.c_str(), &info) != 0) return ProjectResult::StorageError;
  if (info.st_size <= 0 || static_cast<std::size_t>(info.st_size) > kMaxProjectBytes) return ProjectResult::Invalid;
  std::ifstream file(source);
  if (!file) return ProjectResult::StorageError;
  ProjectData project;
  project.sequence.reset(new (std::nothrow) SynthSequencer::ProjectState);
  if (!project.sequence) return ProjectResult::StorageError;
  int version = 0;
  if (!ReadExpected(file, "CCOS_PROJECT") || !(file >> version) || version != 1 || !ReadExpected(file, "BUILD") ||
      !ReadExpected(file, "ComputeCardOS") || !(file >> version) || version != 1 || !ReadExpected(file, "UI") ||
      !(file >> project.activeTrack >> project.octave >> project.root >> project.scale >> project.layout >> project.ledBrightness)) return ProjectResult::Invalid;
  for (std::size_t track = 0; track < kSynthTrackCount; ++track) {
    std::size_t index = 0, parameterCount = 0;
    int kind = 0;
    auto& state = project.engines[track];
    if (!ReadExpected(file, "TRACK") || !(file >> index >> kind) || !ReadQuoted(file, state.basePresetId, 128) ||
        !ReadQuoted(file, state.presetId, 128) || !ReadQuoted(file, state.presetName, 64) || !ReadQuoted(file, project.samples[track], 96) ||
        index != track || !IsProjectEngineKind(kind) ||
        !ReadExpected(file, "PARAM") || !(file >> parameterCount) || parameterCount > 128) return ProjectResult::Invalid;
    project.kinds[track] = static_cast<SynthEngineKind>(kind);
    for (std::size_t p = 0; p < parameterCount; ++p) {
      std::string id;
      int value = 0;
      if (!ReadQuoted(file, id, 64) || !(file >> value) || !state.values.emplace(id, value).second) return ProjectResult::Invalid;
    }
  }
  unsigned bpm = 0;
  if (!ReadExpected(file, "SEQ") || !(file >> bpm) || bpm < 30 || bpm > 300) return ProjectResult::Invalid;
  project.sequence->bpm = static_cast<std::uint16_t>(bpm);
  for (auto& track : project.sequence->tracks) {
    unsigned velocity = 0, length = 0, enabled = 0, muted = 0;
    std::size_t noteCount = 0;
    if (!ReadExpected(file, "T") || !(file >> track.patternCount >> track.activePattern >> velocity >> length >> enabled >> muted) ||
        track.patternCount < 1 || track.patternCount > kSequencerMaxPatternCount || track.activePattern >= track.patternCount ||
        velocity < 1 || velocity > 127 || length < 1 || length > kSequencerStepCount * kSequencerPulsesPerStep || enabled > 1 || muted > 1 ||
        !ReadExpected(file, "NOTES") || !(file >> noteCount) || noteCount > track.patternCount * kSequencerStepCount * 8) return ProjectResult::Invalid;
    track.defaultVelocity = static_cast<std::uint8_t>(velocity);
    track.defaultLengthPulses = static_cast<std::uint16_t>(length);
    track.enabled = enabled != 0;
    track.muted = muted != 0;
    for (std::size_t n = 0; n < noteCount; ++n) {
      std::size_t p = 0, s = 0;
      unsigned pitch = 0, microstep = 0;
      if (!(file >> p >> s >> pitch >> velocity >> length >> microstep) || p >= track.patternCount || s >= kSequencerStepCount ||
          pitch > 127 || velocity < 1 || velocity > 127 || length < 1 || length > 384 || microstep >= kSequencerMicrostepCount) return ProjectResult::Invalid;
      if (!track.patterns.Ensure(p)) return ProjectResult::StorageError;
      auto& notes = track.patterns[p].steps[s].notes;
      auto slot = std::find_if(notes.begin(), notes.end(), [](const auto& note) { return !note.active; });
      if (slot == notes.end()) return ProjectResult::Invalid;
      *slot = {static_cast<std::uint8_t>(pitch), static_cast<std::uint8_t>(velocity), static_cast<std::uint16_t>(length), static_cast<std::uint8_t>(microstep), true};
    }
  }
  if (!ReadExpected(file, "END") || !ValidMetadata(project)) return ProjectResult::Invalid;
  file >> std::ws;
  if (!file.eof()) return ProjectResult::Invalid;
  destination = std::move(project);
  return ProjectResult::Ok;
}

}  // namespace synth

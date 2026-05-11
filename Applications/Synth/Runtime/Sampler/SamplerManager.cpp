#include "Applications/Synth/Runtime/Sampler/SamplerManager.h"

#include "Devices/Device.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <limits>
#include <string>

namespace synth {

namespace {

constexpr std::uint32_t kMaxRecordSeconds = 5;
constexpr std::size_t kInputReadFrames = 512;
constexpr std::uint32_t kRecordingWaveformWindowMs = 750;

std::string PhaseStatus(SamplerTrackPhase phase)
{
  switch (phase)
  {
    case SamplerTrackPhase::Ready:
      return "READY";
    case SamplerTrackPhase::Recording:
      return "RECORDING";
    case SamplerTrackPhase::DraftTrim:
      return "DRAFT TRIM";
    case SamplerTrackPhase::Naming:
      return "NAME DRAFT";
    case SamplerTrackPhase::Browser:
      return "BROWSER";
    case SamplerTrackPhase::Error:
      return "ERROR";
    case SamplerTrackPhase::Empty:
    default:
      return "EMPTY";
  }
}

std::string SanitizedSampleName(std::string_view name)
{
  std::string sanitized;
  sanitized.reserve(name.size());
  for (char ch : name)
  {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (std::isalnum(value) || ch == '_' || ch == '-')
    {
      sanitized.push_back(ch);
    }
    else if (ch == ' ')
    {
      sanitized.push_back('_');
    }
  }

  if (sanitized.empty())
  {
    return "Sample";
  }
  return sanitized;
}

std::string JoinPath(const std::string& left, const std::string& right)
{
  if (left.empty())
  {
    return right;
  }

  const char last = left.back();
  if (last == '/' || last == '\\')
  {
    return left + right;
  }
  return left + "/" + right;
}

bool FileExists(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  return file.good();
}

bool RemoveFileIfPresent(const std::string& path)
{
  if (!FileExists(path))
  {
    return true;
  }
  return std::remove(path.c_str()) == 0;
}

bool RenameFileIfPossible(const std::string& sourcePath, const std::string& destinationPath)
{
  if (sourcePath == destinationPath)
  {
    return FileExists(sourcePath);
  }

  if (!FileExists(sourcePath) || FileExists(destinationPath))
  {
    return false;
  }

  return std::rename(sourcePath.c_str(), destinationPath.c_str()) == 0;
}

std::string FileStem(std::string_view path)
{
  const std::size_t slash = path.find_last_of("/\\");
  const std::string_view name = slash == std::string_view::npos ? path : path.substr(slash + 1);
  const std::size_t dot = name.find_last_of('.');
  return std::string(dot == std::string_view::npos ? name : name.substr(0, dot));
}

void WriteU16(std::ofstream& file, std::uint16_t value)
{
  const char bytes[2] = {
      static_cast<char>(value & 0xFF),
      static_cast<char>((value >> 8) & 0xFF),
  };
  file.write(bytes, sizeof(bytes));
}

void WriteU32(std::ofstream& file, std::uint32_t value)
{
  const char bytes[4] = {
      static_cast<char>(value & 0xFF),
      static_cast<char>((value >> 8) & 0xFF),
      static_cast<char>((value >> 16) & 0xFF),
      static_cast<char>((value >> 24) & 0xFF),
  };
  file.write(bytes, sizeof(bytes));
}

std::uint16_t ReadU16(std::ifstream& file)
{
  std::array<unsigned char, 2> bytes{};
  file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<std::uint16_t>(bytes[0]) |
         (static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t ReadU32(std::ifstream& file)
{
  std::array<unsigned char, 4> bytes{};
  file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8) |
         (static_cast<std::uint32_t>(bytes[2]) << 16) |
         (static_cast<std::uint32_t>(bytes[3]) << 24);
}

bool ReadMono16Wav(const std::string& path, std::vector<std::int16_t>& frames, std::uint32_t& sampleRate)
{
  std::ifstream file(path, std::ios::binary);
  if (!file)
  {
    return false;
  }

  std::array<char, 4> chunkId{};
  file.read(chunkId.data(), 4);
  if (std::string_view(chunkId.data(), 4) != "RIFF")
  {
    return false;
  }

  (void)ReadU32(file);
  file.read(chunkId.data(), 4);
  if (std::string_view(chunkId.data(), 4) != "WAVE")
  {
    return false;
  }

  bool foundFormat = false;
  bool foundData = false;
  std::uint16_t audioFormat = 0;
  std::uint16_t channelCount = 0;
  std::uint16_t bitsPerSample = 0;
  sampleRate = 0;
  frames.clear();

  while (file && (!foundFormat || !foundData))
  {
    file.read(chunkId.data(), 4);
    if (!file)
    {
      break;
    }

    const std::uint32_t chunkSize = ReadU32(file);
    if (std::string_view(chunkId.data(), 4) == "fmt ")
    {
      audioFormat = ReadU16(file);
      channelCount = ReadU16(file);
      sampleRate = ReadU32(file);
      (void)ReadU32(file);
      (void)ReadU16(file);
      bitsPerSample = ReadU16(file);
      if (chunkSize > 16)
      {
        file.seekg(static_cast<std::streamoff>(chunkSize - 16), std::ios::cur);
      }
      foundFormat = true;
    }
    else if (std::string_view(chunkId.data(), 4) == "data")
    {
      frames.resize(chunkSize / sizeof(std::int16_t));
      file.read(reinterpret_cast<char*>(frames.data()), static_cast<std::streamsize>(chunkSize));
      foundData = file.good() || file.eof();
    }
    else
    {
      file.seekg(static_cast<std::streamoff>(chunkSize), std::ios::cur);
    }

    if ((chunkSize & 1u) != 0u)
    {
      file.seekg(1, std::ios::cur);
    }
  }

  return foundFormat && foundData && audioFormat == 1 && channelCount == 1 && bitsPerSample == 16 && sampleRate > 0;
}

bool WriteMono16Wav(const std::string& path,
                    const std::vector<std::int16_t>& frames,
                    std::uint32_t sampleRate)
{
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file)
  {
    return false;
  }

  const std::uint32_t dataBytes = static_cast<std::uint32_t>(frames.size() * sizeof(std::int16_t));
  const std::uint32_t riffSize = 36 + dataBytes;
  file.write("RIFF", 4);
  WriteU32(file, riffSize);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  WriteU32(file, 16);
  WriteU16(file, 1);
  WriteU16(file, 1);
  WriteU32(file, sampleRate);
  WriteU32(file, sampleRate * sizeof(std::int16_t));
  WriteU16(file, sizeof(std::int16_t));
  WriteU16(file, 16);
  file.write("data", 4);
  WriteU32(file, dataBytes);
  file.write(reinterpret_cast<const char*>(frames.data()), static_cast<std::streamsize>(dataBytes));
  return file.good();
}

float PeakLevel(const std::int16_t* frames, std::size_t frameCount)
{
  std::int32_t peak = 0;
  for (std::size_t index = 0; index < frameCount; ++index)
  {
    peak = std::max<std::int32_t>(peak, std::abs(static_cast<int>(frames[index])));
  }
  return static_cast<float>(peak) / static_cast<float>(std::numeric_limits<std::int16_t>::max());
}

void BuildWaveform(std::array<float, kSamplerWaveformBinCount>& waveform, const std::vector<std::int16_t>& frames)
{
  waveform.fill(0.0f);
  if (frames.empty())
  {
    return;
  }

  for (std::size_t bin = 0; bin < waveform.size(); ++bin)
  {
    const std::size_t begin = bin * frames.size() / waveform.size();
    const std::size_t end = std::max<std::size_t>(begin + 1, (bin + 1) * frames.size() / waveform.size());
    std::int32_t peak = 0;
    for (std::size_t index = begin; index < end && index < frames.size(); ++index)
    {
      peak = std::max<std::int32_t>(peak, std::abs(static_cast<int>(frames[index])));
    }
    waveform[bin] = static_cast<float>(peak) / static_cast<float>(std::numeric_limits<std::int16_t>::max());
  }
}

void BuildRecordingWaveform(std::array<float, kSamplerWaveformBinCount>& waveform,
                            const std::vector<std::int16_t>& frames,
                            std::uint32_t sampleRate)
{
  if (frames.empty() || sampleRate == 0)
  {
    BuildWaveform(waveform, frames);
    return;
  }

  const std::size_t windowFrames = std::max<std::size_t>(kInputReadFrames,
                                                          (static_cast<std::size_t>(sampleRate) * kRecordingWaveformWindowMs) / 1000);
  if (frames.size() <= windowFrames)
  {
    BuildWaveform(waveform, frames);
    return;
  }

  waveform.fill(0.0f);
  const std::size_t frameStart = frames.size() - windowFrames;
  const std::size_t frameCount = frames.size() - frameStart;
  for (std::size_t bin = 0; bin < waveform.size(); ++bin)
  {
    const std::size_t begin = frameStart + (bin * frameCount) / waveform.size();
    const std::size_t end = std::max<std::size_t>(begin + 1, frameStart + ((bin + 1) * frameCount) / waveform.size());
    double mean = 0.0;
    std::size_t sampleCount = 0;
    for (std::size_t index = begin; index < end && index < frames.size(); ++index)
    {
      mean += static_cast<double>(frames[index]);
      ++sampleCount;
    }

    if (sampleCount == 0)
    {
      waveform[bin] = 0.0f;
      continue;
    }

    mean /= static_cast<double>(sampleCount);
    double energy = 0.0;
    for (std::size_t index = begin; index < end && index < frames.size(); ++index)
    {
      const double centered = static_cast<double>(frames[index]) - mean;
      energy += centered * centered;
    }

    const double rms = std::sqrt(energy / static_cast<double>(sampleCount));
    waveform[bin] = static_cast<float>(rms / static_cast<double>(std::numeric_limits<std::int16_t>::max()));
  }
}

}  // namespace

void SamplerManager::OnTrackEngineChanged(std::size_t trackIndex, SynthEngineKind kind)
{
  if (trackIndex >= tracks_.size())
  {
    return;
  }

  TrackState& state = tracks_[trackIndex];
  state.isSampler = kind == SynthEngineKind::Sampler;
  if (!state.isSampler)
  {
    if (activeRecordingTrack_ == static_cast<int>(trackIndex))
    {
      Device::Audio::StopInput();
      activeRecordingTrack_ = -1;
    }
    StopPreview(trackIndex, state);
    audio::SetSamplerFrames(trackIndex, {}, audio::kSampleRate);
    state = TrackState{};
    return;
  }

  state.ui.phase = state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  SyncAudioSample(trackIndex, state);
}

void SamplerManager::Tick()
{
  if (activeRecordingTrack_ >= 0 && activeRecordingTrack_ < static_cast<int>(tracks_.size()))
  {
    TrackState& recordingState = tracks_[static_cast<std::size_t>(activeRecordingTrack_)];
    if (recordingState.ui.phase == SamplerTrackPhase::Recording)
    {
      PumpInput(recordingState);
    }
  }

  const auto now = std::chrono::steady_clock::now();
  for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex)
  {
    TrackState& state = tracks_[trackIndex];
    if (!state.ui.previewing)
    {
      continue;
    }

    const auto& frames = ActiveFrames(state);
    const std::uint32_t totalFrames = static_cast<std::uint32_t>(frames.size());
    const std::uint32_t previewEndFrame = std::min<std::uint32_t>(std::max(state.ui.trimEndFrame, state.ui.trimStartFrame + 1), totalFrames);
    if (totalFrames == 0 || state.ui.previewCursorFrame >= previewEndFrame)
    {
      StopPreview(trackIndex, state);
      continue;
    }

    if (state.previewTickAt == std::chrono::steady_clock::time_point{})
    {
      state.previewTickAt = now;
      continue;
    }

    const auto elapsed = now - state.previewTickAt;
    const auto elapsedFrames = static_cast<std::uint32_t>(
        (std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() * state.ui.sampleRate) / 1000000);
    if (elapsedFrames == 0)
    {
      continue;
    }

    state.previewTickAt = now;
    state.ui.previewCursorFrame = std::min(previewEndFrame, state.ui.previewCursorFrame + elapsedFrames);
    if (state.ui.previewCursorFrame >= previewEndFrame)
    {
      StopPreview(trackIndex, state);
    }
  }
}

bool SamplerManager::BeginRecord(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex) || activeRecordingTrack_ >= 0)
  {
    return false;
  }

  TrackState& state = tracks_[trackIndex];
  StopPreview(trackIndex, state);
  if (Device::Audio::StartInput() != Device::Audio::InputStatus::Ready)
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "INPUT ERR";
    return false;
  }
  const Device::Audio::InputStreamInfo info = Device::Audio::InputInfo();

  state.draftFrames.clear();
  state.draftFrames.reserve(static_cast<std::size_t>(info.sampleRate) * kMaxRecordSeconds);
  state.ui.sampleRate = info.sampleRate;
  state.ui.frameCount = 0;
  state.ui.recordMaxFrameCount = static_cast<std::uint32_t>(static_cast<std::size_t>(info.sampleRate) * kMaxRecordSeconds);
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = 0;
  state.ui.inputLevel = 0.0f;
  state.ui.waveform.fill(0.0f);
  state.ui.recordProgress = 0.0f;
  state.ui.previewCursorFrame = 0;
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.overwritePending = false;
  state.ui.previewing = false;
  state.ui.phase = SamplerTrackPhase::Recording;
  state.ui.recording = true;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  activeRecordingTrack_ = static_cast<int>(trackIndex);
  return true;
}

bool SamplerManager::StopRecord(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex) || activeRecordingTrack_ != static_cast<int>(trackIndex))
  {
    return false;
  }

  TrackState& state = tracks_[trackIndex];
  PumpInput(state);
  Device::Audio::StopInput();
  activeRecordingTrack_ = -1;
  state.ui.recording = false;

  if (state.draftFrames.empty())
  {
    state.ui.phase = state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty;
    state.ui.statusText = "NO INPUT";
    return false;
  }

  FinishDraft(state);
  return true;
}

void SamplerManager::CancelRecord(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return;
  }

  if (activeRecordingTrack_ == static_cast<int>(trackIndex))
  {
    Device::Audio::StopInput();
    activeRecordingTrack_ = -1;
  }
  DiscardDraftTake(trackIndex);
}

void SamplerManager::DiscardDraftTake(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return;
  }

  TrackState& state = tracks_[trackIndex];
  StopPreview(trackIndex, state);
  state.draftFrames.clear();
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.recording = false;
  state.ui.recordProgress = 0.0f;
  state.ui.inputLevel = 0.0f;
  state.ui.recordMaxFrameCount = 0;
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.hasSample ? state.ui.frameCount : 0;
  state.ui.previewCursorFrame = state.ui.trimStartFrame;
  BuildWaveform(state.ui.waveform, state.currentFrames);
  state.ui.phase = state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  SyncAudioSample(trackIndex, state);
}

void SamplerManager::SetDraftTrimWindow(std::size_t trackIndex, int start127, int end127)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return;
  }

  TrackState& state = tracks_[trackIndex];
  const std::uint32_t totalFrames = static_cast<std::uint32_t>(state.draftFrames.empty() ? state.currentFrames.size() : state.draftFrames.size());
  if (totalFrames == 0)
  {
    return;
  }

  const int clampedStart = std::clamp(start127, 0, 126);
  const int clampedEnd = std::clamp(end127, clampedStart + 1, 127);
  state.ui.trimStartFrame = static_cast<std::uint32_t>((static_cast<std::uint64_t>(totalFrames) * clampedStart) / 127);
  state.ui.trimEndFrame = static_cast<std::uint32_t>((static_cast<std::uint64_t>(totalFrames) * clampedEnd) / 127);
}

void SamplerManager::SetDraftLoopEnabled(std::size_t, bool)
{
}

void SamplerManager::EnterDraftNaming(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return;
  }

  TrackState& state = tracks_[trackIndex];
  if (!state.ui.hasDraft)
  {
    return;
  }

  state.ui.phase = SamplerTrackPhase::Naming;
  state.ui.needsName = true;
  state.ui.statusText = PhaseStatus(state.ui.phase);
}

bool SamplerManager::CommitDraftName(std::size_t trackIndex, std::string_view name)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return false;
  }
  return SaveDraftAs(tracks_[trackIndex], name, false);
}

bool SamplerManager::ConfirmOverwriteDraft(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return false;
  }
  return SaveDraftAs(tracks_[trackIndex], tracks_[trackIndex].ui.draftName, true);
}

void SamplerManager::CancelNameEntry(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return;
  }

  TrackState& state = tracks_[trackIndex];
  if (state.ui.hasDraft)
  {
    state.ui.phase = SamplerTrackPhase::DraftTrim;
    state.ui.needsName = false;
    state.ui.overwritePending = false;
    state.ui.deleteConfirm = false;
    StopPreview(trackIndex, state);
    state.ui.statusText = PhaseStatus(state.ui.phase);
    return;
  }

  if (state.ui.deleteConfirm)
  {
    state.ui.deleteConfirm = false;
    StopPreview(trackIndex, state);
    state.ui.phase = state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty;
    state.ui.statusText = PhaseStatus(state.ui.phase);
  }
}

bool SamplerManager::PreviewCurrentSample(std::size_t trackIndex, std::uint8_t rootNote)
{
  if (!IsValidSamplerTrack(trackIndex) || (!tracks_[trackIndex].ui.hasDraft && !tracks_[trackIndex].ui.hasSample))
  {
    return false;
  }

  TrackState& state = tracks_[trackIndex];
  const auto& frames = ActiveFrames(state);
  if (frames.empty())
  {
    return false;
  }

  StopPreview(trackIndex, state);
  SyncAudioSample(trackIndex, state);
  state.ui.previewing = true;
  state.ui.previewCursorFrame = state.ui.trimStartFrame;
  state.previewNote = rootNote;
  state.previewTickAt = std::chrono::steady_clock::now();
  audio::NoteOn(trackIndex, rootNote, 127);
  return true;
}

bool SamplerManager::SelectBrowserSample(std::size_t trackIndex, std::string_view id)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return false;
  }

  TrackState& state = tracks_[trackIndex];
  const std::string idString = std::string(id);
  if (idString.empty())
  {
    state.ui.phase = SamplerTrackPhase::Browser;
    state.ui.statusText = PhaseStatus(SamplerTrackPhase::Browser);
    return false;
  }

  const std::string fileName = idString.size() >= 4 && idString.substr(idString.size() - 4) == ".wav"
      ? idString
      : idString + ".wav";
  std::vector<std::int16_t> loadedFrames;
  std::uint32_t sampleRate = 0;
  if (!ReadMono16Wav(JoinPath(Device::Storage::SampleDirectoryPath(), fileName), loadedFrames, sampleRate))
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "LOAD ERR";
    return false;
  }

  StopPreview(trackIndex, state);
  state.currentFrames = std::move(loadedFrames);
  state.draftFrames.clear();
  state.ui.sampleName = FileStem(fileName);
  state.ui.hasSample = !state.currentFrames.empty();
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.overwritePending = false;
  state.ui.deleteConfirm = false;
  state.ui.sampleRate = sampleRate;
  state.ui.frameCount = static_cast<std::uint32_t>(state.currentFrames.size());
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.frameCount;
  state.ui.previewCursorFrame = 0;
  BuildWaveform(state.ui.waveform, state.currentFrames);
  state.ui.phase = state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  SyncAudioSample(trackIndex, state);
  return state.ui.hasSample;
}

bool SamplerManager::DeleteBrowserSample(std::size_t trackIndex, std::string_view id)
{
  if (!IsValidSamplerTrack(trackIndex) || id.empty())
  {
    return false;
  }

  const std::string idString(id);
  const std::string fileName = idString.size() >= 4 && idString.substr(idString.size() - 4) == ".wav"
      ? idString
      : idString + ".wav";
  return RemoveFileIfPresent(JoinPath(Device::Storage::SampleDirectoryPath(), fileName));
}

bool SamplerManager::RenameCurrentSample(std::size_t trackIndex, std::string_view newName)
{
  if (!IsValidSamplerTrack(trackIndex) || !tracks_[trackIndex].ui.hasSample)
  {
    return false;
  }

  TrackState& state = tracks_[trackIndex];
  const std::string currentName = SanitizedSampleName(state.ui.sampleName);
  const std::string renamedName = SanitizedSampleName(newName);
  if (renamedName == currentName)
  {
    state.ui.sampleName = renamedName;
    state.ui.statusText = PhaseStatus(state.ui.phase);
    return true;
  }

  const std::string sourcePath = JoinPath(Device::Storage::SampleDirectoryPath(), currentName + ".wav");
  const std::string destinationPath = JoinPath(Device::Storage::SampleDirectoryPath(), renamedName + ".wav");
  if (!RenameFileIfPossible(sourcePath, destinationPath))
  {
    state.ui.statusText = "REN ERR";
    return false;
  }

  state.ui.sampleName = renamedName;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  return true;
}

bool SamplerManager::DeleteCurrentSample(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return false;
  }

  TrackState& state = tracks_[trackIndex];
  if (!state.ui.hasSample)
  {
    return false;
  }

  if (!state.ui.deleteConfirm)
  {
    StopPreview(trackIndex, state);
    state.ui.deleteConfirm = true;
    state.ui.statusText = "DELETE?";
    return false;
  }

  StopPreview(trackIndex, state);
  const std::string filePath = JoinPath(Device::Storage::SampleDirectoryPath(), SanitizedSampleName(state.ui.sampleName) + ".wav");
  if (!RemoveFileIfPresent(filePath))
  {
    state.ui.statusText = "DEL ERR";
    state.ui.deleteConfirm = false;
    return false;
  }

  state.currentFrames.clear();
  state.ui.sampleName = "EMPTY";
  state.ui.hasSample = false;
  state.ui.deleteConfirm = false;
  state.ui.frameCount = 0;
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = 0;
  state.ui.previewCursorFrame = 0;
  state.ui.waveform.fill(0.0f);
  state.ui.phase = SamplerTrackPhase::Empty;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  SyncAudioSample(trackIndex, state);
  return true;
}

SamplerTrackUiState SamplerManager::BuildUiState(std::size_t trackIndex) const
{
  if (trackIndex >= tracks_.size())
  {
    return {};
  }
  return tracks_[trackIndex].ui;
}

bool SamplerManager::IsValidSamplerTrack(std::size_t trackIndex) const
{
  return trackIndex < tracks_.size() && tracks_[trackIndex].isSampler;
}

const std::vector<std::int16_t>& SamplerManager::ActiveFrames(const TrackState& state)
{
  return state.ui.hasDraft ? state.draftFrames : state.currentFrames;
}

void SamplerManager::StopPreview(std::size_t trackIndex, TrackState& state)
{
  if (!state.ui.previewing)
  {
    return;
  }

  audio::NoteOff(trackIndex, state.previewNote);
  state.ui.previewing = false;
  state.previewTickAt = std::chrono::steady_clock::time_point{};
}

void SamplerManager::SyncAudioSample(std::size_t trackIndex, const TrackState& state)
{
  if (!state.isSampler)
  {
    audio::SetSamplerFrames(trackIndex, {}, audio::kSampleRate);
    return;
  }

  const auto& frames = ActiveFrames(state);
  audio::SetSamplerFrames(trackIndex, frames, state.ui.sampleRate);
}

void SamplerManager::PumpInput(TrackState& state)
{
  const std::size_t maxFrames = static_cast<std::size_t>(state.ui.sampleRate) * kMaxRecordSeconds;
  if (state.draftFrames.size() >= maxFrames)
  {
    Device::Audio::StopInput();
    activeRecordingTrack_ = -1;
    FinishDraft(state);
    return;
  }

  std::array<std::int16_t, kInputReadFrames> frames{};
  std::size_t framesRead = 0;
  const std::size_t requestFrames = std::min<std::size_t>(frames.size(), maxFrames - state.draftFrames.size());
  if (Device::Audio::ReadInputFrames(frames.data(), requestFrames, framesRead, 0) != Device::Audio::InputStatus::Ready)
  {
    return;
  }

  state.ui.inputLevel = PeakLevel(frames.data(), framesRead);
  state.draftFrames.insert(state.draftFrames.end(), frames.begin(), frames.begin() + static_cast<std::ptrdiff_t>(framesRead));
  state.ui.frameCount = static_cast<std::uint32_t>(state.draftFrames.size());
  BuildRecordingWaveform(state.ui.waveform, state.draftFrames, state.ui.sampleRate);
  state.ui.recordProgress = maxFrames == 0 ? 0.0f : static_cast<float>(state.draftFrames.size()) / static_cast<float>(maxFrames);
}

void SamplerManager::FinishDraft(TrackState& state)
{
  state.ui.hasDraft = !state.draftFrames.empty();
  state.ui.recording = false;
  state.ui.phase = state.ui.hasDraft ? SamplerTrackPhase::DraftTrim : (state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty);
  state.ui.statusText = PhaseStatus(state.ui.phase);
  state.ui.frameCount = static_cast<std::uint32_t>(state.draftFrames.size());
  state.ui.recordMaxFrameCount = 0;
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.frameCount;
  state.ui.previewCursorFrame = state.ui.trimStartFrame;
  BuildWaveform(state.ui.waveform, state.draftFrames);
  state.ui.previewing = false;
  state.ui.recordProgress = 0.0f;
}

bool SamplerManager::SaveDraftAs(TrackState& state, std::string_view name, bool overwrite)
{
  if (!state.ui.hasDraft || state.draftFrames.empty())
  {
    return false;
  }

  const std::string sanitizedName = SanitizedSampleName(name);
  const std::string filePath = JoinPath(Device::Storage::SampleDirectoryPath(), sanitizedName + ".wav");
  state.ui.draftName = sanitizedName;
  if (!overwrite && FileExists(filePath))
  {
    state.ui.overwritePending = true;
    state.ui.phase = SamplerTrackPhase::Naming;
    state.ui.statusText = "NAME EXISTS";
    return false;
  }

  const std::uint32_t totalFrames = static_cast<std::uint32_t>(state.draftFrames.size());
  const std::uint32_t startFrame = std::min<std::uint32_t>(state.ui.trimStartFrame, totalFrames - 1);
  const std::uint32_t endFrame = std::clamp<std::uint32_t>(state.ui.trimEndFrame, startFrame + 1, totalFrames);
  const std::vector<std::int16_t> savedFrames(state.draftFrames.begin() + static_cast<std::ptrdiff_t>(startFrame),
                                              state.draftFrames.begin() + static_cast<std::ptrdiff_t>(endFrame));

  if (!Device::Storage::EnsureDirectoryLayout() || !WriteMono16Wav(filePath, savedFrames, state.ui.sampleRate))
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "SAVE ERR";
    return false;
  }

  state.currentFrames = savedFrames;
  state.draftFrames.clear();
  state.ui.sampleName = sanitizedName;
  state.ui.hasSample = true;
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.overwritePending = false;
  state.ui.phase = SamplerTrackPhase::Ready;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  state.ui.frameCount = static_cast<std::uint32_t>(state.currentFrames.size());
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.frameCount;
  state.ui.previewCursorFrame = state.ui.trimStartFrame;
  BuildWaveform(state.ui.waveform, state.currentFrames);
  state.ui.previewing = false;
  const std::size_t trackIndex = static_cast<std::size_t>(&state - tracks_.data());
  SyncAudioSample(trackIndex, state);
  return true;
}

}  // namespace synth
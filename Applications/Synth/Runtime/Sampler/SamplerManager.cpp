#include "Applications/Synth/Runtime/Sampler/SamplerManager.h"

#include "Devices/Device.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Applications/Synth/Runtime/Sampler/SamplerWav.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <limits>
#include <string>
#include <sys/stat.h>

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
  sanitized.reserve(std::min<std::size_t>(name.size(), 80));
  for (char ch : name)
  {
    if (sanitized.size() >= 80)
    {
      break;
    }
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

std::string SampleFileName(std::string_view id)
{
  if (id.empty() || id.size() > 96 || id.find_first_of("/\\:") != std::string_view::npos ||
      id.find('\0') != std::string_view::npos || id == "." || id == "..")
  {
    return {};
  }
  const bool hasExtension = id.size() >= 4 && id[id.size() - 4] == '.' &&
      std::tolower(static_cast<unsigned char>(id[id.size() - 3])) == 'w' &&
      std::tolower(static_cast<unsigned char>(id[id.size() - 2])) == 'a' &&
      std::tolower(static_cast<unsigned char>(id[id.size() - 1])) == 'v';
  return hasExtension ? std::string(id) : std::string(id) + ".wav";
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
  struct stat info{};
  return stat(path.c_str(), &info) == 0;
}

bool IsRegularFile(const std::string& path)
{
  struct stat info{};
  return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFMT) == S_IFREG;
}

bool SameSampleFile(std::string_view left, std::string_view right)
{
  // Firmware storage is FAT; keep reference handling consistent on host filesystems.
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
      [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); });
}

bool RemoveFileIfPresent(const std::string& path)
{
  struct stat info{};
  if (stat(path.c_str(), &info) != 0)
  {
    return errno == ENOENT;
  }
  return (info.st_mode & S_IFMT) == S_IFREG && std::remove(path.c_str()) == 0;
}

bool RenameFileIfPossible(const std::string& sourcePath, const std::string& destinationPath)
{
  if (sourcePath == destinationPath)
  {
    return IsRegularFile(sourcePath);
  }

  struct stat destination{};
  if (!IsRegularFile(sourcePath) || stat(destinationPath.c_str(), &destination) == 0 || errno != ENOENT)
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

float PeakLevel(const std::int16_t* frames, std::size_t frameCount)
{
  std::int32_t peak = 0;
  for (std::size_t index = 0; index < frameCount; ++index)
  {
    peak = std::max<std::int32_t>(peak, std::abs(static_cast<int>(frames[index])));
  }
  return static_cast<float>(peak) / static_cast<float>(std::numeric_limits<std::int16_t>::max());
}

void BuildWaveform(std::array<float, kSamplerWaveformBinCount>& waveform, const audio::SharedSampleBuffer& frames)
{
  waveform.fill(0.0f);
  if (!frames || frames->frameCount == 0)
  {
    return;
  }

  for (std::size_t bin = 0; bin < waveform.size(); ++bin)
  {
    const std::size_t begin = bin * frames->frameCount / waveform.size();
    const std::size_t end = std::max<std::size_t>(begin + 1, (bin + 1) * frames->frameCount / waveform.size());
    std::int32_t peak = 0;
    for (std::size_t index = begin; index < end && index < frames->frameCount; ++index)
    {
      peak = std::max<std::int32_t>(peak, std::abs(static_cast<int>(frames->frames[index])));
    }
    waveform[bin] = static_cast<float>(peak) / static_cast<float>(std::numeric_limits<std::int16_t>::max());
  }
}

void BuildRecordingWaveform(std::array<float, kSamplerWaveformBinCount>& waveform,
                            const audio::SharedSampleBuffer& frames,
                            std::uint32_t sampleRate)
{
  if ((!frames || frames->frameCount == 0) || sampleRate == 0)
  {
    BuildWaveform(waveform, frames);
    return;
  }

  const std::size_t windowFrames = std::max<std::size_t>(kInputReadFrames,
                                                          (static_cast<std::size_t>(sampleRate) * kRecordingWaveformWindowMs) / 1000);
  if (frames->frameCount <= windowFrames)
  {
    BuildWaveform(waveform, frames);
    return;
  }

  waveform.fill(0.0f);
  const std::size_t frameStart = frames->frameCount - windowFrames;
  const std::size_t frameCount = frames->frameCount - frameStart;
  for (std::size_t bin = 0; bin < waveform.size(); ++bin)
  {
    const std::size_t begin = frameStart + (bin * frameCount) / waveform.size();
    const std::size_t end = std::max<std::size_t>(begin + 1, frameStart + ((bin + 1) * frameCount) / waveform.size());
    double mean = 0.0;
    std::size_t sampleCount = 0;
    for (std::size_t index = begin; index < end && index < frames->frameCount; ++index)
    {
      mean += static_cast<double>(frames->frames[index]);
      ++sampleCount;
    }

    if (sampleCount == 0)
    {
      waveform[bin] = 0.0f;
      continue;
    }

    mean /= static_cast<double>(sampleCount);
    double energy = 0.0;
    for (std::size_t index = begin; index < end && index < frames->frameCount; ++index)
    {
      const double centered = static_cast<double>(frames->frames[index]) - mean;
      energy += centered * centered;
    }

    const double rms = std::sqrt(energy / static_cast<double>(sampleCount));
    waveform[bin] = static_cast<float>(rms / static_cast<double>(std::numeric_limits<std::int16_t>::max()));
  }
}

}  // namespace

SamplerManager::~SamplerManager()
{
  if (activeRecordingTrack_ >= 0)
  {
    Device::Audio::StopInput();
  }
}

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
    const std::uint32_t totalFrames = frames ? static_cast<std::uint32_t>(frames->frameCount) : 0;
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

bool SamplerManager::BeginRecord(std::size_t trackIndex, bool replaceDraft)
{
  if (!IsValidSamplerTrack(trackIndex) || activeRecordingTrack_ >= 0)
  {
    return false;
  }

  TrackState& state = tracks_[trackIndex];
  if (state.ui.hasDraft && !replaceDraft)
  {
    state.ui.statusText = "SAVE SAMPLE FIRST";
    return false;
  }
  StopPreview(trackIndex, state);
  const Device::Audio::InputStreamInfo info = Device::Audio::InputInfo();
  if (info.sampleRate < sampler::kMinSampleRate || info.sampleRate > sampler::kMaxSampleRate ||
      info.channelCount != 1 || info.bitsPerSample != 16)
  {
    state.ui.statusText = "INPUT FORMAT";
    return false;
  }
  std::size_t capacity = std::min<std::size_t>(info.sampleRate * kMaxRecordSeconds, sampler::kMaxSampleFrames);
  std::shared_ptr<audio::SampleBuffer> draft;
  for (; capacity >= info.sampleRate / 4; capacity /= 2)
  {
    draft = sampler::AllocateFrames(capacity);
    if (draft)
    {
      break;
    }
  }
  if (!draft)
  {
    state.ui.statusText = "NO MEMORY";
    return false;
  }
  if (Device::Audio::StartInput() != Device::Audio::InputStatus::Ready)
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "INPUT ERR";
    return false;
  }
  state.draftFrames = std::move(draft);
  state.draftFrames->frameCount = 0;
  state.ui.sampleRate = info.sampleRate;
  state.ui.frameCount = 0;
  state.ui.recordMaxFrameCount = static_cast<std::uint32_t>(capacity);
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = 0;
  state.ui.inputLevel = 0.0f;
  state.ui.waveform.fill(0.0f);
  state.ui.recordProgress = 0.0f;
  state.ui.previewCursorFrame = 0;
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.overwritePending = false;
  state.ui.deleteConfirm = false;
  state.pendingDeleteFileName.clear();
  state.ui.previewing = false;
  state.trimResetPending = true;
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
  if (state.ui.phase == SamplerTrackPhase::Recording)
  {
    PumpInput(state);
  }
  activeRecordingTrack_ = -1;
  state.ui.recording = false;
  if (state.ui.phase == SamplerTrackPhase::Error)
  {
    return false;
  }

  if (!state.draftFrames || state.draftFrames->frameCount == 0)
  {
    DiscardDraftTake(trackIndex);
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
  if (activeRecordingTrack_ == static_cast<int>(trackIndex))
  {
    Device::Audio::StopInput();
    activeRecordingTrack_ = -1;
  }
  StopPreview(trackIndex, state);
  state.draftFrames.reset();
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.overwritePending = false;
  state.ui.deleteConfirm = false;
  state.pendingDeleteFileName.clear();
  state.ui.recording = false;
  state.ui.recordProgress = 0.0f;
  state.ui.inputLevel = 0.0f;
  state.ui.recordMaxFrameCount = 0;
  state.ui.sampleRate = state.currentSampleRate;
  state.ui.frameCount = state.currentFrames ? static_cast<std::uint32_t>(state.currentFrames->frameCount) : 0;
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.hasSample ? state.ui.frameCount : 0;
  state.ui.previewCursorFrame = state.ui.trimStartFrame;
  BuildWaveform(state.ui.waveform, state.currentFrames);
  state.ui.phase = state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  state.trimResetPending = true;
  SyncAudioSample(trackIndex, state);
}

void SamplerManager::SetDraftTrimWindow(std::size_t trackIndex, int start127, int end127)
{
  if (!IsValidSamplerTrack(trackIndex))
  {
    return;
  }

  TrackState& state = tracks_[trackIndex];
  const auto frames = ActiveFrames(state);
  const std::uint32_t totalFrames = frames ? static_cast<std::uint32_t>(frames->frameCount) : 0;
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
  if (!IsValidSamplerTrack(trackIndex) || !tracks_[trackIndex].ui.overwritePending)
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
  state.pendingDeleteFileName.clear();
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
  if (!frames || frames->frameCount == 0)
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
  if (!CanReplaceSample(trackIndex))
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

  PreparedSample sample;
  if (!PrepareSample(id, sample))
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "LOAD ERR";
    return false;
  }

  return ApplySample(trackIndex, sample);
}

bool SamplerManager::PrepareSample(std::string_view id, PreparedSample& sample)
{
  PreparedSample prepared;
  prepared.fileName = SampleFileName(id);
  if (prepared.fileName.empty() || !sampler::ReadMono16Wav(
          JoinPath(Device::Storage::SampleDirectoryPath(), prepared.fileName), prepared.frames, prepared.sampleRate))
  {
    return false;
  }
  sample = std::move(prepared);
  return true;
}

bool SamplerManager::ApplySample(std::size_t trackIndex, const PreparedSample& sample)
{
  if (!CanReplaceSample(trackIndex) || !sample.frames || !sample.frames->frames || sample.frames->frameCount == 0 ||
      sample.frames->frameCount > sampler::kMaxSampleFrames || sample.sampleRate < sampler::kMinSampleRate ||
      sample.sampleRate > sampler::kMaxSampleRate || sample.fileName.empty() || SampleFileName(sample.fileName) != sample.fileName)
  {
    return false;
  }
  TrackState& state = tracks_[trackIndex];
  StopPreview(trackIndex, state);
  state.currentFrames = sample.frames;
  state.currentSampleRate = sample.sampleRate;
  state.draftFrames.reset();
  state.ui.sampleName = FileStem(sample.fileName);
  state.ui.sampleFileName = sample.fileName;
  state.ui.hasSample = state.currentFrames && state.currentFrames->frameCount != 0;
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.overwritePending = false;
  state.ui.deleteConfirm = false;
  state.pendingDeleteFileName.clear();
  state.ui.sampleRate = sample.sampleRate;
  state.ui.frameCount = static_cast<std::uint32_t>((state.currentFrames ? state.currentFrames->frameCount : 0));
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.frameCount;
  state.ui.previewCursorFrame = 0;
  BuildWaveform(state.ui.waveform, state.currentFrames);
  state.ui.phase = state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  state.trimResetPending = true;
  SyncAudioSample(trackIndex, state);
  return state.ui.hasSample;
}

bool SamplerManager::DeleteBrowserSample(std::size_t trackIndex, std::string_view id)
{
  if (!CanReplaceSample(trackIndex))
  {
    return false;
  }

  const std::string fileName = SampleFileName(id);
  if (fileName.empty())
  {
    return false;
  }
  TrackState& state = tracks_[trackIndex];
  if (!state.ui.deleteConfirm || state.pendingDeleteFileName != fileName)
  {
    state.pendingDeleteFileName = fileName;
    state.ui.deleteConfirm = true;
    state.ui.statusText = "DELETE?";
    return false;
  }
  state.pendingDeleteFileName.clear();
  state.ui.deleteConfirm = false;
  if (FileUsedByOtherTrack(trackIndex, fileName))
  {
    state.ui.statusText = "FILE IN USE";
    return false;
  }
  if (!RemoveFileIfPresent(JoinPath(Device::Storage::SampleDirectoryPath(), fileName)))
  {
    state.ui.statusText = "DEL ERR";
    return false;
  }
  if (SameSampleFile(state.ui.sampleFileName, fileName))
  {
    StopPreview(trackIndex, state);
    state.currentFrames.reset();
    state.currentSampleRate = audio::kSampleRate;
    state.ui = SamplerTrackUiState{};
    state.trimResetPending = true;
    SyncAudioSample(trackIndex, state);
  }
  state.ui.statusText = "DELETED";
  return true;
}

bool SamplerManager::RenameCurrentSample(std::size_t trackIndex, std::string_view newName)
{
  if (!IsValidSamplerTrack(trackIndex) || !tracks_[trackIndex].ui.hasSample)
  {
    return false;
  }

  return RenameBrowserSample(trackIndex, tracks_[trackIndex].ui.sampleFileName, newName);
}

bool SamplerManager::RenameBrowserSample(std::size_t trackIndex, std::string_view id, std::string_view newName,
                                        std::string* renamedFile)
{
  if (!CanReplaceSample(trackIndex)) return false;
  const std::string fileName = SampleFileName(id);
  if (fileName.empty()) return false;
  TrackState& state = tracks_[trackIndex];
  const std::string renamedName = SanitizedSampleName(newName);
  // Preserve the extension for a no-op rename, including imported uppercase .WAV.
  const std::string destinationName = renamedName == FileStem(fileName) ? fileName : renamedName + ".wav";
  const std::string sourcePath = JoinPath(Device::Storage::SampleDirectoryPath(), fileName);
  const std::string destinationPath = JoinPath(Device::Storage::SampleDirectoryPath(), destinationName);
  if (!RenameFileIfPossible(sourcePath, destinationPath))
  {
    state.ui.statusText = "REN ERR";
    return false;
  }

  for (auto& track : tracks_)
  {
    if (track.ui.hasSample && SameSampleFile(track.ui.sampleFileName, fileName))
    {
      track.ui.sampleName = renamedName;
      track.ui.sampleFileName = destinationName;
    }
    if (SameSampleFile(track.pendingDeleteFileName, fileName))
    {
      track.pendingDeleteFileName.clear();
      track.ui.deleteConfirm = false;
    }
  }
  if (renamedFile) *renamedFile = destinationName;
  state.ui.statusText = "RENAMED";
  return true;
}

bool SamplerManager::DeleteCurrentSample(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex) || !tracks_[trackIndex].ui.hasSample)
  {
    return false;
  }

  return DeleteBrowserSample(trackIndex, tracks_[trackIndex].ui.sampleFileName);
}

SamplerTrackUiState SamplerManager::BuildUiState(std::size_t trackIndex) const
{
  if (trackIndex >= tracks_.size())
  {
    return {};
  }
  return tracks_[trackIndex].ui;
}

bool SamplerManager::ConsumeTrimReset(std::size_t trackIndex)
{
  if (trackIndex >= tracks_.size())
  {
    return false;
  }
  const bool pending = tracks_[trackIndex].trimResetPending;
  tracks_[trackIndex].trimResetPending = false;
  return pending;
}

bool SamplerManager::ValidateSampleReference(std::string_view id)
{
  const std::string name = SampleFileName(id);
  if (name.empty())
  {
    return false;
  }
  std::ifstream file(JoinPath(Device::Storage::SampleDirectoryPath(), name), std::ios::binary);
  sampler::WavInfo info;
  return file && sampler::InspectMono16Wav(file, info);
}

bool SamplerManager::IsValidSamplerTrack(std::size_t trackIndex) const
{
  return trackIndex < tracks_.size() && tracks_[trackIndex].isSampler;
}

bool SamplerManager::CanReplaceSample(std::size_t trackIndex)
{
  if (!IsValidSamplerTrack(trackIndex)) return false;
  auto& state = tracks_[trackIndex];
  if (state.ui.recording || state.ui.hasDraft)
  {
    state.ui.statusText = "SAVE SAMPLE FIRST";
    return false;
  }
  return true;
}

bool SamplerManager::FileUsedByOtherTrack(std::size_t trackIndex, std::string_view fileName) const
{
  for (std::size_t index = 0; index < tracks_.size(); ++index)
  {
    if (index != trackIndex && tracks_[index].ui.hasSample && SameSampleFile(tracks_[index].ui.sampleFileName, fileName))
      return true;
  }
  return false;
}

audio::SharedSampleBuffer SamplerManager::ActiveFrames(const TrackState& state)
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
  audio::SetSamplerFrames(trackIndex, frames, state.ui.hasDraft ? state.ui.sampleRate : state.currentSampleRate);
}

void SamplerManager::PumpInput(TrackState& state)
{
  if (!state.draftFrames || !state.ui.recording)
  {
    return;
  }
  const std::size_t maxFrames = state.ui.recordMaxFrameCount;
  float peak = 0;
  while (state.draftFrames->frameCount < maxFrames)
  {
    std::size_t framesRead = 0;
    const std::size_t requestFrames = std::min<std::size_t>(kInputReadFrames, maxFrames - state.draftFrames->frameCount);
    auto* destination = state.draftFrames->frames.get() + state.draftFrames->frameCount;
    const auto status = Device::Audio::ReadInputFrames(destination, requestFrames, framesRead, 0);
    if (status == Device::Audio::InputStatus::Overflow)
    {
      Device::Audio::StopInput();
      activeRecordingTrack_ = -1;
      FinishDraft(state);
      state.ui.phase = SamplerTrackPhase::Error;
      state.ui.statusText = "INPUT OVERRUN";
      return;
    }
    if (status != Device::Audio::InputStatus::Ready || framesRead == 0)
    {
      break;
    }
    peak = std::max(peak, PeakLevel(destination, framesRead));
    state.draftFrames->frameCount += framesRead;
  }
  state.ui.inputLevel = peak;
  state.ui.frameCount = static_cast<std::uint32_t>(state.draftFrames->frameCount);
  BuildRecordingWaveform(state.ui.waveform, state.draftFrames, state.ui.sampleRate);
  state.ui.recordProgress = maxFrames == 0 ? 0.0f : static_cast<float>(state.draftFrames->frameCount) / static_cast<float>(maxFrames);
  if (state.draftFrames->frameCount >= maxFrames)
  {
    Device::Audio::StopInput();
    activeRecordingTrack_ = -1;
    FinishDraft(state);
  }
}

void SamplerManager::FinishDraft(TrackState& state)
{
  state.ui.hasDraft = state.draftFrames && state.draftFrames->frameCount != 0;
  state.ui.recording = false;
  state.ui.phase = state.ui.hasDraft ? SamplerTrackPhase::DraftTrim : (state.ui.hasSample ? SamplerTrackPhase::Ready : SamplerTrackPhase::Empty);
  state.ui.statusText = PhaseStatus(state.ui.phase);
  state.ui.frameCount = state.draftFrames ? static_cast<std::uint32_t>(state.draftFrames->frameCount) : 0;
  state.ui.recordMaxFrameCount = 0;
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.frameCount;
  state.ui.previewCursorFrame = state.ui.trimStartFrame;
  BuildWaveform(state.ui.waveform, state.draftFrames);
  state.ui.previewing = false;
  state.ui.recordProgress = 0.0f;
  state.trimResetPending = true;
}

bool SamplerManager::SaveDraftAs(TrackState& state, std::string_view name, bool overwrite)
{
  if (!state.ui.hasDraft || (!state.draftFrames || state.draftFrames->frameCount == 0))
  {
    return false;
  }

  const std::string sanitizedName = SanitizedSampleName(name);
  const std::string filePath = JoinPath(Device::Storage::SampleDirectoryPath(), sanitizedName + ".wav");
  const std::size_t trackIndex = static_cast<std::size_t>(&state - tracks_.data());
  state.ui.draftName = sanitizedName;
  if (!overwrite) state.ui.overwritePending = false;
  if (FileExists(filePath) && !IsRegularFile(filePath))
  {
    state.ui.statusText = "SAVE ERR";
    return false;
  }
  if (!overwrite && FileExists(filePath))
  {
    state.ui.overwritePending = true;
    state.ui.phase = SamplerTrackPhase::Naming;
    state.ui.statusText = "NAME EXISTS";
    return false;
  }
  if (FileUsedByOtherTrack(trackIndex, sanitizedName + ".wav"))
  {
    state.ui.statusText = "FILE IN USE";
    return false;
  }

  const std::uint32_t totalFrames = static_cast<std::uint32_t>(state.draftFrames->frameCount);
  const std::uint32_t startFrame = std::min<std::uint32_t>(state.ui.trimStartFrame, totalFrames - 1);
  const std::uint32_t endFrame = std::clamp<std::uint32_t>(state.ui.trimEndFrame, startFrame + 1, totalFrames);
  audio::SharedSampleBuffer savedFrames = state.draftFrames;
  if (startFrame != 0 || endFrame != totalFrames)
  {
    auto trimmed = sampler::AllocateFrames(endFrame - startFrame);
    if (!trimmed)
    {
      state.ui.statusText = "NO MEMORY";
      return false;
    }
    std::copy_n(state.draftFrames->frames.get() + startFrame, trimmed->frameCount, trimmed->frames.get());
    savedFrames = std::move(trimmed);
  }

  if (!Device::Storage::EnsureDirectoryLayout() || !sampler::WriteMono16Wav(
          filePath, savedFrames->frames.get(), savedFrames->frameCount, state.ui.sampleRate))
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "SAVE ERR";
    return false;
  }

  state.currentFrames = savedFrames;
  state.currentSampleRate = state.ui.sampleRate;
  state.draftFrames.reset();
  state.ui.sampleName = sanitizedName;
  state.ui.sampleFileName = sanitizedName + ".wav";
  state.ui.hasSample = true;
  state.ui.hasDraft = false;
  state.ui.needsName = false;
  state.ui.overwritePending = false;
  state.ui.phase = SamplerTrackPhase::Ready;
  state.ui.statusText = PhaseStatus(state.ui.phase);
  state.ui.frameCount = static_cast<std::uint32_t>((state.currentFrames ? state.currentFrames->frameCount : 0));
  state.ui.trimStartFrame = 0;
  state.ui.trimEndFrame = state.ui.frameCount;
  state.ui.previewCursorFrame = state.ui.trimStartFrame;
  BuildWaveform(state.ui.waveform, state.currentFrames);
  state.ui.previewing = false;
  state.trimResetPending = true;
  SyncAudioSample(trackIndex, state);
  return true;
}

}  // namespace synth

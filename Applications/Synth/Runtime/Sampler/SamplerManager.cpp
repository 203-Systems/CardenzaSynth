#include "Applications/Synth/Runtime/Sampler/SamplerManager.h"

#include "Devices/Device.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Library/Synth/Core/SynthStorage.h"
#include "Applications/Synth/Runtime/Sampler/SamplerWav.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <sys/stat.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace synth {

namespace {

// Without a card the take has to fit in RAM.
constexpr std::uint32_t kMaxRamRecordSeconds = 5;
constexpr std::size_t kInputReadFrames = 512;
constexpr std::uint32_t kRecordingWaveformWindowMs = 750;
constexpr std::size_t kWaveformProbeFrames = 512;

std::string TakeFilePath(std::size_t trackIndex)
{
  const std::string directory = storage::SessionDirectory();
  return directory.empty() ? std::string() : directory + "/take-" + std::to_string(trackIndex + 1) + ".wav";
}

std::string WavHeaderText(std::size_t frameCount, std::uint32_t sampleRate)
{
  std::ostringstream header;
  sampler::WriteMono16WavHeader(header, frameCount, sampleRate);
  return header.str();
}

// What has been written so far is on the card even if the power goes now (the file size in the
// directory only changes here, not with every write).
void SyncFile(std::FILE* file)
{
  std::fflush(file);
#if defined(_WIN32)
  (void)_commit(_fileno(file));
#else
  (void)fsync(fileno(file));
#endif
}

// A take a power cut interrupted has a header from when it began: make it agree with what arrived.
bool RepairTakeHeader(const std::string& path)
{
  std::FILE* file = std::fopen(path.c_str(), "r+b");
  if (file == nullptr) return false;
  std::array<unsigned char, sampler::kWavHeaderBytes> header{};
  const bool read = std::fread(header.data(), 1, header.size(), file) == header.size() && std::memcmp(header.data(), "RIFF", 4) == 0;
  bool repaired = false;
  if (read && std::fseek(file, 0, SEEK_END) == 0)
  {
    const long size = std::ftell(file);
    const std::uint32_t rate = static_cast<std::uint32_t>(header[24]) | (static_cast<std::uint32_t>(header[25]) << 8) |
                               (static_cast<std::uint32_t>(header[26]) << 16) | (static_cast<std::uint32_t>(header[27]) << 24);
    if (size > static_cast<long>(sampler::kWavHeaderBytes) + 1 && rate >= sampler::kMinSampleRate && rate <= sampler::kMaxSampleRate)
    {
      const std::size_t frames = std::min<std::size_t>((static_cast<std::size_t>(size) - sampler::kWavHeaderBytes) / sizeof(std::int16_t), sampler::kMaxSampleFrames);
      const std::string text = WavHeaderText(frames, rate);
      repaired = std::fseek(file, 0, SEEK_SET) == 0 && std::fwrite(text.data(), 1, text.size(), file) == text.size();
      // An odd byte or anything past the frames would make it an invalid file.
      if (repaired && static_cast<std::size_t>(size) != sampler::kWavHeaderBytes + frames * sizeof(std::int16_t))
      {
        std::fclose(file);
#if defined(_WIN32)
        file = nullptr;
#else
        repaired = truncate(path.c_str(), static_cast<off_t>(sampler::kWavHeaderBytes + frames * sizeof(std::int16_t))) == 0;
        file = nullptr;
#endif
      }
    }
  }
  if (file != nullptr) std::fclose(file);
  return repaired;
}

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

  if (frames->stream)
  {
    // Too long to scan on the card: a look at the start of every bin.
    std::array<std::int16_t, kWaveformProbeFrames> probe{};
    for (std::size_t bin = 0; bin < waveform.size(); ++bin)
    {
      const std::size_t begin = bin * frames->frameCount / waveform.size();
      const std::size_t count = std::min(probe.size(), frames->frameCount - begin);
      if (count != 0 && frames->stream->ReadFrames(begin, probe.data(), count)) waveform[bin] = PeakLevel(probe.data(), count);
    }
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

}  // namespace

SamplerManager::~SamplerManager()
{
  if (activeRecordingTrack_ >= 0)
  {
    Device::Audio::StopInput();
  }
  if (takeFile_ != nullptr)
  {
    std::fclose(takeFile_);
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
      CloseTakeFile(state, keepTakes_);
    }
    if (state.takeOnCard && !keepTakes_) RemoveTakeFile(trackIndex);
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
  // First the microphone: if it fails, an unsaved take that is there stays as it is.
  if (Device::Audio::StartInput() != Device::Audio::InputStatus::Ready)
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "INPUT ERR";
    return false;
  }
  // The take goes straight to the card when there is one; the WAV header is written when it ends.
  std::size_t capacity = 0;
  std::shared_ptr<audio::SampleBuffer> buffer;
  const std::string takePath = storage::EnsureLayout() ? TakeFilePath(trackIndex) : std::string();
  if (!takePath.empty())
  {
    audio::SampleFileGuard guard(takePath);
    takeFile_ = std::fopen(takePath.c_str(), "wb");
    if (takeFile_ != nullptr)
    {
      // The rate is on the card from the start; the length is set when the take ends, or from the file
      // size after a power cut.
      const std::string header = WavHeaderText(0, info.sampleRate);
      if (std::fwrite(header.data(), 1, header.size(), takeFile_) == header.size())
      {
        capacity = std::min<std::size_t>(info.sampleRate * kMaxRecordSeconds, sampler::kMaxSampleFrames);
      }
      else
      {
        std::fclose(takeFile_);
        takeFile_ = nullptr;
      }
    }
  }
  if (takeFile_ == nullptr)
  {
    capacity = std::min<std::size_t>(info.sampleRate * kMaxRamRecordSeconds, sampler::kMaxSampleFrames);
    for (; capacity >= info.sampleRate / 4; capacity /= 2)
    {
      buffer = sampler::AllocateFrames(capacity);
      if (buffer)
      {
        break;
      }
    }
    if (!buffer)
    {
      Device::Audio::StopInput();
      state.ui.statusText = "NO MEMORY";
      return false;
    }
  }
  state.draftFrames.reset();
  state.recordBuffer = std::move(buffer);
  state.recordedFrames = 0;
  state.recordBinPeak = 0;
  state.recordBinFrames = 0;
  state.recordSyncedFrames = 0;
  state.takeOnCard = takeFile_ != nullptr;
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

  if (state.recordedFrames == 0)
  {
    DiscardDraftTake(trackIndex);
    state.ui.statusText = "NO INPUT";
    return false;
  }

  FinishDraft(state);
  return state.ui.hasDraft;
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
  CloseTakeFile(tracks_[trackIndex], false);
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
  CloseTakeFile(state, false);
  StopPreview(trackIndex, state);
  state.draftFrames.reset();
  state.recordBuffer.reset();
  state.recordedFrames = 0;
  if (state.takeOnCard) RemoveTakeFile(trackIndex);
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

std::string SamplerManager::FreeTakeName() const
{
  for (int number = 1; number < 1000; ++number)
  {
    const std::string name = std::string("TAKE_") + (number < 10 ? "0" : "") + std::to_string(number);
    if (!FileExists(JoinPath(storage::SampleDirectory(), name + ".wav"))) return name;
  }
  return "TAKE";
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
          JoinPath(storage::SampleDirectory(), prepared.fileName), prepared.frames, prepared.sampleRate))
  {
    return false;
  }
  sample = std::move(prepared);
  return true;
}

bool SamplerManager::ApplySample(std::size_t trackIndex, const PreparedSample& sample)
{
  if (!CanReplaceSample(trackIndex) || !sample.frames || (!sample.frames->frames && !sample.frames->stream) || sample.frames->frameCount == 0 ||
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
  {
    const std::string path = JoinPath(storage::SampleDirectory(), fileName);
    audio::SampleFileGuard guard(path);
    if (!RemoveFileIfPresent(path))
    {
      state.ui.statusText = "DEL ERR";
      return false;
    }
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
  const std::string sourcePath = JoinPath(storage::SampleDirectory(), fileName);
  const std::string destinationPath = JoinPath(storage::SampleDirectory(), destinationName);
  {
    audio::SampleFileGuard guard(sourcePath);
    if (!RenameFileIfPossible(sourcePath, destinationPath))
    {
      state.ui.statusText = "REN ERR";
      return false;
    }
    guard.MovedTo(destinationPath);
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
  std::ifstream file(JoinPath(storage::SampleDirectory(), name), std::ios::binary);
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

bool SamplerManager::Previewing(std::size_t trackIndex) const
{
  return trackIndex < tracks_.size() && tracks_[trackIndex].ui.previewing;
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
  if (!state.ui.recording || (takeFile_ == nullptr && !state.recordBuffer))
  {
    return;
  }
  const std::size_t maxFrames = state.ui.recordMaxFrameCount;
  const std::size_t binFrames = std::max<std::size_t>(1, state.ui.sampleRate * kRecordingWaveformWindowMs / 1000 / state.ui.waveform.size());
  std::array<std::int16_t, kInputReadFrames> chunk{};
  float peak = 0;
  while (state.recordedFrames < maxFrames)
  {
    std::size_t framesRead = 0;
    const std::size_t requestFrames = std::min<std::size_t>(kInputReadFrames, maxFrames - state.recordedFrames);
    auto* destination = takeFile_ != nullptr ? chunk.data() : state.recordBuffer->frames.get() + state.recordedFrames;
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
    if (takeFile_ != nullptr && std::fwrite(destination, sizeof(std::int16_t), framesRead, takeFile_) != framesRead)
    {
      // The card is full or gone: what is on it so far is the take.
      Device::Audio::StopInput();
      activeRecordingTrack_ = -1;
      FinishDraft(state);
      state.ui.statusText = "CARD FULL";
      return;
    }
    peak = std::max(peak, PeakLevel(destination, framesRead));
    state.recordedFrames += framesRead;
    if (takeFile_ != nullptr && state.recordedFrames - state.recordSyncedFrames >= state.ui.sampleRate)
    {
      SyncFile(takeFile_);
      state.recordSyncedFrames = state.recordedFrames;
    }
    // The waveform of the last moments scrolls by, one peak per bin.
    for (std::size_t frame = 0; frame < framesRead; ++frame)
    {
      state.recordBinPeak = std::max<std::int32_t>(state.recordBinPeak, std::abs(static_cast<int>(destination[frame])));
      if (++state.recordBinFrames < binFrames) continue;
      std::rotate(state.ui.waveform.begin(), state.ui.waveform.begin() + 1, state.ui.waveform.end());
      state.ui.waveform.back() = static_cast<float>(state.recordBinPeak) / static_cast<float>(std::numeric_limits<std::int16_t>::max());
      state.recordBinPeak = 0;
      state.recordBinFrames = 0;
    }
  }
  state.ui.inputLevel = peak;
  state.ui.frameCount = static_cast<std::uint32_t>(state.recordedFrames);
  state.ui.recordProgress = maxFrames == 0 ? 0.0f : static_cast<float>(state.recordedFrames) / static_cast<float>(maxFrames);
  if (state.recordedFrames >= maxFrames)
  {
    Device::Audio::StopInput();
    activeRecordingTrack_ = -1;
    FinishDraft(state);
  }
}

void SamplerManager::CloseTakeFile(TrackState& state, bool keep)
{
  if (takeFile_ == nullptr)
  {
    return;
  }
  const std::size_t trackIndex = static_cast<std::size_t>(&state - tracks_.data());
  bool closed = true;
  if (keep && state.recordedFrames != 0)
  {
    // Now the length is known.
    const std::string text = WavHeaderText(state.recordedFrames, state.ui.sampleRate);
    closed = std::fseek(takeFile_, 0, SEEK_SET) == 0 && std::fwrite(text.data(), 1, text.size(), takeFile_) == text.size();
  }
  closed = std::fclose(takeFile_) == 0 && closed;
  takeFile_ = nullptr;
  if (keep && closed && state.recordedFrames != 0)
  {
    audio::SharedSampleBuffer take;
    std::uint32_t rate = state.ui.sampleRate;
    if (sampler::ReadMono16Wav(TakeFilePath(trackIndex), take, rate)) state.draftFrames = std::move(take);
  }
  if (!state.draftFrames) RemoveTakeFile(trackIndex);
}

void SamplerManager::RemoveTakeFile(std::size_t trackIndex)
{
  tracks_[trackIndex].takeOnCard = false;
  const std::string path = TakeFilePath(trackIndex);
  if (path.empty()) return;
  audio::SampleFileGuard guard(path);
  (void)RemoveFileIfPresent(path);
}

int SamplerManager::RestoreTakes()
{
  int first = -1;
  for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex)
  {
    TrackState& state = tracks_[trackIndex];
    if (state.ui.hasDraft || activeRecordingTrack_ == static_cast<int>(trackIndex)) continue;
    const std::string path = TakeFilePath(trackIndex);
    if (path.empty() || !IsRegularFile(path)) continue;
    audio::SharedSampleBuffer take;
    std::uint32_t rate = audio::kSampleRate;
    const bool usable = state.isSampler && RepairTakeHeader(path) && sampler::ReadMono16Wav(path, take, rate);
    if (!usable)
    {
      RemoveTakeFile(trackIndex);
      continue;
    }
    state.draftFrames = std::move(take);
    state.recordedFrames = state.draftFrames->frameCount;
    state.takeOnCard = true;
    state.ui.sampleRate = rate;
    FinishDraft(state);
    if (first < 0) first = static_cast<int>(trackIndex);
  }
  return first;
}

void SamplerManager::KeepTakesOnCard()
{
  keepTakes_ = true;
  if (activeRecordingTrack_ < 0) return;
  // The recording up to here is the take the next start finds.
  const auto trackIndex = static_cast<std::size_t>(activeRecordingTrack_);
  (void)StopRecord(trackIndex);
}

void SamplerManager::FinishDraft(TrackState& state)
{
  if (takeFile_ != nullptr)
  {
    CloseTakeFile(state, true);
  }
  else if (state.recordBuffer)
  {
    state.recordBuffer->frameCount = state.recordedFrames;
    state.draftFrames = std::move(state.recordBuffer);
  }
  state.recordBuffer.reset();
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
  const std::string filePath = JoinPath(storage::SampleDirectory(), sanitizedName + ".wav");
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
  const std::size_t savedCount = endFrame - startFrame;
  const audio::SampleBuffer& draft = *state.draftFrames;
  // The file is written piece by piece from the take, in RAM or on the card.
  bool written = false;
  {
    audio::SampleFileGuard guard(filePath);
    written = storage::EnsureLayout() &&
        sampler::WriteMono16WavFrom(filePath, savedCount, state.ui.sampleRate, [&](std::int16_t* chunk, std::size_t offset, std::size_t count) {
          if (draft.stream) return draft.stream->ReadFrames(startFrame + offset, chunk, count);
          std::copy_n(draft.frames.get() + startFrame + offset, count, chunk);
          return true;
        });
  }
  // Back from the file: a short sample into RAM, a long one streamed from where it now lives.
  audio::SharedSampleBuffer savedFrames;
  std::uint32_t savedRate = state.ui.sampleRate;
  if (!written || !sampler::ReadMono16Wav(filePath, savedFrames, savedRate))
  {
    state.ui.phase = SamplerTrackPhase::Error;
    state.ui.statusText = "SAVE ERR";
    return false;
  }

  state.currentFrames = savedFrames;
  state.currentSampleRate = state.ui.sampleRate;
  state.draftFrames.reset();
  if (state.takeOnCard) RemoveTakeFile(trackIndex);
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

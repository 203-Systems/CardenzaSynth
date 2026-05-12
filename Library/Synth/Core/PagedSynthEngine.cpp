#include "Library/Synth/Core/PagedSynthEngine.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include "Devices/DeviceAudio.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "OS/Framework/Midi/MidiPort.h"

namespace synth {

namespace {

constexpr std::size_t kMaxMidiMessagesPerFrame = 24;

int ClampToParameterRange(const SynthParameterInfo& parameter, int value)
{
  return std::clamp(value, parameter.minValue, parameter.maxValue);
}

std::string NormalizePresetName(std::string_view name)
{
  std::string normalized;
  normalized.reserve(name.size());

  bool lastWasSpace = true;
  for (char character : name)
  {
    if (std::isspace(static_cast<unsigned char>(character)) != 0)
    {
      if (!lastWasSpace)
      {
        normalized.push_back(' ');
        lastWasSpace = true;
      }
      continue;
    }

    normalized.push_back(character);
    lastWasSpace = false;
  }

  if (!normalized.empty() && normalized.back() == ' ')
  {
    normalized.pop_back();
  }

  return normalized;
}

std::string FoldAscii(std::string_view text)
{
  std::string folded;
  folded.reserve(text.size());
  for (char character : text)
  {
    folded.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }
  return folded;
}

}  // namespace

PagedSynthEngine::PagedSynthEngine(SynthDescriptor descriptor)
    : descriptor_(std::move(descriptor)) {}

PagedSynthEngine::~PagedSynthEngine() = default;

bool PagedSynthEngine::Init(const SynthEngineInit& init)
{
  engineIndex_ = init.engineIndex;
  const std::string portName = init.portName.empty() ? "SynthTrack" + std::to_string(init.engineIndex + 1) : init.portName;
  port_ = std::make_unique<midi::MidiPort>(portName, init.portId, 128);
  if (!port_ || port_->Id() == midi::MIDI_PORT_INVALID)
  {
    port_.reset();
    engineIndex_ = static_cast<std::size_t>(-1);
    lastEvent_ = "port offline";
    audioBackendName_ = "none";
    audioConnected_ = false;
    audioHasNativeOutput_ = false;
    return false;
  }

  audio::EnsureEngine(engineIndex_, descriptor_.kind);
  device::SetSynthAudioRenderProvider({audio::kSampleRate, audio::RenderFrames});
  SyncRuntimeState();
  audio::WarmUp();
  const device::SynthAudioPipelineBinding audioBinding = device::ConnectSynthAudioPipeline(init.engineIndex);
  audioBackendName_ = audioBinding.backendName;
  audioConnected_ = audioBinding.connected;
  audioHasNativeOutput_ = audioBinding.hasNativeOutput;
  lastEvent_ = audioBinding.connected ? "ready" : "ready (audio pending)";
  return true;
}

uint16_t PagedSynthEngine::PortId() const
{
  return port_ ? port_->Id() : static_cast<uint16_t>(midi::MIDI_PORT_INVALID);
}

void PagedSynthEngine::DispatchPendingMessages()
{
  if (!port_)
  {
    return;
  }

  midi::MidiPacket packet;
  std::size_t processed = 0;
  while (processed < kMaxMidiMessagesPerFrame && port_->Get(&packet))
  {
    ProcessMidi(packet);
    ++processed;
  }
}

const SynthDescriptor& PagedSynthEngine::Descriptor() const
{
  return descriptor_;
}

SynthParameterPage PagedSynthEngine::ListParameters(const SynthPageRequest& request) const
{
  SynthParameterPage page;
  page.totalCount = parameters_.size();
  if (request.offset >= parameters_.size() || request.limit == 0)
  {
    return page;
  }

  const std::size_t end = std::min(parameters_.size(), request.offset + request.limit);
  page.items.reserve(end - request.offset);
  for (std::size_t index = request.offset; index < end; ++index)
  {
    page.items.push_back(parameters_[index]);
  }
  return page;
}

SynthPresetPage PagedSynthEngine::ListPresets(const SynthPageRequest& request) const
{
  SynthPresetPage page;
  page.totalCount = presets_.size();
  if (request.offset >= presets_.size() || request.limit == 0)
  {
    return page;
  }

  const std::size_t end = std::min(presets_.size(), request.offset + request.limit);
  page.items.reserve(end - request.offset);
  for (std::size_t index = request.offset; index < end; ++index)
  {
    page.items.push_back(presets_[index].info);
    page.items.back().active = (presets_[index].info.id == currentPresetId_);
  }
  return page;
}

bool PagedSynthEngine::GetParameterInfo(const std::string& parameterId, SynthParameterInfo& parameter) const
{
  const SynthParameterInfo* match = FindParameter(parameterId);
  if (match == nullptr)
  {
    return false;
  }

  parameter = *match;
  return true;
}

void PagedSynthEngine::ProcessMidi(const midi::MidiPacket& packet)
{
  switch (packet.status)
  {
    case midi::MidiStatus::NoteOn:
      if (packet.data[2] == 0)
      {
        if (activeNotes_ > 0)
        {
          --activeNotes_;
        }
        audio::NoteOff(engineIndex_, packet.data[1]);
        lastEvent_ = "note off " + std::to_string(packet.data[1]);
      }
      else
      {
        ++activeNotes_;
        audio::NoteOn(engineIndex_, packet.data[1], packet.data[2]);
        lastEvent_ = "note on " + std::to_string(packet.data[1]) + " vel " + std::to_string(packet.data[2]);
      }
      break;
    case midi::MidiStatus::NoteOff:
      if (activeNotes_ > 0)
      {
        --activeNotes_;
      }
      audio::NoteOff(engineIndex_, packet.data[1]);
      lastEvent_ = "note off " + std::to_string(packet.data[1]);
      break;
    case midi::MidiStatus::ControlChange:
      if (packet.data[1] == 120 || packet.data[1] == 123)
      {
        audio::AllNotesOff(engineIndex_);
        activeNotes_ = 0;
      }
      lastEvent_ = "cc " + std::to_string(packet.data[1]) + "=" + std::to_string(packet.data[2]);
      break;
    case midi::MidiStatus::ProgramChange:
      lastEvent_ = "program " + std::to_string(packet.data[1]);
      break;
    default:
      lastEvent_ = "midi 0x" + std::to_string(static_cast<int>(packet.data[0]));
      break;
  }
}

bool PagedSynthEngine::SetParameterValue(const std::string& parameterId, int value)
{
  SynthParameterInfo* parameter = FindParameterMutable(parameterId);
  if (parameter == nullptr)
  {
    return false;
  }

  parameter->value = ClampToParameterRange(*parameter, value);
  lastEvent_ = parameter->id + "=" + std::to_string(parameter->value);
  OnParameterChanged(*parameter);
  return true;
}

bool PagedSynthEngine::AdjustParameterValue(const std::string& parameterId, int delta)
{
  const SynthParameterInfo* parameter = FindParameter(parameterId);
  if (parameter == nullptr)
  {
    return false;
  }

  const int step = std::max(1, parameter->step);
  return SetParameterValue(parameterId, parameter->value + delta * step);
}

std::string PagedSynthEngine::DescribeParameterValue(const std::string& parameterId) const
{
  const SynthParameterInfo* parameter = FindParameter(parameterId);
  if (parameter == nullptr)
  {
    return "--";
  }

  return FormatParameterValue(*parameter);
}

bool PagedSynthEngine::LoadPreset(const std::string& presetId)
{
  auto iterator = std::find_if(presets_.begin(), presets_.end(), [&](const PresetState& preset) {
    return preset.info.id == presetId;
  });
  if (iterator == presets_.end())
  {
    return false;
  }

  for (SynthParameterInfo& parameter : parameters_)
  {
    const auto valueIterator = iterator->values.find(parameter.id);
    parameter.value = valueIterator == iterator->values.end() ? parameter.defaultValue : ClampToParameterRange(parameter, valueIterator->second);
  }

  currentPresetId_ = iterator->info.id;
  currentPresetName_ = iterator->info.name;
  lastEvent_ = "preset " + currentPresetName_;
  OnPresetLoaded(currentPresetId_);
  return true;
}

bool PagedSynthEngine::SavePreset(const std::string& presetId, const std::string& presetName)
{
  const std::string resolvedId = presetId.empty() ? NextUserPresetId() : presetId;
  auto iterator = std::find_if(presets_.begin(), presets_.end(), [&](const PresetState& preset) {
    return preset.info.id == resolvedId;
  });

  if (iterator == presets_.end())
  {
    presets_.push_back(PresetState{});
    iterator = presets_.end() - 1;
    iterator->info.id = resolvedId;
  }

  iterator->info.name = presetName.empty() ? resolvedId : presetName;
  iterator->info.writable = true;
  iterator->values.clear();
  for (const SynthParameterInfo& parameter : parameters_)
  {
    iterator->values.emplace(parameter.id, parameter.value);
  }

  currentPresetId_ = iterator->info.id;
  currentPresetName_ = iterator->info.name;
  lastEvent_ = "saved " + currentPresetName_;
  return true;
}

std::string PagedSynthEngine::CurrentPresetId() const
{
  return currentPresetId_;
}

std::string PagedSynthEngine::CurrentPresetName() const
{
  return currentPresetName_;
}

std::string PagedSynthEngine::DebugState() const
{
  std::ostringstream stream;
  stream << descriptor_.displayName << " state: " << lastEvent_ << ", activeNotes=" << activeNotes_;
  stream << ", preset=" << currentPresetName_;
  stream << ", audio=" << audioBackendName_;
  if (!audioConnected_)
  {
    stream << ":offline";
  }
  else if (!audioHasNativeOutput_)
  {
    stream << ":logical";
  }
  return stream.str();
}

void PagedSynthEngine::RegisterParameter(const SynthParameterInfo& parameter)
{
  SynthParameterInfo stored = parameter;
  stored.value = ClampToParameterRange(stored, stored.defaultValue);
  parameters_.push_back(std::move(stored));
}

void PagedSynthEngine::RegisterPreset(const std::string& presetId,
                                      const std::string& presetName,
                                      const std::map<std::string, int>& values,
                                      bool writable)
{
  PresetState preset;
  preset.info.id = presetId;
  preset.info.name = NormalizePresetName(presetName);
  preset.info.writable = writable;
  preset.values = values;
  presets_.push_back(std::move(preset));
  std::stable_sort(presets_.begin(), presets_.end(), [](const PresetState& left, const PresetState& right) {
    const std::string leftName = FoldAscii(left.info.name);
    const std::string rightName = FoldAscii(right.info.name);
    if (leftName != rightName)
    {
      return leftName < rightName;
    }
    return left.info.id < right.info.id;
  });
}

SynthParameterInfo* PagedSynthEngine::FindParameterMutable(const std::string& parameterId)
{
  auto iterator = std::find_if(parameters_.begin(), parameters_.end(), [&](const SynthParameterInfo& parameter) {
    return parameter.id == parameterId;
  });
  return iterator == parameters_.end() ? nullptr : &(*iterator);
}

const SynthParameterInfo* PagedSynthEngine::FindParameter(const std::string& parameterId) const
{
  auto iterator = std::find_if(parameters_.begin(), parameters_.end(), [&](const SynthParameterInfo& parameter) {
    return parameter.id == parameterId;
  });
  return iterator == parameters_.end() ? nullptr : &(*iterator);
}

std::string PagedSynthEngine::FormatParameterValue(const SynthParameterInfo& parameter) const
{
  if (parameter.kind == SynthParameterKind::Toggle)
  {
    return parameter.value == 0 ? "OFF" : "ON";
  }
  return std::to_string(parameter.value);
}

void PagedSynthEngine::OnParameterChanged(const SynthParameterInfo& parameter)
{
  if (engineIndex_ == static_cast<std::size_t>(-1))
  {
    return;
  }
  if (!ShouldSyncParameterToRuntime(parameter))
  {
    return;
  }

  audio::SetEngineParameter(engineIndex_, parameter.id, parameter.value);
}

void PagedSynthEngine::OnPresetLoaded(const std::string& presetId)
{
  (void)presetId;
  SyncRuntimeState();
}

void PagedSynthEngine::SyncRuntimeState()
{
  if (engineIndex_ == static_cast<std::size_t>(-1))
  {
    return;
  }

  audio::EnsureEngine(engineIndex_, descriptor_.kind);
  audio::SetEnginePreset(engineIndex_, currentPresetId_);
  for (const SynthParameterInfo& parameter : parameters_)
  {
    if (!ShouldSyncParameterOnPresetLoad(parameter))
    {
      continue;
    }
    audio::SetEngineParameter(engineIndex_, parameter.id, parameter.value);
  }
}

bool PagedSynthEngine::ShouldSyncParameterToRuntime(const SynthParameterInfo& parameter) const
{
  (void)parameter;
  return true;
}

bool PagedSynthEngine::ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const
{
  return ShouldSyncParameterToRuntime(parameter);
}

std::string PagedSynthEngine::NextUserPresetId() const
{
  std::size_t index = 1;
  while (true)
  {
    const std::string candidate = "user." + std::to_string(index);
    const auto iterator = std::find_if(presets_.begin(), presets_.end(), [&](const PresetState& preset) {
      return preset.info.id == candidate;
    });
    if (iterator == presets_.end())
    {
      return candidate;
    }
    ++index;
  }
}

}  // namespace synth

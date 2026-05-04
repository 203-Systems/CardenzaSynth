#include "Library/Synth/PagedSynthEngine.h"

#include <algorithm>
#include <sstream>

#include "OS/Framework/Midi/MidiPort.h"

namespace synth {

namespace {

int ClampToParameterRange(const SynthParameterInfo& parameter, int value)
{
  return std::clamp(value, parameter.minValue, parameter.maxValue);
}

}  // namespace

PagedSynthEngine::PagedSynthEngine(SynthDescriptor descriptor)
    : descriptor_(std::move(descriptor)) {}

PagedSynthEngine::~PagedSynthEngine() = default;

bool PagedSynthEngine::Init(const SynthEngineInit& init)
{
  const std::string portName = init.portName.empty() ? "SynthTrack" + std::to_string(init.engineIndex + 1) : init.portName;
  port_ = std::make_unique<midi::MidiPort>(portName, init.portId, 128);
  if (!port_ || port_->Id() == midi::MIDI_PORT_INVALID)
  {
    port_.reset();
    audioBinding_ = {};
    lastEvent_ = "port offline";
    return false;
  }

  audioBinding_ = device::ConnectSynthAudioPipeline(init.engineIndex);
  lastEvent_ = audioBinding_.connected ? "ready" : "ready (audio pending)";
  return true;
}

uint16_t PagedSynthEngine::PortId() const
{
  return port_ ? port_->Id() : midi::MIDI_PORT_INVALID;
}

void PagedSynthEngine::DispatchPendingMessages()
{
  if (!port_)
  {
    return;
  }

  midi::MidiPacket packet;
  while (port_->Get(&packet))
  {
    ProcessMidi(packet);
  }
}

const device::SynthAudioPipelineBinding& PagedSynthEngine::AudioBinding() const
{
  return audioBinding_;
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
        lastEvent_ = "note off " + std::to_string(packet.data[1]);
      }
      else
      {
        ++activeNotes_;
        lastEvent_ = "note on " + std::to_string(packet.data[1]) + " vel " + std::to_string(packet.data[2]);
      }
      break;
    case midi::MidiStatus::NoteOff:
      if (activeNotes_ > 0)
      {
        --activeNotes_;
      }
      lastEvent_ = "note off " + std::to_string(packet.data[1]);
      break;
    case midi::MidiStatus::ControlChange:
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
    OnParameterChanged(parameter);
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
  stream << ", audio=" << audioBinding_.backendName;
  if (!audioBinding_.connected)
  {
    stream << ":offline";
  }
  else if (!audioBinding_.hasNativeOutput)
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
  preset.info.name = presetName;
  preset.info.writable = writable;
  preset.values = values;
  presets_.push_back(std::move(preset));
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
  (void)parameter;
}

void PagedSynthEngine::OnPresetLoaded(const std::string& presetId)
{
  (void)presetId;
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
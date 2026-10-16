#include "Library/Synth/Core/PagedSynthEngine.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "Devices/Device.h"
#include "Devices/DeviceAudio.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "OS/Framework/Midi/MidiPort.h"

namespace synth {

namespace {

constexpr std::size_t kMaxMidiMessagesPerFrame = 24;
constexpr std::size_t kMaxUserPresets = 32;

bool IsUserPresetId(const std::string& id)
{
  return id.size() > 5 && id.size() <= 16 && id.compare(0, 5, "user.") == 0 &&
         std::all_of(id.begin() + 5, id.end(), [](unsigned char c) { return c >= '0' && c <= '9'; });
}

std::string PresetBankPath(const SynthDescriptor& descriptor)
{
  return Device::Storage::PresetDirectoryPath(descriptor.displayName) + "/user.ccpreset";
}

bool ReadQuoted(std::istream& file, std::string& value, std::size_t limit)
{
  file >> std::ws;
  if (file.get() != '"') return false;
  value.clear();
  char character;
  while (file.get(character))
  {
    if (character == '"') return true;
    if (character == '\\' && !file.get(character)) return false;
    if (value.size() >= limit) return false;
    value.push_back(character);
  }
  return false;
}

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
  (void)RefreshPresetCatalog();
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
  // Indexed factory catalog first, then registered presets and user presets.
  page.totalCount = factoryOrder_.size() + presets_.size();
  if (request.offset >= page.totalCount || request.limit == 0)
  {
    return page;
  }

  const std::size_t end = std::min(page.totalCount, request.offset + request.limit);
  page.items.reserve(end - request.offset);
  for (std::size_t index = request.offset; index < end; ++index)
  {
    if (index < factoryOrder_.size())
    {
      const std::size_t factory = factoryOrder_[index];
      page.items.push_back({std::string(FactoryPresetId(factory)), FactoryPresetDisplayName(factory), false, false});
    }
    else
    {
      page.items.push_back(presets_[index - factoryOrder_.size()].info);
    }
    page.items.back().active = (page.items.back().id == currentPresetId_);
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
  const auto iterator = std::find_if(presets_.begin(), presets_.end(), [&](const PresetState& preset) {
    return preset.info.id == presetId;
  });
  const PresetState* stored = iterator == presets_.end() ? nullptr : &*iterator;
  const std::size_t factory = stored ? kNoFactoryPreset : FindFactoryPreset(presetId);
  if (!stored && factory == kNoFactoryPreset)
  {
    return false;
  }

  for (SynthParameterInfo& parameter : parameters_)
  {
    const auto valueIterator = stored ? stored->values.find(parameter.id) : std::map<std::string, int>::const_iterator{};
    parameter.value = stored && valueIterator != stored->values.end() ? ClampToParameterRange(parameter, valueIterator->second) : FactoryParameterValue(presetId, parameter);
  }

  currentPresetId_ = presetId;
  basePresetId_ = stored ? stored->basePresetId : presetId;
  currentPresetName_ = stored ? stored->info.name : FactoryPresetDisplayName(factory);
  lastEvent_ = "preset " + currentPresetName_;
  OnPresetLoaded(currentPresetId_);
  return true;
}

bool PagedSynthEngine::SavePreset(const std::string& presetId, const std::string& presetName)
{
  if ((!presetId.empty() && !IsUserPresetId(presetId)) || !Device::Storage::EnsureDirectoryLayout())
  {
    return false;
  }
  // Refresh the shared bank before selecting an ID; other tracks can save the same engine kind.
  if (!ReadUserPresets()) return false;
  const std::string resolvedId = presetId.empty() ? NextUserPresetId() : presetId;
  auto iterator = std::find_if(presets_.begin(), presets_.end(), [&](const PresetState& preset) {
    return preset.info.id == resolvedId;
  });

  const std::size_t userCount = std::count_if(presets_.begin(), presets_.end(), [](const PresetState& p) { return p.info.writable; });
  if (iterator == presets_.end() && userCount >= kMaxUserPresets)
  {
    return false;
  }
  SynthEngineState saved = CaptureState();
  saved.presetId = resolvedId;
  saved.presetName = NormalizePresetName(presetName.empty() ? currentPresetName_ : presetName);
  if (!ValidState(saved))
  {
    return false;
  }

  const std::string path = PresetBankPath(descriptor_);
  const std::string temporary = path + ".tmp";
  const std::string backup = path + ".bak";
  std::ofstream file(temporary, std::ios::trunc | std::ios::binary);
  if (!file) return false;
  file << "CCOS_PRESETS 1 " << std::quoted(descriptor_.id) << '\n';
  const auto write = [&](const SynthEngineState& state) {
    file << std::quoted(state.presetId) << ' ' << std::quoted(state.presetName) << ' '
         << std::quoted(state.basePresetId) << ' ' << state.values.size() << '\n';
    for (const auto& value : state.values) file << std::quoted(value.first) << ' ' << value.second << '\n';
  };
  for (const PresetState& preset : presets_)
  {
    if (preset.info.writable && preset.info.id != resolvedId)
      write({preset.basePresetId, preset.info.id, preset.info.name, preset.values});
  }
  write(saved);
  file.flush();
  const bool written = file.good();
  file.close();
  if (!written || file.fail()) { std::remove(temporary.c_str()); return false; }

  const bool exists = std::ifstream(path, std::ios::binary).good();
  if (exists)
  {
    std::remove(backup.c_str());
    if (std::rename(path.c_str(), backup.c_str()) != 0) { std::remove(temporary.c_str()); return false; }
  }
  if (std::rename(temporary.c_str(), path.c_str()) != 0)
  {
    if (exists) std::rename(backup.c_str(), path.c_str());
    std::remove(temporary.c_str());
    return false;
  }
  if (exists) std::remove(backup.c_str());
  (void)ReadUserPresets();
  currentPresetId_ = saved.presetId;
  currentPresetName_ = saved.presetName;
  lastEvent_ = "saved " + currentPresetName_;
  return true;
}

SynthEngineState PagedSynthEngine::CaptureState() const
{
  SynthEngineState state{basePresetId_, currentPresetId_, currentPresetName_, {}};
  for (const auto& parameter : parameters_) state.values.emplace(parameter.id, parameter.value);
  return state;
}

bool PagedSynthEngine::ValidState(const SynthEngineState& state) const
{
  if (state.presetName.empty() || state.presetName.size() > 48 || state.presetId.size() > 64 ||
      state.values.size() != parameters_.size() ||
      std::any_of(state.presetName.begin(), state.presetName.end(), [](unsigned char c) { return c < 32 || c == 127; })) return false;
  const bool registeredBase = std::any_of(presets_.begin(), presets_.end(), [&](const PresetState& preset) {
    return !preset.info.writable && preset.info.id == state.basePresetId;
  });
  if ((!registeredBase && FindFactoryPreset(state.basePresetId) == kNoFactoryPreset) ||
      (state.presetId != state.basePresetId && !IsUserPresetId(state.presetId))) return false;
  for (const auto& entry : state.values)
  {
    const auto* parameter = FindParameter(entry.first);
    if (!parameter || entry.second < parameter->minValue || entry.second > parameter->maxValue) return false;
  }
  return true;
}

bool PagedSynthEngine::RestoreState(const SynthEngineState& state)
{
  if (!ValidState(state)) return false;
  for (auto& parameter : parameters_) parameter.value = state.values.at(parameter.id);
  basePresetId_ = state.basePresetId;
  currentPresetId_ = state.presetId;
  currentPresetName_ = state.presetName;
  lastEvent_ = "restored " + currentPresetName_;
  SyncRuntimeState();
  return true;
}

bool PagedSynthEngine::HasBinding() const
{
  return port_ && port_->Id() != midi::MIDI_PORT_INVALID && engineIndex_ < kSynthTrackCount;
}

bool PagedSynthEngine::CanReplace(const ISynthEngine& previous) const
{
  return !port_ && !currentPresetId_.empty() && previous.HasBinding();
}

ISynthEngine::RuntimeBinding PagedSynthEngine::TakeBinding()
{
  RuntimeBinding binding{std::move(port_), engineIndex_, std::move(audioBackendName_), audioConnected_, audioHasNativeOutput_};
  engineIndex_ = static_cast<std::size_t>(-1);
  audioConnected_ = false;
  audioHasNativeOutput_ = false;
  return binding;
}

void PagedSynthEngine::ReplaceBinding(ISynthEngine& previous)
{
  assert(CanReplace(previous));
  RuntimeBinding binding = previous.TakeBinding();
  port_ = std::move(binding.port);
  engineIndex_ = binding.engineIndex;
  audioBackendName_ = std::move(binding.backendName);
  audioConnected_ = binding.connected;
  audioHasNativeOutput_ = binding.nativeOutput;
  // Discard stale queued notes from the previous project before the new patch is activated.
  midi::MidiPacket discarded;
  while (port_->Get(&discarded)) {}
  activeNotes_ = 0;
  SyncRuntimeState();
  audio::WarmUp();
}

bool PagedSynthEngine::ReadUserPresets()
{
  const std::string path = PresetBankPath(descriptor_);
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file) { file.clear(); file.open(path + ".bak", std::ios::binary | std::ios::ate); }
  if (!file) return true; // No saved bank yet.
  if (file.tellg() < 0 || file.tellg() > 256 * 1024) return false;
  file.seekg(0);
  std::string magic, engineId;
  int version = 0;
  if (!(file >> std::setw(16) >> magic >> version) || !ReadQuoted(file, engineId, 16) ||
      magic != "CCOS_PRESETS" || version != 1 || engineId != descriptor_.id) return false;
  std::vector<PresetState> loaded;
  while (file >> std::ws && file.peek() != std::char_traits<char>::eof())
  {
    SynthEngineState state;
    std::size_t count = 0;
    if (loaded.size() >= kMaxUserPresets || !ReadQuoted(file, state.presetId, 16) || !ReadQuoted(file, state.presetName, 48) ||
        !ReadQuoted(file, state.basePresetId, 64) || !(file >> count) ||
        count != parameters_.size() || !IsUserPresetId(state.presetId)) return false;
    for (std::size_t i = 0; i < count; ++i)
    {
      std::string id;
      int value;
      if (!ReadQuoted(file, id, 64) || !(file >> value) || !state.values.emplace(id, value).second) return false;
    }
    if (!ValidState(state) || std::any_of(loaded.begin(), loaded.end(), [&](const PresetState& p) { return p.info.id == state.presetId; })) return false;
    loaded.push_back({{state.presetId, state.presetName, true, false}, std::move(state.values), state.basePresetId});
  }
  if (file.bad()) return false;
  presets_.erase(std::remove_if(presets_.begin(), presets_.end(), [](const PresetState& p) { return p.info.writable; }), presets_.end());
  for (auto& preset : loaded) presets_.push_back(std::move(preset));
  return true;
}

bool PagedSynthEngine::RefreshPresetCatalog()
{
  return ReadUserPresets();
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
  preset.basePresetId = presetId;
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

void PagedSynthEngine::IndexFactoryPresets()
{
  const std::size_t count = std::min<std::size_t>(FactoryPresetCount(), UINT16_MAX);
  factoryOrder_.resize(count);
  for (std::size_t index = 0; index < count; ++index)
  {
    factoryOrder_[index] = static_cast<std::uint16_t>(index);
  }
  // Same order RegisterPreset keeps: folded display name, then ID.
  std::sort(factoryOrder_.begin(), factoryOrder_.end(), [&](std::uint16_t left, std::uint16_t right) {
    const std::string leftName = FoldAscii(FactoryPresetDisplayName(left));
    const std::string rightName = FoldAscii(FactoryPresetDisplayName(right));
    if (leftName != rightName)
    {
      return leftName < rightName;
    }
    return FactoryPresetId(left) < FactoryPresetId(right);
  });
}

std::size_t PagedSynthEngine::FactoryPresetCount() const
{
  return 0;
}

std::string_view PagedSynthEngine::FactoryPresetId(std::size_t) const
{
  return {};
}

std::string PagedSynthEngine::FactoryPresetName(std::size_t) const
{
  return {};
}

std::size_t PagedSynthEngine::FindFactoryPreset(const std::string& presetId) const
{
  for (std::size_t index = 0; index < factoryOrder_.size(); ++index)
  {
    if (FactoryPresetId(index) == presetId)
    {
      return index;
    }
  }
  return kNoFactoryPreset;
}

std::string PagedSynthEngine::FactoryPresetDisplayName(std::size_t index) const
{
  return NormalizePresetName(FactoryPresetName(index));
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
  audio::SetEnginePreset(engineIndex_, basePresetId_);
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
  return ShouldSyncParameterToRuntime(parameter) &&
         (descriptor_.kind == SynthEngineKind::Sampler || parameter.value != FactoryParameterValue(basePresetId_, parameter));
}

int PagedSynthEngine::FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const
{
  const auto preset = std::find_if(presets_.begin(), presets_.end(), [&](const PresetState& p) { return p.info.id == presetId; });
  if (preset != presets_.end())
  {
    const auto value = preset->values.find(parameter.id);
    if (value != preset->values.end()) return value->second;
  }
  return parameter.defaultValue;
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

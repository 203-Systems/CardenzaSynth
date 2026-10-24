#include "Applications/Synth/GrooveApp.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <new>
#include <set>

#if defined(ESP_PLATFORM)
#include <dirent.h>
#include <esp_heap_caps.h>
#else
#include <filesystem>
#endif

#include "Applications/Synth/Runtime/ProjectStorage.h"
#include "Applications/Synth/UI/Groove/Render/Gfx.h"
#include "Applications/Synth/UI/Groove/Render/GrooveRenderer.h"
#include "Devices/Device.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "OS/MIDI/MIDI.h"

namespace apps {

namespace {

std::unique_ptr<app::Application> CreateGrooveApplication()
{
  return std::make_unique<GrooveApp>();
}

app::ApplicationInfo BuildGrooveInfo()
{
  app::ApplicationInfo info;
  info.id = app::MakeApplicationId("Cardenza", "Synth");
  info.name = "Groovebox";
  info.author = "Cardenza";
  info.factory = &CreateGrooveApplication;
  return info;
}

bool HasWavExtension(const std::string& name)
{
  if (name.size() < 5) return false;
  std::string extension = name.substr(name.size() - 4);
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return extension == ".wav";
}

}  // namespace

void RegisterGrooveApplication()
{
  static const bool registered = app::RegisterApplication(BuildGrooveInfo());
  (void)registered;
}

GrooveApp::GrooveApp() : controller_(*this) {}

std::uint32_t GrooveApp::NowMs() const
{
  if (clock_ != nullptr) return clock_();
  using namespace std::chrono;
  return static_cast<std::uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void GrooveApp::Setup(const std::vector<std::string>& args)
{
  (void)args;
  if (initialized_) return;
  os::midi::Initialize();
  sequencer_.SetMidiSink(&GrooveApp::SendSequencerMidi, this);
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    rack_.AttachEngine(track, kDefaultKinds[track]);
    sampler_.OnTrackEngineChanged(track, kDefaultKinds[track]);
  }
  storageReady_ = Device::Storage::EnsureDirectoryLayout();
  ApplyMix();
  SetMasterVolume(masterVolume_);
  ApplyMasterFx(synth::ProjectData::DefaultMasterFx());
  Device::Display::SetBacklight(backlight_);
  controller_.Restore(groove::ProjectUiSettings{}, "");
  initialized_ = true;
}

void GrooveApp::Tick()
{
  sampler_.Tick();
  rack_.DispatchPendingMessages();
  SyncSamplerTrim();
  // The delay time follows the sequencer tempo.
  const std::uint16_t bpm = sequencer_.Tempo();
  if (bpm != fxTempo_)
  {
    fxTempo_ = bpm;
    synth::audio::SetTempo(static_cast<float>(bpm));
  }
  const std::uint32_t now = NowMs();
  controller_.Tick(now);
  animator_.Update(controller_, now);
}

void GrooveApp::SyncSamplerTrim()
{
  // The sampler engine's start / end parameters are the trim window of the loaded sample or take.
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    const auto& slot = rack_.Slot(track);
    if (slot.kind != synth::SynthEngineKind::Sampler || slot.engine == nullptr) continue;
    if (sampler_.ConsumeTrimReset(track))
    {
      slot.engine->SetParameterValue("sample.start", 0);
      slot.engine->SetParameterValue("sample.end", 127);
    }
    synth::SynthParameterInfo start, end, loop;
    if (slot.engine->GetParameterInfo("sample.start", start) && slot.engine->GetParameterInfo("sample.end", end))
      sampler_.SetDraftTrimWindow(track, start.value, end.value);
    if (slot.engine->GetParameterInfo("sample.loop", loop)) sampler_.SetDraftLoopEnabled(track, loop.value != 0);
  }
}

void GrooveApp::UpdateLeds()
{
  groove::LedFrame frame{};
  controller_.ComputeLeds(frame);
  std::array<std::uint8_t, groove::kKeyCount> levels{};
  animator_.DecorateLeds(controller_, frame, levels);
  animator_.SmoothLeds(levels);
  for (std::size_t row = 0; row < groove::kRows; ++row)
    for (std::size_t column = 0; column < groove::kColumns; ++column)
      Device::LED::SetKey(row, column, levels[groove::KeyIndex(row, column)]);
  Device::LED::SetEnabled(true);
}

void GrooveApp::RenderFrame()
{
  const auto start = std::chrono::steady_clock::now();
  groove::Gfx gfx(Device::Display::MutableFrameBuffer());
  groove::RenderFrame(gfx, controller_, animator_);
#if defined(ESP_PLATFORM)
  // Frame budget report on the serial console every ~10 s (600 frames at 60 fps).
  const auto elapsed = static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
  renderTotalUs_ += elapsed;
  renderMaxUs_ = std::max(renderMaxUs_, elapsed);
  if (++renderFrames_ >= 600)
  {
    std::printf("Groove: render avg=%luus max=%luus heap=%u min=%u\n", static_cast<unsigned long>(renderTotalUs_ / renderFrames_),
                static_cast<unsigned long>(renderMaxUs_), static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)));
    renderFrames_ = 0;
    renderTotalUs_ = 0;
    renderMaxUs_ = 0;
  }
#else
  (void)start;
#endif
}

void GrooveApp::HandleMatrixKeyEvent(std::size_t row, std::size_t column, bool pressed)
{
  controller_.KeyEvent(row, column, pressed, NowMs());
}

void GrooveApp::HandleHomeButtonEvent(bool pressed)
{
  controller_.HomeButton(pressed, NowMs());
}

// ---------------------------------------------------------------------------------------------
// Engines and notes

synth::ISynthEngine* GrooveApp::Engine(std::size_t track)
{
  return track < synth::kSynthTrackCount ? rack_.Slot(track).engine.get() : nullptr;
}

const synth::ISynthEngine* GrooveApp::Engine(std::size_t track) const
{
  return track < synth::kSynthTrackCount ? rack_.Slot(track).engine.get() : nullptr;
}

synth::SynthEngineKind GrooveApp::EngineKind(std::size_t track) const
{
  return track < synth::kSynthTrackCount ? rack_.Slot(track).kind : synth::SynthEngineKind::None;
}

bool GrooveApp::SwitchEngine(std::size_t track, synth::SynthEngineKind kind, std::string& error)
{
  if (track >= synth::kSynthTrackCount) return false;
  auto& slot = rack_.Slot(track);
  if (slot.kind == kind) return true;
  const auto sample = sampler_.BuildUiState(track);
  if (sample.recording || sample.hasDraft)
  {
    error = "SAVE SAMPLE FIRST";
    return false;
  }
  auto engine = synth::CreateSynthEngine(kind);
  if (!engine || !slot.engine || !engine->CanReplace(*slot.engine))
  {
    error = "ENGINE UNAVAILABLE";
    return false;
  }
  (void)engine->RefreshPresetCatalog();
  synth::audio::AllNotesOff(track);
  sampler_.OnTrackEngineChanged(track, synth::SynthEngineKind::None);
  engine->ReplaceBinding(*slot.engine);
  slot.engine = std::move(engine);
  slot.kind = kind;
  sampler_.OnTrackEngineChanged(track, kind);
  return true;
}

void GrooveApp::NoteOn(std::size_t track, std::uint8_t note, std::uint8_t velocity)
{
  if (track >= synth::kSynthTrackCount) return;
  os::midi::Send(midi::MidiPacket::NoteOn(0, note, velocity), rack_.PortForTrack(track));
}

void GrooveApp::NoteOff(std::size_t track, std::uint8_t note)
{
  if (track >= synth::kSynthTrackCount) return;
  os::midi::Send(midi::MidiPacket::NoteOff(0, note, 0), rack_.PortForTrack(track));
}

void GrooveApp::Panic()
{
  sequencer_.Stop();
  sequencer_.SetRecordArmed(false);
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) synth::audio::AllNotesOff(track);
}

void GrooveApp::SendSequencerMidi(void* context, std::size_t trackIndex, const midi::MidiPacket& packet)
{
  (void)context;
  if (trackIndex >= synth::kSynthTrackCount) return;
  // Bypass the UI queue: the audio runtime synchronizes stable track indices.
  if (packet.status == midi::MidiStatus::NoteOn && packet.data[2] != 0)
    synth::audio::NoteOn(trackIndex, packet.data[1], packet.data[2]);
  else if (packet.status == midi::MidiStatus::NoteOff || packet.status == midi::MidiStatus::NoteOn)
    synth::audio::NoteOff(trackIndex, packet.data[1]);
  else if (packet.status == midi::MidiStatus::ControlChange && (packet.data[1] == 120 || packet.data[1] == 123))
    synth::audio::AllNotesOff(trackIndex);
}

// ---------------------------------------------------------------------------------------------
// Sampler

void GrooveApp::PreviewSample(std::size_t track)
{
  std::uint8_t note = 60;
  if (const auto* engine = Engine(track))
  {
    synth::SynthParameterInfo root;
    if (engine->GetParameterInfo("sample.root", root)) note = static_cast<std::uint8_t>(std::clamp(root.value, 0, 127));
  }
  (void)sampler_.PreviewCurrentSample(track, note);
}

std::vector<std::string> GrooveApp::ListSamples() const
{
  std::set<std::string> names;
  const std::string directory = Device::Storage::SampleDirectoryPath();
  if (!Device::Storage::EnsureDirectoryLayout() || directory.empty()) return {};
#if defined(ESP_PLATFORM)
  DIR* handle = opendir(directory.c_str());
  if (handle == nullptr) return {};
  while (const dirent* entry = readdir(handle))
    if (entry->d_name[0] != '\0' && HasWavExtension(entry->d_name)) names.insert(entry->d_name);
  closedir(handle);
#else
  namespace fs = std::filesystem;
  std::error_code error;
  for (const fs::directory_entry& entry : fs::directory_iterator(directory, error))
  {
    if (error) break;
    if (!entry.is_regular_file(error)) continue;
    const std::string name = entry.path().filename().string();
    if (HasWavExtension(name)) names.insert(name);
  }
#endif
  return std::vector<std::string>(names.begin(), names.end());
}

// ---------------------------------------------------------------------------------------------
// Mixer

void GrooveApp::ApplyMix()
{
  const bool anySolo = std::any_of(soloed_.begin(), soloed_.end(), [](bool soloed) { return soloed; });
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    synth::audio::SetTrackGain(track, static_cast<float>(levels_[track]) / 100.0f);
    synth::audio::SetTrackMuted(track, muted_[track] || (anySolo && !soloed_[track]));
  }
}

void GrooveApp::SetTrackLevel(std::size_t track, std::uint8_t level)
{
  if (track >= synth::kSynthTrackCount) return;
  levels_[track] = std::min<std::uint8_t>(level, 100);
  ApplyMix();
}

std::uint8_t GrooveApp::TrackLevel(std::size_t track) const
{
  return track < synth::kSynthTrackCount ? levels_[track] : 0;
}

void GrooveApp::SetTrackMuted(std::size_t track, bool muted)
{
  if (track >= synth::kSynthTrackCount) return;
  muted_[track] = muted;
  ApplyMix();
}

bool GrooveApp::TrackMuted(std::size_t track) const
{
  return track < synth::kSynthTrackCount && muted_[track];
}

void GrooveApp::SetTrackSoloed(std::size_t track, bool soloed)
{
  if (track >= synth::kSynthTrackCount) return;
  soloed_[track] = soloed;
  ApplyMix();
}

bool GrooveApp::TrackSoloed(std::size_t track) const
{
  return track < synth::kSynthTrackCount && soloed_[track];
}

bool GrooveApp::TrackAudible(std::size_t track) const
{
  if (track >= synth::kSynthTrackCount || muted_[track]) return false;
  const bool anySolo = std::any_of(soloed_.begin(), soloed_.end(), [](bool soloed) { return soloed; });
  return !anySolo || soloed_[track];
}

void GrooveApp::SetMasterVolume(std::uint8_t percent)
{
  masterVolume_ = std::min<std::uint8_t>(percent, 100);
  // The codec steps about 0.95 dB per device percent, so a linear map would leave the lower half of
  // the UI range inaudible. UI 1..100 spans roughly -42..0 dB; 0 is mute.
  const int device = masterVolume_ == 0 ? 0 : 100 - (100 - masterVolume_) * 42 / 100;
  Device::Audio::SetOutputVolume(static_cast<std::uint8_t>(device));
}

std::uint8_t GrooveApp::MasterVolume() const
{
  return masterVolume_;
}

float GrooveApp::TrackActivity(std::size_t track) const
{
  return synth::audio::TrackActivity(track);
}

float GrooveApp::OutputPeak() const
{
  return synth::audio::OutputPeak();
}

void GrooveApp::SetMasterFx(std::size_t param, int value)
{
  if (param >= synth::fx::kMasterParamCount) return;
  synth::audio::SetMasterFx(static_cast<synth::fx::MasterParam>(param), value);
}

int GrooveApp::MasterFx(std::size_t param) const
{
  return param < synth::fx::kMasterParamCount ? synth::audio::MasterFx(static_cast<synth::fx::MasterParam>(param)) : 0;
}

float GrooveApp::FxReturnLevel(std::size_t bus) const
{
  return synth::audio::FxReturnLevel(bus);
}

bool GrooveApp::FxBusAvailable(std::size_t bus) const
{
  return synth::audio::FxBusAvailable(bus);
}

void GrooveApp::ApplyMasterFx(const std::array<std::uint8_t, synth::fx::kMasterParamCount>& values)
{
  for (std::size_t param = 0; param < values.size(); ++param) SetMasterFx(param, values[param]);
}

// ---------------------------------------------------------------------------------------------
// Projects

std::vector<std::string> GrooveApp::ListProjects() const
{
  return synth::ListProjects();
}

groove::SaveResult GrooveApp::SaveProject(const std::string& name, bool overwrite, const groove::ProjectUiSettings& settings, std::string& message)
{
  if (!synth::IsProjectNameValid(name))
  {
    message = "INVALID NAME";
    return groove::SaveResult::Failed;
  }
  synth::ProjectData project;
  project.sequence = sequencer_.CaptureState();
  if (!project.sequence)
  {
    message = "NO MEMORY";
    return groove::SaveResult::Failed;
  }
  project.activeTrack = settings.activeTrack;
  project.octave = settings.keyboard.octave;
  project.root = settings.keyboard.root;
  project.scale = settings.keyboard.scale;
  project.layout = static_cast<int>(settings.keyboard.layout);
  project.ledBrightness = Device::LED::GlobalBrightness();
  project.backlight = backlight_;
  project.mixer.level = levels_;
  project.mixer.muted = muted_;
  project.mixer.soloed = soloed_;
  project.mixer.master = masterVolume_;
  for (std::size_t param = 0; param < synth::fx::kMasterParamCount; ++param)
    project.masterFx[param] = static_cast<std::uint8_t>(MasterFx(param));
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    const auto sample = sampler_.BuildUiState(track);
    if (sample.recording || sample.hasDraft)
    {
      message = "SAVE SAMPLE FIRST";
      return groove::SaveResult::Failed;
    }
    const auto& slot = rack_.Slot(track);
    if (!slot.engine)
    {
      message = "ENGINE OFFLINE";
      return groove::SaveResult::Failed;
    }
    project.kinds[track] = slot.kind;
    project.engines[track] = slot.engine->CaptureState();
    project.samples[track] = sample.sampleFileName;
  }
  const auto result = synth::SaveProjectFile(name, project, overwrite);
  storageReady_ = result != synth::ProjectResult::StorageError;
  switch (result)
  {
    case synth::ProjectResult::Ok: return groove::SaveResult::Ok;
    case synth::ProjectResult::Exists: return groove::SaveResult::Exists;
    case synth::ProjectResult::Invalid: message = "INVALID PROJECT"; return groove::SaveResult::Failed;
    case synth::ProjectResult::StorageError:
    default: message = "SD CARD ERROR"; return groove::SaveResult::Failed;
  }
}

bool GrooveApp::ReplaceSession(const std::string& name, synth::ProjectData& project, bool createNew, std::string& message)
{
  (void)name;
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    const auto sample = sampler_.BuildUiState(track);
    if (sample.recording || sample.hasDraft)
    {
      message = "SAVE SAMPLE FIRST";
      return false;
    }
    if (!rack_.Slot(track).engine)
    {
      message = "ENGINE OFFLINE";
      return false;
    }
  }

  std::array<std::unique_ptr<synth::ISynthEngine>, synth::kSynthTrackCount> engines;
  std::array<synth::SamplerManager::PreparedSample, synth::kSynthTrackCount> samples;
  // Validate and preload everything while the current session stays intact.
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    engines[track] = synth::CreateSynthEngine(project.kinds[track]);
    if (!engines[track])
    {
      message = "NO MEMORY";
      return false;
    }
    (void)engines[track]->RefreshPresetCatalog();
    if ((!createNew && !engines[track]->RestoreState(project.engines[track])) || !engines[track]->CanReplace(*rack_.Slot(track).engine))
    {
      message = "INVALID SOUND";
      return false;
    }
    if (project.samples[track].empty()) continue;
    bool shared = false;
    for (std::size_t previous = 0; previous < track; ++previous)
      if (project.samples[previous] == project.samples[track])
      {
        samples[track] = samples[previous];
        shared = true;
        break;
      }
    if (!shared && !synth::SamplerManager::PrepareSample(project.samples[track], samples[track]))
    {
      message = "SAMPLE MISSING";
      return false;
    }
  }

  // No file reads or allocations that can fail remain: swap bindings in place.
  sequencer_.Stop();
  sequencer_.SetRecordArmed(false);
  sequencer_.ClearSelectedSteps();
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) sampler_.OnTrackEngineChanged(track, synth::SynthEngineKind::None);
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    auto& slot = rack_.Slot(track);
    synth::audio::AllNotesOff(track);
    engines[track]->ReplaceBinding(*slot.engine);
    slot.engine = std::move(engines[track]);
    slot.kind = project.kinds[track];
    sampler_.OnTrackEngineChanged(track, slot.kind);
    if (!project.samples[track].empty()) (void)sampler_.ApplySample(track, samples[track]);
    // The engine already carries the project's saved trim.
    (void)sampler_.ConsumeTrimReset(track);
  }
  sequencer_.RestoreState(*project.sequence);
  levels_ = project.mixer.level;
  muted_ = project.mixer.muted;
  soloed_ = project.mixer.soloed;
  ApplyMix();
  SetMasterVolume(project.mixer.master);
  ApplyMasterFx(project.masterFx);
  SetBacklight(project.backlight);
  Device::LED::SetGlobalBrightness(static_cast<std::uint8_t>(project.ledBrightness));
  return true;
}

bool GrooveApp::LoadProject(const std::string& name, groove::ProjectUiSettings& settings, std::string& message)
{
  synth::ProjectData project;
  if (synth::LoadProjectFile(name, project) != synth::ProjectResult::Ok)
  {
    message = "INVALID PROJECT";
    return false;
  }
  if (!ReplaceSession(name, project, false, message)) return false;
  settings.activeTrack = project.activeTrack;
  settings.keyboard.octave = project.octave;
  settings.keyboard.root = project.root;
  settings.keyboard.scale = project.scale;
  settings.keyboard.layout = static_cast<groove::KeyboardLayout>(std::clamp(project.layout, 0, 2));
  return true;
}

bool GrooveApp::NewProject(groove::ProjectUiSettings& settings, std::string& message)
{
  synth::ProjectData project;
  project.sequence.reset(new (std::nothrow) synth::SynthSequencer::ProjectState);
  if (!project.sequence)
  {
    message = "NO MEMORY";
    return false;
  }
  project.kinds = kDefaultKinds;
  project.mixer.master = masterVolume_;
  project.backlight = backlight_;
  project.ledBrightness = Device::LED::GlobalBrightness();
  if (!ReplaceSession("", project, true, message)) return false;
  settings = groove::ProjectUiSettings{};
  return true;
}

// ---------------------------------------------------------------------------------------------
// System

void GrooveApp::SetLedBrightness(std::uint8_t level)
{
  Device::LED::SetGlobalBrightness(std::min<std::uint8_t>(level, Device::LED::kBrightnessMax));
}

std::uint8_t GrooveApp::LedBrightness() const
{
  return Device::LED::GlobalBrightness();
}

void GrooveApp::SetBacklight(std::uint8_t level)
{
  backlight_ = std::max<std::uint8_t>(level, 10);
  Device::Display::SetBacklight(backlight_);
}

std::uint8_t GrooveApp::Backlight() const
{
  return backlight_;
}

groove::SystemInfo GrooveApp::QuerySystem() const
{
  groove::SystemInfo info;
  info.backend = Device::BackendName();
  info.storageReady = storageReady_;
#if defined(ESP_PLATFORM)
  info.freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  info.minimumFreeHeap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
#endif
  return info;
}

}  // namespace apps

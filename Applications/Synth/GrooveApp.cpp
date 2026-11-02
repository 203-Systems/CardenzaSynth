#include "Applications/Synth/GrooveApp.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <new>
#include <set>
#include <utility>

#if defined(ESP_PLATFORM)
#include <dirent.h>
#include <esp_heap_caps.h>
#else
#include <filesystem>
#endif

#include "Applications/Synth/Runtime/ProjectStorage.h"
#include "Applications/Synth/Runtime/SynthStorage.h"
#include "Applications/Synth/UI/Groove/Render/Gfx.h"
#include "Applications/Synth/UI/Groove/Render/GrooveRenderer.h"
#include "Devices/Device.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "OS/MIDI/MIDI.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Settings.h"

namespace apps {

namespace {

// Session handover files in Apps/Cardenza/Synth/Session.
constexpr const char* kSessionProject = "session.ccproj";
// After a key: this much quiet, or this long at most while something is playing.
constexpr std::uint32_t kSessionQuietMs = 1500;
constexpr std::uint32_t kSessionLongestWaitMs = 60000;
constexpr std::uint32_t kSessionRetryMs = 2000;
constexpr std::uint32_t kSessionFailedRetryMs = 30000;
constexpr std::uint32_t kCardCheckMs = 2000;
constexpr std::uint32_t kCardProbeMs = 10000;
constexpr std::uint32_t kOrphanCheckMs = 200;
constexpr std::uint32_t kOrphanConfirmMs = 50;
#if defined(ESP_PLATFORM)
// What the autosave needs at the same time, with a record as one block.
constexpr std::size_t kSessionHeapBytes = 12 * 1024;
constexpr std::size_t kSessionBlockBytes = 5 * 1024;
constexpr std::size_t kProjectSaveHeapBytes = 8 * 1024;
#endif

os::storage::Settings& AppSettings()
{
  return os::storage::AppSettings(synth::storage::kAppId);
}

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

GrooveApp::~GrooveApp()
{
  // Silence the engines and hand the sample memory back before the next app starts.
  sampler_.KeepTakesOnCard();
  sequencer_.ArpReleaseAll();
  sequencer_.Stop();
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    synth::audio::AllNotesOff(track);
    sampler_.OnTrackEngineChanged(track, synth::SynthEngineKind::None);
  }
}

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
  synth::audio::SetLineInput(&Device::Audio::ReadComputerAudio);
  sequencer_.SetMidiSink(&GrooveApp::SendSequencerMidi, this);
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    rack_.AttachEngine(track, kDefaultKinds[track]);
    sampler_.OnTrackEngineChanged(track, kDefaultKinds[track]);
  }
  storageReady_ = synth::storage::EnsureLayout();
  ApplyMix();
  ApplyMasterFx(synth::ProjectData::DefaultMasterFx());
  // Volume, key LEDs and screen are device settings: the system applied them at boot.
  RestorePlaySettings();
  // The autosave is in internal flash: it comes back with or without a card.
  (void)RestoreSessionState();
  cardWasMounted_ = os::storage::CardMounted();
  if (cardWasMounted_)
  {
    const int take = sampler_.RestoreTakes();
    if (take >= 0) controller_.ResumeTake(static_cast<std::size_t>(take));
  }
  initialized_ = true;
}

void GrooveApp::Suspend()
{
  StorePlaySettings();
  sampler_.KeepTakesOnCard();
  (void)SaveSessionState();
}

void GrooveApp::RestorePlaySettings()
{
  const auto& settings = AppSettings();
  groove::ProjectUiSettings ui;
  groove::KeyboardSettings& keys = ui.keyboard;
  const groove::KeyboardSettings defaults{};
  keys.layout = static_cast<groove::KeyboardLayout>(
      settings.GetInt("keys.layout", static_cast<int>(defaults.layout), 0, static_cast<int>(groove::kLayoutNames.size()) - 1));
  keys.scale = static_cast<std::size_t>(settings.GetInt("keys.scale", static_cast<int>(defaults.scale), 0, static_cast<int>(groove::kScales.size()) - 1));
  keys.root = settings.GetInt("keys.root", defaults.root, 0, 11);
  keys.octave = settings.GetInt("keys.octave", defaults.octave, -3, 3);
  keys.lock = static_cast<groove::ScaleLock>(
      settings.GetInt("keys.lock", static_cast<int>(defaults.lock), 0, static_cast<int>(groove::kScaleLockNames.size()) - 1));
  keys.multiSelect = settings.GetInt("keys.select", defaults.multiSelect ? 1 : 0, 0, 1) != 0;
  keys.velocity = static_cast<std::uint8_t>(settings.GetInt("keys.velocity", defaults.velocity, 1, 127));
  // The keyboard belongs to the project now; the values older firmware kept in the app settings
  // only start a session that has no project of its own (the session file overrides them).
  controller_.Restore(ui, "");
  storedMultiSelect_ = keys.multiSelect;
}

void GrooveApp::StorePlaySettings()
{
  // Polled every tick. The system writes the file a second after the last change, and when the app
  // is suspended.
  const bool multiSelect = controller_.Settings().keyboard.multiSelect;
  if (multiSelect == storedMultiSelect_) return;
  AppSettings().SetInt("keys.select", multiSelect ? 1 : 0);
  storedMultiSelect_ = multiSelect;
}

void GrooveApp::ApplyPlaySettings(const synth::ProjectData& project, groove::ProjectUiSettings& settings)
{
  if (!project.hasPlay) return;
  groove::KeyboardSettings& keys = settings.keyboard;
  keys.octave = std::clamp(project.octave, -3, 3);
  keys.root = std::clamp(project.root, 0, 11);
  keys.scale = std::min(project.scale, groove::kScales.size() - 1);
  keys.layout = static_cast<groove::KeyboardLayout>(std::clamp(project.layout, 0, static_cast<int>(groove::kLayoutNames.size()) - 1));
  keys.lock = static_cast<groove::ScaleLock>(std::clamp(project.lock, 0, static_cast<int>(groove::kScaleLockNames.size()) - 1));
  keys.velocity = static_cast<std::uint8_t>(std::clamp(project.velocity, 1, 127));
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    const auto& play = project.play[track];
    settings.chords[track].chord = static_cast<groove::ChordType>(std::clamp(play.chord, 0, static_cast<int>(groove::kChordTypeCount) - 1));
    settings.chords[track].voicing = static_cast<std::uint8_t>(std::clamp(play.voicing, 0, static_cast<int>(groove::kVoicingNames.size()) - 1));
    sequencer_.SetArpSettings(track, play.arp);
  }
}

synth::SessionStore::SaveResult GrooveApp::SaveSessionState()
{
  synth::ProjectData project;
  std::string message;
  if (!CaptureProject(controller_.Settings(), project, message, true)) return synth::SessionStore::SaveResult::Failed;
  const auto& view = controller_.State();
  synth::SessionInfo info;
  info.name = view.projectName;
  info.dirty = view.dirty;
  return session_.Save(info, project);
}

bool GrooveApp::RestoreSessionState()
{
  if (!synth::SessionStore::BeginRestore()) return false;
  synth::ProjectData project;
  synth::SessionInfo info;
  std::string message;
  const bool restored = session_.Load(info, project) && ReplaceSession(info.name, project, false, message, true);
  if (restored)
  {
    groove::ProjectUiSettings ui = controller_.Settings();
    // Every project opens on the first track, whichever it was left on.
    ui.activeTrack = synth::kDefaultActiveTrack;
    ApplyPlaySettings(project, ui);
    // The session comes back with its unsaved state, so loading another project still asks first.
    controller_.Restore(ui, synth::IsProjectNameValid(info.name) ? info.name : std::string(), info.dirty);
  }
  synth::SessionStore::FinishRestore();
  return restored;
}

// Nothing that could be heard, and no finger on a key.
bool GrooveApp::Quiet(std::uint32_t now) const
{
  return now - lastKeyAt_ >= kSessionQuietMs && keysHeld_ == 0 && !sequencer_.Playing() && sampler_.RecordingTrack() < 0;
}

// The card comes and goes on its own: what waits for it gets it.
void GrooveApp::CheckCard(std::uint32_t now)
{
  if (now - lastCardCheckAt_ < kCardCheckMs) return;
  lastCardCheckAt_ = now;
  // Looking for a card that is not there takes a moment: only for something that waits for it, and
  // only when nobody is playing.
  const bool waiting = std::any_of(pendingSamples_.begin(), pendingSamples_.end(), [](const std::string& name) { return !name.empty(); });
  if (!cardWasMounted_ && waiting && Quiet(now) && now - lastCardProbeAt_ >= kCardProbeMs)
  {
    lastCardProbeAt_ = now;
    (void)os::storage::CardPresent();
  }
  const bool mounted = os::storage::CardMounted();
  if (mounted == cardWasMounted_) return;
  cardWasMounted_ = mounted;
  if (!mounted || !synth::storage::EnsureLayout()) return;
  storageReady_ = true;
  synth::audio::RetrySampleStreams();
  ApplyPendingSamples();
  const int take = sampler_.RestoreTakes();
  if (take >= 0) controller_.ResumeTake(static_cast<std::size_t>(take));
}

// The samples the autosave names, for when it came back before the card did.
void GrooveApp::ApplyPendingSamples()
{
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    if (pendingSamples_[track].empty()) continue;
    const std::string name = std::move(pendingSamples_[track]);
    pendingSamples_[track].clear();
    if (rack_.Slot(track).kind != synth::SynthEngineKind::Sampler || !sampler_.BuildUiState(track).sampleFileName.empty()) continue;
    synth::SamplerManager::PreparedSample sample;
    if (!synth::SamplerManager::PrepareSample(name, sample)) continue;
    (void)sampler_.ApplySample(track, sample);
    // The engine already carries the saved trim.
    (void)sampler_.ConsumeTrimReset(track);
  }
}

void GrooveApp::KeepSessionCurrent(std::uint32_t now)
{
  if (!sessionChanged_ || sampler_.RecordingTrack() >= 0) return;
  // Writing to flash can stop both cores for a moment, which sound would notice: so when nothing is
  // heard, or else only when the changes have waited long enough to be worth that risk.
  if (!Quiet(now) && now - sessionChangedAt_ < kSessionLongestWaitMs) return;
  if (now - lastSessionTryAt_ < (autosaveFailureShown_ ? kSessionFailedRetryMs : kSessionRetryMs)) return;
  lastSessionTryAt_ = now;
#if defined(ESP_PLATFORM)
  // A plain allocation that fails ends the program. Later is fine.
  if (heap_caps_get_free_size(MALLOC_CAP_8BIT) < kSessionHeapBytes || heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < kSessionBlockBytes) return;
#endif
  const auto result = SaveSessionState();
  if (result == synth::SessionStore::SaveResult::Failed)
  {
    if (!autosaveFailureShown_) controller_.ReportAutosaveProblem("AUTOSAVE FAILED: FN+S TO CARD");
    autosaveFailureShown_ = true;
    return;
  }
  autosaveFailureShown_ = false;
  sessionChanged_ = false;
  if (result == synth::SessionStore::SaveResult::Partial)
  {
    if (!autosavePartialShown_) controller_.ReportAutosaveProblem("TOO LONG TO AUTOSAVE: FN+S TO CARD");
    autosavePartialShown_ = true;
  }
  else autosavePartialShown_ = false;
}


void GrooveApp::Tick()
{
  ReadComputerMidi();
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
  StorePlaySettings();
  CheckCard(now);
  KeepSessionCurrent(now);
  EndOrphanNotes(now);
}

// The safety net under every path that starts a note: the audio side's notes against those someone
// is sounding (pads, steps, arpeggiator, sample preview). A note nobody claims gets a second look once
// any note-off on its way has arrived, and if it is still there, its note-off from here.
void GrooveApp::EndOrphanNotes(std::uint32_t now)
{
  // Keys going down and up keep the audio side a moment behind; it can wait until they rest.
  if (now - lastKeyAt_ < kOrphanCheckMs) return;
  if (now - lastOrphanCheckAt_ < (orphanCandidates_ ? kOrphanConfirmMs : kOrphanCheckMs)) return;
  lastOrphanCheckAt_ = now;
  const std::uint32_t shed = synth::audio::ShedVoiceCount();
  if (shed != loggedShedVoices_) printf("Groove: too much to render: %u voices shed so far\n", static_cast<unsigned>(shed));
  loggedShedVoices_ = shed;
  const bool confirming = orphanCandidates_;
  orphanCandidates_ = false;
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    const synth::audio::NoteSet held = synth::audio::HeldNotes(track);
    synth::audio::NoteSet owned{};
    controller_.AddHeldPadNotes(track, owned);
    sequencer_.AddSoundingNotes(track, owned);
    for (std::size_t word = 0; word < owned.size(); ++word) owned[word] |= computerNotes_[track][word];
    // A sample preview plays the sample's root note, whichever that is.
    const bool preview = sampler_.Previewing(track);
    for (std::size_t word = 0; word < held.size(); ++word)
    {
      const std::uint32_t orphans = preview ? 0u : held[word] & ~owned[word];
      for (std::size_t bit = 0; confirming && bit < 32; ++bit)
      {
        if ((orphans & orphanNotes_[track][word] & (1u << bit)) == 0) continue;
        synth::audio::NoteOff(track, static_cast<std::uint8_t>(word * 32 + bit));
        ++endedOrphanNotes_;
        printf("Groove: ended a note nobody held: track %u note %u\n", static_cast<unsigned>(track), static_cast<unsigned>(word * 32 + bit));
      }
      orphanNotes_[track][word] = confirming ? 0u : orphans;
      orphanCandidates_ = orphanCandidates_ || (!confirming && orphans != 0);
    }
  }
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
  // The eight edge keys (ESC TAB FN CTRL, ADD SUB OK PLAY) never go above the dimmest glow: their
  // state shows on the screen's edge strips, the keys only say they are there.
  for (std::size_t row = 0; row < groove::kRows; ++row)
    for (const std::size_t column : {std::size_t{0}, groove::kColumns - 1})
    {
      std::uint8_t& level = levels[groove::KeyIndex(row, column)];
      level = std::min(level, groove::kLedEdgeMax);
    }
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
    // The key LED levels this frame (0..f per key, rows top to bottom), to check the lights from
    // the serial console.
    char leds[4 * 15 + 1] = {};
    for (std::size_t row = 0; row < 4; ++row)
    {
      for (std::size_t column = 0; column < 14; ++column)
        leds[row * 15 + column] = "0123456789abcdef"[Device::LED::Brightness(row, column) & 0x0F];
      leds[row * 15 + 14] = row == 3 ? '\0' : ' ';
    }
    std::printf("Groove: leds %s master=%u\n", leds, static_cast<unsigned>(Device::LED::GlobalBrightness()));
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
  const std::uint32_t now = NowMs();
  controller_.KeyEvent(row, column, pressed, now);
  if (!sessionChanged_) sessionChangedAt_ = now;
  sessionChanged_ = true;
  lastKeyAt_ = now;
  keysHeld_ = pressed ? keysHeld_ + 1 : std::max(0, keysHeld_ - 1);
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
  SendComputerMidi(0x90, track, note, velocity);
}

void GrooveApp::NoteOff(std::size_t track, std::uint8_t note)
{
  if (track >= synth::kSynthTrackCount) return;
  os::midi::Send(midi::MidiPacket::NoteOff(0, note, 0), rack_.PortForTrack(track));
  SendComputerMidi(0x80, track, note, 0);
}

// What the device plays goes out to a computer on USB too: track n on MIDI channel n.
void GrooveApp::SendComputerMidi(std::uint8_t status, std::size_t track, std::uint8_t data1, std::uint8_t data2)
{
  const std::uint8_t packet[4] = {static_cast<std::uint8_t>(status >> 4), static_cast<std::uint8_t>(status | (track & 0x0F)), data1, data2};
  (void)Device::USB::WriteMidi(packet);
}

// A computer plays the device over USB: MIDI channels 1-4 are tracks 1-4, any other channel the current
// track. Its notes are owned here (EndOrphanNotes) and end when it disconnects.
void GrooveApp::ReadComputerMidi()
{
  const bool connected = Device::USB::Connected();
  if (!connected && computerConnected_) ReleaseComputerNotes();
  computerConnected_ = connected;
  std::uint8_t packet[4];
  for (int count = 0; count < 64 && Device::USB::ReadMidi(packet); ++count)
  {
    const std::uint8_t status = packet[1] & 0xF0;
    const std::uint8_t channel = packet[1] & 0x0F;
    const std::size_t track = channel < synth::kSynthTrackCount ? channel : controller_.State().track;
    const std::uint8_t data1 = packet[2] & 0x7F;
    const std::uint8_t data2 = packet[3] & 0x7F;
    auto& held = computerNotes_[track][data1 / 32];
    const std::uint32_t bit = 1u << (data1 % 32);
    if (status == 0x90 && data2 != 0)
    {
      synth::audio::NoteOn(track, data1, data2);
      held |= bit;
    }
    else if (status == 0x80 || status == 0x90)
    {
      synth::audio::NoteOff(track, data1);
      held &= ~bit;
    }
    else if (status == 0xB0 && (data1 == 120 || data1 == 123))
    {
      ReleaseComputerNotes(track);
    }
    else if (status == 0xB0)
    {
      os::midi::Send(midi::MidiPacket::ControlChange(0, data1, data2), rack_.PortForTrack(track));
    }
  }
}

void GrooveApp::ReleaseComputerNotes(std::size_t onlyTrack)
{
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    if (onlyTrack < synth::kSynthTrackCount && track != onlyTrack) continue;
    for (std::size_t word = 0; word < computerNotes_[track].size(); ++word)
      for (std::size_t bit = 0; bit < 32; ++bit)
        if (computerNotes_[track][word] & (1u << bit)) synth::audio::NoteOff(track, static_cast<std::uint8_t>(word * 32 + bit));
    computerNotes_[track] = {};
  }
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
  {
    synth::audio::NoteOn(trackIndex, packet.data[1], packet.data[2]);
    SendComputerMidi(0x90, trackIndex, packet.data[1], packet.data[2]);
  }
  else if (packet.status == midi::MidiStatus::NoteOff || packet.status == midi::MidiStatus::NoteOn)
  {
    synth::audio::NoteOff(trackIndex, packet.data[1]);
    SendComputerMidi(0x80, trackIndex, packet.data[1], 0);
  }
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
  const std::string directory = synth::storage::SampleDirectory();
  if (!synth::storage::EnsureLayout() || directory.empty()) return {};
  // Ours, and what the 0.1.0 release left.
  for (const std::string& folder : {directory, synth::storage::ReleaseSampleDirectory()})
  {
#if defined(ESP_PLATFORM)
    DIR* handle = opendir(folder.c_str());
    if (handle == nullptr) continue;
    while (const dirent* entry = readdir(handle))
      if (entry->d_name[0] != '\0' && HasWavExtension(entry->d_name)) names.insert(entry->d_name);
    closedir(handle);
#else
    namespace fs = std::filesystem;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, error))
    {
      if (error) break;
      if (!entry.is_regular_file(error)) continue;
      const std::string name = entry.path().filename().string();
      if (HasWavExtension(name)) names.insert(name);
    }
#endif
  }
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
  // A device setting (System/settings.cfg) on the system's perceptual curve.
  os::storage::SetOutputVolume(percent);
}

std::uint8_t GrooveApp::MasterVolume() const
{
  return os::storage::OutputVolume();
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

bool GrooveApp::CaptureProject(const groove::ProjectUiSettings& settings, synth::ProjectData& project, std::string& message, bool forSession) const
{
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    const auto sample = sampler_.BuildUiState(track);
    if (!forSession && (sample.recording || sample.hasDraft))
    {
      message = "SAVE SAMPLE FIRST";
      return false;
    }
    const auto& slot = rack_.Slot(track);
    if (!slot.engine)
    {
      message = "ENGINE OFFLINE";
      return false;
    }
    project.kinds[track] = slot.kind;
    project.engines[track] = forSession ? slot.engine->CaptureChanges() : slot.engine->CaptureState();
    project.samples[track] = sample.sampleFileName;
    // Still waiting for the card it is on.
    if (forSession && sample.sampleFileName.empty() && slot.kind == synth::SynthEngineKind::Sampler) project.samples[track] = pendingSamples_[track];
  }
  project.sequence = sequencer_.CaptureState();
  if (!project.sequence)
  {
    message = "NO MEMORY";
    return false;
  }
  project.activeTrack = settings.activeTrack;
  // The play settings belong to the project: the keyboard, each track's chord and arpeggiator.
  project.hasPlay = true;
  project.octave = settings.keyboard.octave;
  project.root = settings.keyboard.root;
  project.scale = settings.keyboard.scale;
  project.layout = static_cast<int>(settings.keyboard.layout);
  project.lock = static_cast<int>(settings.keyboard.lock);
  project.velocity = settings.keyboard.velocity;
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
  {
    project.play[track].chord = static_cast<int>(settings.chords[track].chord);
    project.play[track].voicing = settings.chords[track].voicing;
    project.play[track].arp = sequencer_.GetArpSettings(track);
  }
  // Volume, key LEDs and screen are device settings; their old fields are still written.
  project.ledBrightness = os::storage::KeyLedBrightness();
  project.backlight = os::storage::ScreenBacklight();
  project.mixer.master = os::storage::OutputVolume();
  project.mixer.level = levels_;
  project.mixer.muted = muted_;
  project.mixer.soloed = soloed_;
  for (std::size_t param = 0; param < synth::fx::kMasterParamCount; ++param)
    project.masterFx[param] = static_cast<std::uint8_t>(MasterFx(param));
  return true;
}

groove::SaveResult GrooveApp::SaveProject(const std::string& name, bool overwrite, const groove::ProjectUiSettings& settings, std::string& message)
{
  if (!synth::IsProjectNameValid(name))
  {
    message = "INVALID NAME";
    return groove::SaveResult::Failed;
  }
#if defined(ESP_PLATFORM)
  // The file gets every parameter of every track, copied first in many small blocks, and a plain
  // allocation that fails ends the program: about 64 bytes each, and room for the rest.
  std::size_t needed = kProjectSaveHeapBytes;
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
    if (const synth::ISynthEngine* engine = Engine(track)) needed += engine->ListParameters({0, 0}).totalCount * 64;
  if (heap_caps_get_free_size(MALLOC_CAP_8BIT) < needed)
  {
    message = "NO MEMORY";
    return groove::SaveResult::Failed;
  }
#endif
  synth::ProjectData project;
  if (!CaptureProject(settings, project, message)) return groove::SaveResult::Failed;
  const auto result = synth::SaveProjectFile(name, project, overwrite);
  storageReady_ = result != synth::ProjectResult::StorageError;
  if (!storageReady_) os::storage::ReportCardIoError();
  switch (result)
  {
    case synth::ProjectResult::Ok: return groove::SaveResult::Ok;
    case synth::ProjectResult::Exists: return groove::SaveResult::Exists;
    case synth::ProjectResult::Invalid: message = "INVALID PROJECT"; return groove::SaveResult::Failed;
    case synth::ProjectResult::StorageError:
    default: message = "SD CARD ERROR"; return groove::SaveResult::Failed;
  }
}

bool GrooveApp::ReplaceSession(const std::string& name, synth::ProjectData& project, bool createNew, std::string& message, bool workingState)
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
  std::array<std::string, synth::kSynthTrackCount> pending;
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
    // The working state does not fall over a sound this firmware no longer knows: it starts from the
    // engine's first one.
    const bool restored = createNew || engines[track]->RestoreState(project.engines[track]) || workingState;
    if (!restored || !engines[track]->CanReplace(*rack_.Slot(track).engine))
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
      // The working state does not fall over a file that is not there: the card may just not be in.
      if (workingState)
      {
        pending[track] = project.samples[track];
        project.samples[track].clear();
        continue;
      }
      message = "SAMPLE MISSING";
      return false;
    }
  }

  // No file reads or allocations that can fail remain: swap bindings in place. The arpeggiator
  // goes first so no arp note fires while the engines are swapped.
  sequencer_.ArpReleaseAll();
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
  // Mixer levels, mute, solo and the master bus belong to the project (the play settings too, applied
  // by ApplyPlaySettings); volume, key LEDs and screen do not.
  levels_ = project.mixer.level;
  muted_ = project.mixer.muted;
  soloed_ = project.mixer.soloed;
  ApplyMix();
  ApplyMasterFx(project.masterFx);
  pendingSamples_ = std::move(pending);
  return true;
}

bool GrooveApp::LoadProject(const std::string& name, groove::ProjectUiSettings& settings, std::string& message)
{
  synth::ProjectData project;
  auto result = synth::LoadProjectFile(name, project);
  if (result == synth::ProjectResult::NoMemory)
  {
    // Two long projects do not fit in RAM side by side. The current one waits in the autosave; if the
    // new one does not load after all, it comes back from there.
    project = synth::ProjectData{};
    if (SaveSessionState() != synth::SessionStore::SaveResult::Ok)
    {
      message = "NO MEMORY";
      return false;
    }
    sequencer_.Reset();
    result = synth::LoadProjectFile(name, project);
    if (result != synth::ProjectResult::Ok)
    {
      (void)RestoreSessionState();
      message = result == synth::ProjectResult::NoMemory ? "NO MEMORY" : "INVALID PROJECT";
      return false;
    }
  }
  if (result != synth::ProjectResult::Ok)
  {
    message = "INVALID PROJECT";
    return false;
  }
  if (!ReplaceSession(name, project, false, message)) return false;
  settings = controller_.Settings();
  settings.activeTrack = synth::kDefaultActiveTrack;
  ApplyPlaySettings(project, settings);
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
  if (!ReplaceSession("", project, true, message)) return false;
  // A new project starts on the first track with the default play settings:
  // C major, no chords, every arpeggiator off. SELECT is a preference and stays.
  const bool multiSelect = controller_.Settings().keyboard.multiSelect;
  settings = groove::ProjectUiSettings{};
  settings.keyboard.multiSelect = multiSelect;
  for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) sequencer_.SetArpSettings(track, synth::ArpSettings{});
  return true;
}

// ---------------------------------------------------------------------------------------------
// System

// Device settings: os::storage keeps them in System/settings.cfg for every app.
void GrooveApp::SetLedBrightness(std::uint8_t level)
{
  os::storage::SetKeyLedBrightness(level);
}

std::uint8_t GrooveApp::LedBrightness() const
{
  return os::storage::KeyLedBrightness();
}

void GrooveApp::SetBacklight(std::uint8_t level)
{
  os::storage::SetScreenBacklight(level);
}

std::uint8_t GrooveApp::Backlight() const
{
  return os::storage::ScreenBacklight();
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

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Applications/Application.h"
#include "Applications/Synth/Runtime/SessionStore.h"
#include "Applications/Synth/Runtime/ProjectStorage.h"
#include "Applications/Synth/Runtime/Sampler/SamplerManager.h"
#include "Applications/Synth/Runtime/Sequencer/SynthSequencer.h"
#include "Applications/Synth/Runtime/SynthRack.h"
#include "Applications/Synth/UI/Groove/GrooveController.h"
#include "Applications/Synth/UI/Groove/GrooveHost.h"
#include "Applications/Synth/UI/Groove/Render/Animator.h"

namespace apps {

// The Cardenza groovebox: four engine tracks, the step sequencer, sampler, mixer and project
// storage behind the terminal / dither UI in Applications/Synth/UI/Groove.
//
// Storage (docs/app-storage-protocol.md): projects in Apps/Cardenza/Synth/Projects, preset banks in
// Apps/Cardenza/Synth/Presets/<engine>, samples in Shared/Samples. The FN SETUP play settings
// (keys.*, arp.*) are app settings in Apps/Cardenza/Synth/settings.cfg; volume, key LEDs and screen
// are device settings (os::storage). Neither changes when a project is loaded or created. Suspend
// saves the working project to Apps/Cardenza/Synth/Session and the next Setup restores it.
class GrooveApp : public app::Application, public groove::Host {
public:
  GrooveApp();
  ~GrooveApp() override;

  void Setup(const std::vector<std::string>& args) override;
  void Suspend() override;
  void Tick() override;
  void UpdateLeds() override;
  void RenderFrame() override;
  // The render-side motion state (tests read standby's scene from it).
  const groove::Animator& Motion() const { return animator_; }
  void HandleMatrixKeyEvent(std::size_t row, std::size_t column, bool pressed) override;
  std::uint32_t EndedOrphanNotes() const { return endedOrphanNotes_; }
  std::uint32_t loggedShedVoices_ = 0;

  // Tests replace the wall clock to make hold and repeat timing deterministic.
  void UseClock(std::uint32_t (*clock)()) { clock_ = clock; }
  // Tests stand in for the device heap (free bytes, largest free block) to run out of memory on
  // purpose; nullptr is the real heap (unlimited on the host).
  using HeapProbe = void (*)(std::size_t& freeBytes, std::size_t& largestBlock);
  void UseHeapProbe(HeapProbe probe) { heapProbe_ = probe; }

  // Writes the autosave to the card now if something changed (not during a sample recording or a
  // lesson). The web host calls it when the page is hidden or closed, where waiting for a quiet moment
  // would lose the last edits.
  void FlushSession();

  groove::Controller& Ui() { return controller_; }
  const groove::Controller& Ui() const { return controller_; }
  synth::SynthRack& Rack() { return rack_; }

  // Host
  synth::ISynthEngine* Engine(std::size_t track) override;
  const synth::ISynthEngine* Engine(std::size_t track) const override;
  synth::SynthEngineKind EngineKind(std::size_t track) const override;
  bool SwitchEngine(std::size_t track, synth::SynthEngineKind kind, std::string& error) override;
  void NoteOn(std::size_t track, std::uint8_t note, std::uint8_t velocity) override;
  void NoteOff(std::size_t track, std::uint8_t note) override;
  void Panic() override;
  synth::SynthSequencer& Sequencer() override { return sequencer_; }
  const synth::SynthSequencer& Sequencer() const override { return sequencer_; }
  synth::SamplerManager& Sampler() override { return sampler_; }
  const synth::SamplerManager& Sampler() const override { return sampler_; }
  void PreviewSample(std::size_t track) override;
  std::vector<std::string> ListSamples() const override;
  void SetTrackLevel(std::size_t track, std::uint8_t level) override;
  std::uint8_t TrackLevel(std::size_t track) const override;
  void SetTrackMuted(std::size_t track, bool muted) override;
  bool TrackMuted(std::size_t track) const override;
  void SetTrackSoloed(std::size_t track, bool soloed) override;
  bool TrackSoloed(std::size_t track) const override;
  bool TrackAudible(std::size_t track) const override;
  void SetMasterVolume(std::uint8_t percent) override;
  std::uint8_t MasterVolume() const override;
  float TrackActivity(std::size_t track) const override;
  float OutputPeak() const override;
  void SetMasterFx(std::size_t param, int value) override;
  int MasterFx(std::size_t param) const override;
  float FxReturnLevel(std::size_t bus) const override;
  bool FxBusAvailable(std::size_t bus) const override;
  std::vector<std::string> ListProjects() const override;
  groove::SaveResult SaveProject(const std::string& name, bool overwrite, const groove::ProjectUiSettings& settings, std::string& message) override;
  bool LoadProject(const std::string& name, groove::ProjectUiSettings& settings, std::string& message) override;
  bool NewProject(groove::ProjectUiSettings& settings, std::string& message) override;
  void SetLedBrightness(std::uint8_t level) override;
  std::uint8_t LedBrightness() const override;
  void SetBacklight(std::uint8_t level) override;
  void SetStandbyMinutes(std::uint8_t minutes) override;
  std::uint8_t StandbyMinutes() const override;
  void SetStandbyLights(std::uint8_t style) override;
  std::uint8_t StandbyLights() const override;
  std::uint8_t Backlight() const override;
  void SetLogging(bool on) override;
  bool Logging() const override;
  groove::SystemInfo QuerySystem() const override;

  static constexpr std::array<synth::SynthEngineKind, synth::kSynthTrackCount> kDefaultKinds{{
      synth::SynthEngineKind::Drums, synth::SynthEngineKind::Juno, synth::SynthEngineKind::Dx7, synth::SynthEngineKind::Sampler}};

private:
  void ReadComputerMidi();
  // The guide (GrooveAppGuide.cpp): a lesson page on the computer sets the working state aside in a
  // practice project, drives the device and follows what is pressed.
  void ServiceGuide(std::uint32_t now);
  void HandleGuideMessage(const char* text, std::size_t length, std::uint32_t now);
  bool EnterGuide(std::string& reason);
  void ExitGuide();
  void GuideSetup(const char* fields, std::size_t length);
  bool FreshGuideProject();
  void FillGuideDemo();
  void SendGuide(const std::string& text);
  void SendGuideState(std::uint32_t now, bool always);
  void SendGuideLeds(std::uint32_t now);
  void ReleaseComputerNotes(std::size_t onlyTrack = synth::kSynthTrackCount);
  static void SendComputerMidi(std::uint8_t status, std::size_t track, std::uint8_t data1, std::uint8_t data2);
  std::uint32_t NowMs() const;
  void ApplyMix();
  void ApplyMasterFx(const std::array<std::uint8_t, synth::fx::kMasterParamCount>& values);
  void SyncSamplerTrim();
  bool ReplaceSession(const std::string& name, synth::ProjectData& project, bool createNew, std::string& message, bool workingState = false);
  // Memory for engines. A plain allocation that fails ends the program, and a whole engine is many
  // of them (a DX7 is about 10 KB), so an engine is only built with room for it: the heap now, the
  // size of its kind (the most one gave back here when it was destroyed, an estimate before that),
  // and its largest block.
  struct HeapRoom {
    std::size_t freeBytes = 0;
    std::size_t largestBlock = 0;
  };
  HeapRoom Heap() const;
  std::size_t EngineBytes(synth::SynthEngineKind kind) const;
  std::size_t EngineBlockBytes(synth::SynthEngineKind kind) const;
  // A new engine with its user presets, or nullptr when there is no room for it plus `alsoNeeded`
  // (`force` builds it anyway: restoring the working state at boot, as before).
  std::unique_ptr<synth::ISynthEngine> BuildEngine(synth::SynthEngineKind kind, std::size_t alsoNeeded, bool force = false);
  // Destroys an engine of `kind` and learns its size from what the heap got back.
  void DropEngine(std::unique_ptr<synth::ISynthEngine>& engine, synth::SynthEngineKind kind);
  // The current session as project data. False (with a UI message) while a sampler take is unsaved,
  // except for the session itself: the take has its own file and comes back with it.
  bool CaptureProject(const groove::ProjectUiSettings& settings, synth::ProjectData& project, std::string& message, bool forSession = false) const;
  // The app settings file: SELECT (a preference), and at boot the keyboard older firmware kept there.
  void RestorePlaySettings();
  void StorePlaySettings();
  // The project's play settings (keyboard, chords, arpeggiators) into `settings` and the sequencer;
  // a project without them (format before 6) keeps the current ones.
  void ApplyPlaySettings(const synth::ProjectData& project, groove::ProjectUiSettings& settings);
  // The working state stays on the card, so the power can go at any moment (SessionStore): every key
  // may have changed something, so after a key what changed is written once it is quiet, or while it
  // plays a little later, on the device's background task. Without a card it is not kept (said once).
  void KeepSessionCurrent(std::uint32_t now);
  // The autosave written now, waiting for the card: false without one, or when it fails.
  bool SaveSessionState();
  // Two projects do not always fit in RAM side by side. The current patterns go to the autosave
  // (the card) and are given back, and the effects give their memory back until
  // synth::audio::ResumeEffects; a caller that still fails brings the patterns back with
  // RestoreSessionPatterns (the engines are only changed by a load that works). False (NO MEMORY)
  // when the autosave cannot be written (no card).
  bool MakeRoom(std::string& message);
  bool RestoreSessionPatterns();
  bool RestoreSessionState();
  // Makes `project` the working state with its name and unsaved mark, as the start and the end of a
  // lesson bring one back.
  bool ApplySession(const synth::SessionInfo& info, synth::ProjectData& project, std::string& message);
  bool Quiet(std::uint32_t now) const;
  void CheckCard(std::uint32_t now);
  void ApplyPendingSamples();
  static void SendSequencerMidi(void* context, std::size_t trackIndex, const midi::MidiPacket& packet);

  bool initialized_ = false;
  bool guide_ = false;
  bool guideOverUsb_ = false;      // entered over a cable: pulling it ends the lesson
  bool guideInjecting_ = false;    // a key the page pressed (not reported back)
  bool guideSongMode_ = false;     // SONG / LOOP before the lesson (a performance control)
  std::uint64_t guideInjectedDown_ = 0;
  std::uint32_t guideHeardAt_ = 0;
  std::uint32_t guideStateAt_ = 0;
  std::string guideState_;
  std::uint32_t guideLedsAt_ = 0;
  std::array<char, 2 + groove::kKeyCount> guideLeds_{};  // the last GD sent
  groove::Scene guideScene_ = groove::Scene::Instrument;  // where the user was, to go back there
  std::uint8_t guidePage_ = 0;
  // The working state of before a lesson, in RAM (PSRAM: the pattern pages are shared with what was
  // playing until either side changes one), given back when it ends.
  std::unique_ptr<synth::ProjectData> guideReturn_;
  synth::SessionInfo guideReturnInfo_;
  bool guideSessionChanged_ = false;  // changes the card did not have when the lesson began
  bool sessionChanged_ = false;
  bool autosaveFailureShown_ = false;
  bool noCardShown_ = false;
  // An older firmware's autosave in internal flash: erased at a quiet moment once the card has the state.
  bool flashSessionLeft_ = false;
  // Started without the card: its autosave comes back when it does (CheckCard), unless there is new work.
  bool sessionWaitsForCard_ = false;
  bool cardWasMounted_ = false;
  int keysHeld_ = 0;
  std::array<std::string, synth::kSynthTrackCount> pendingSamples_;
  std::uint32_t lastKeyAt_ = 0;
  void UpdateScreenDim(std::uint32_t now);
  void UpdateStandbyLeds();
  // The working key lights of the last frame before standby (they fade out as it starts), and whether
  // standby set the master level (put back on waking).
  std::array<std::uint8_t, groove::kKeyCount> workingLeds_{};
  bool standbyMaster_ = false;
  // Standby dims the screen (os::storage::SetScreenDim): the percent shown now and when it last moved.
  float screenDim_ = 100.0f;
  std::uint8_t screenDimShown_ = 100;
  std::uint32_t screenDimAt_ = 0;
  void EndOrphanNotes(std::uint32_t now);
  std::uint32_t lastOrphanCheckAt_ = 0;
  std::array<synth::audio::NoteSet, synth::kSynthTrackCount> orphanNotes_{};
  // Notes a computer plays over USB MIDI and has not let go yet.
  std::array<synth::audio::NoteSet, synth::kSynthTrackCount> computerNotes_{};
  bool computerConnected_ = false;
  bool orphanCandidates_ = false;
  std::uint32_t endedOrphanNotes_ = 0;
  // Changes not on the card yet since when, the last try, and the generation of the last autosave.
  std::uint32_t sessionChangedAt_ = 0;
  std::uint32_t lastSessionTryAt_ = 0;
  std::uint32_t sessionGeneration_ = 0;
  // Takes the working state and has the background task write it to the card: false when it could
  // not start (no card, a write still going, no room for the snapshot).
  bool SaveSessionToCard();
  void CheckCardSession(std::uint32_t now);
  std::uint32_t lastCardCheckAt_ = 0;
  std::uint32_t lastCardProbeAt_ = 0;
  std::uint32_t (*clock_)() = nullptr;
  HeapProbe heapProbe_ = nullptr;
  // By SynthEngineKind: the most an engine of the kind gave back when destroyed (0 = none yet).
  std::array<std::size_t, 5> engineBytes_{};
  synth::SynthRack rack_;
  synth::SamplerManager sampler_;
  synth::SynthSequencer sequencer_;
  groove::Controller controller_;
  groove::Animator animator_;
  std::array<std::uint8_t, synth::kSynthTrackCount> levels_{{100, 100, 100, 100}};
  std::array<bool, synth::kSynthTrackCount> muted_{};
  std::array<bool, synth::kSynthTrackCount> soloed_{};
  // SELECT as last written to the app settings, to notice changes cheaply every tick.
  bool storedMultiSelect_ = false;
  // Tempo last sent to the synced delay.
  std::uint16_t fxTempo_ = 0;
  // Probed at boot and after saves; the SYSTEM page reads it every frame.
  bool storageReady_ = false;
  std::uint32_t renderFrames_ = 0;
  std::uint64_t renderTotalUs_ = 0;
  std::uint32_t renderMaxUs_ = 0;
};

void RegisterGrooveApplication();

}  // namespace apps

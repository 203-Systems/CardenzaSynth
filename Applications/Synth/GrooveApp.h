#pragma once

#include <array>
#include <cstdint>
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
  void HandleMatrixKeyEvent(std::size_t row, std::size_t column, bool pressed) override;
  std::uint32_t EndedOrphanNotes() const { return endedOrphanNotes_; }
  std::uint32_t loggedShedVoices_ = 0;

  // Tests replace the wall clock to make hold and repeat timing deterministic.
  void UseClock(std::uint32_t (*clock)()) { clock_ = clock; }

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
  std::uint8_t Backlight() const override;
  groove::SystemInfo QuerySystem() const override;

  static constexpr std::array<synth::SynthEngineKind, synth::kSynthTrackCount> kDefaultKinds{{
      synth::SynthEngineKind::Drums, synth::SynthEngineKind::Juno, synth::SynthEngineKind::Dx7, synth::SynthEngineKind::Sampler}};

private:
  void ReadComputerMidi();
  void ReleaseComputerNotes(std::size_t onlyTrack = synth::kSynthTrackCount);
  static void SendComputerMidi(std::uint8_t status, std::size_t track, std::uint8_t data1, std::uint8_t data2);
  std::uint32_t NowMs() const;
  void ApplyMix();
  void ApplyMasterFx(const std::array<std::uint8_t, synth::fx::kMasterParamCount>& values);
  void SyncSamplerTrim();
  bool ReplaceSession(const std::string& name, synth::ProjectData& project, bool createNew, std::string& message, bool workingState = false);
  // The current session as project data. False (with a UI message) while a sampler take is unsaved,
  // except for the session itself: the take has its own file and comes back with it.
  bool CaptureProject(const groove::ProjectUiSettings& settings, synth::ProjectData& project, std::string& message, bool forSession = false) const;
  // The app settings file: SELECT (a preference), and at boot the keyboard older firmware kept there.
  void RestorePlaySettings();
  void StorePlaySettings();
  // The project's play settings (keyboard, chords, arpeggiators) into `settings` and the sequencer;
  // a project without them (format before 6) keeps the current ones.
  void ApplyPlaySettings(const synth::ProjectData& project, groove::ProjectUiSettings& settings);
  // The working state stays in internal flash, so the power can go at any moment (SessionStore): every
  // key may have changed something, so after a key what changed is written once it is quiet.
  void KeepSessionCurrent(std::uint32_t now);
  synth::SessionStore::SaveResult SaveSessionState();
  bool RestoreSessionState();
  bool Quiet(std::uint32_t now) const;
  void CheckCard(std::uint32_t now);
  void ApplyPendingSamples();
  static void SendSequencerMidi(void* context, std::size_t trackIndex, const midi::MidiPacket& packet);

  bool initialized_ = false;
  synth::SessionStore session_;
  bool sessionChanged_ = false;
  bool autosavePartialShown_ = false;
  bool autosaveFailureShown_ = false;
  bool cardWasMounted_ = false;
  int keysHeld_ = 0;
  std::array<std::string, synth::kSynthTrackCount> pendingSamples_;
  std::uint32_t lastKeyAt_ = 0;
  void EndOrphanNotes(std::uint32_t now);
  std::uint32_t lastOrphanCheckAt_ = 0;
  std::array<synth::audio::NoteSet, synth::kSynthTrackCount> orphanNotes_{};
  // Notes a computer plays over USB MIDI and has not let go yet.
  std::array<synth::audio::NoteSet, synth::kSynthTrackCount> computerNotes_{};
  bool computerConnected_ = false;
  bool orphanCandidates_ = false;
  std::uint32_t endedOrphanNotes_ = 0;
  std::uint32_t sessionChangedAt_ = 0;
  std::uint32_t lastSessionTryAt_ = 0;
  std::uint32_t lastCardCheckAt_ = 0;
  std::uint32_t lastCardProbeAt_ = 0;
  std::uint32_t (*clock_)() = nullptr;
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

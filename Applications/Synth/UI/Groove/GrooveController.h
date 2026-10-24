#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Applications/Synth/UI/Groove/GrooveHost.h"
#include "Applications/Synth/UI/Groove/GrooveTypes.h"
#include "Applications/Synth/UI/Groove/InstrumentPages.h"

namespace groove {

inline constexpr std::uint32_t kHoldMs = 250;
inline constexpr std::uint32_t kRepeatDelayMs = 400;
inline constexpr std::uint32_t kRepeatIntervalMs = 70;
inline constexpr std::uint32_t kRepeatAccelerateAfter = 12;
inline constexpr std::uint32_t kToastMs = 2000;
inline constexpr std::uint32_t kTransitionMs = 110;
inline constexpr std::uint8_t kPadVelocity = 110;

enum class ToastKind : std::uint8_t { Info, Ok, Error };

struct Toast {
  std::string text;
  ToastKind kind = ToastKind::Info;
  std::uint32_t until = 0;
};

enum class ModalKind : std::uint8_t {
  None,
  TextEntry,
  Confirm,
  PresetBrowser,
  EnginePicker,
  SampleBrowser,
  SampleOptions,
  SampleRecord,
};

enum class TextPurpose : std::uint8_t {
  ProjectSaveAs,
  PresetSaveAs,
  SampleName,
  SampleRename,
};

enum class ConfirmPurpose : std::uint8_t {
  NewProject,
  LoadProject,
  OverwriteProject,
  ClearPattern,
  DeleteSample,
  DiscardTake,
  ReplaceTake,
  OverwriteSample,
};

struct ListItem {
  std::string id;
  std::string label;
  bool marked = false;  // current / active entry
};

struct Modal {
  ModalKind kind = ModalKind::None;
  // Text entry
  TextPurpose textPurpose = TextPurpose::ProjectSaveAs;
  std::string title;
  std::string text;
  std::size_t maxLength = 24;
  // Confirm
  ConfirmPurpose confirmPurpose = ConfirmPurpose::NewProject;
  std::string detail;
  std::string payload;
  bool danger = false;
  // Lists (presets, engines, samples, file options)
  std::vector<ListItem> items;
  std::size_t cursor = 0;
  std::string search;
  // Sample take editing: 0 = start marker, 1 = end marker.
  std::uint8_t trimMarker = 0;
};

enum class Arrow : std::uint8_t { Up, Down, Left, Right };

// Key-light vocabulary. Brightness says how much (value / content), the effect says what kind of key
// it is: steady = holds a value, breathe = runs an action when pressed, shimmer = focus / selection,
// blink = waiting (queued / cued). The Animator turns effects into motion.
enum class LedEffect : std::uint8_t { Steady, Breathe, Shimmer, Blink };

struct LedCell {
  std::uint8_t level = 0;
  LedEffect effect = LedEffect::Steady;
};
using LedFrame = std::array<LedCell, kKeyCount>;

struct LeftKeyState {
  bool down = false;
  bool used = false;        // combined with another key while held
  bool suppressed = false;  // part of a chord that has its own meaning (Fn + CTRL), or pressed under a modal
  std::uint32_t pressedAt = 0;
};

struct RepeatState {
  bool down = false;
  int direction = 0;
  bool coarse = false;
  std::uint32_t nextAt = 0;
  std::uint32_t count = 0;
};

struct UiState {
  Scene scene = Scene::Sequence;
  Scene previousScene = Scene::Instrument;
  std::array<std::uint8_t, kSceneCount> page{};
  std::size_t track = 0;
  KeyboardSettings keyboard{};

  std::array<LeftKeyState, 4> left{};  // ESC, TAB, FN, CTRL
  RepeatState repeat{};

  std::vector<Modal> modals;
  Toast toast{};
  std::uint32_t sceneChangedAt = 0;
  std::uint32_t now = 0;

  // Focused GRID slot per INSTR page and per fixed page. Each track keeps its own INSTR page and focus.
  std::array<std::uint8_t, 16> instrumentFocus{};
  std::array<std::uint8_t, kTrackCount> trackInstrumentPage{};
  std::array<std::array<std::uint8_t, 16>, kTrackCount> trackInstrumentFocus{};
  std::uint8_t projectFocus = 4;  // BPM
  std::uint8_t systemFocus = 0;
  std::uint8_t seqTrackFocus = 0;
  std::uint8_t mixFocus = 0;      // 0..3 track, 4 master
  std::uint8_t sendsFocus = 2;    // TAPE SENDS cell: row = effect, column = track
  std::uint8_t fxFocus = 0;       // TAPE FX: synth::fx::MasterParam
  // Value + 1 each SENDS cell had before ENTER switched it off (0 = none), so ENTER can bring it back.
  std::array<std::uint8_t, kGridCount> sendsMemory{};
  // TAPE song cursor: arrangement row and track, plus the first row shown on the GRID.
  std::size_t songRow = 0;
  std::size_t songTrack = 0;
  std::size_t songScroll = 0;
  synth::SongRow songClipboard{};
  bool songClipboardValid = false;
  std::size_t fileCursor = 0;
  std::vector<std::string> projects;

  std::string projectName;
  bool dirty = false;

  // Copy / paste: steps of the active pattern or a whole pattern.
  bool clipboardIsPattern = false;
  std::size_t clipboardIndex = 0;
  bool clipboardValid = false;

  // Pads: note and track that each held pad started.
  std::array<std::uint8_t, kRows * kPadColumns> padNote{};
  std::array<std::uint8_t, kRows * kPadColumns> padTrack{};
  std::array<bool, kGridCount> gridDown{};
  std::array<std::uint32_t, kTrackCount> lastTrackNoteAt{};

  // Tap tempo
  std::array<std::uint32_t, 4> taps{};
  std::size_t tapCount = 0;

  // Event log for motion and key lighting: when each matrix key was last pressed, the last panic
  // stop, and the last time the whole session was replaced (load / new).
  std::array<std::uint32_t, kKeyCount> keyPressedAt{};
  std::uint32_t panicAt = 0;
  std::uint32_t sessionAt = 0;
};

class Controller {
public:
  explicit Controller(Host& host);

  void Restore(const ProjectUiSettings& settings, const std::string& projectName);
  ProjectUiSettings Settings() const;

  void KeyEvent(std::size_t row, std::size_t column, bool pressed, std::uint32_t nowMs);
  void HomeButton(bool pressed, std::uint32_t nowMs);
  void Tick(std::uint32_t nowMs);
  void ComputeLeds(LedFrame& frame) const;
  // Levels only (no effects), for callers that do not animate.
  void ComputeLeds(std::array<std::uint8_t, kKeyCount>& levels) const;

  const UiState& State() const { return state_; }
  const Host& HostView() const { return host_; }
  const Modal* TopModal() const { return state_.modals.empty() ? nullptr : &state_.modals.back(); }
  // Hold maps: while a left key is held, the GRID chooses what it switches to. ESC = scenes,
  // TAB = tracks (select / mute / solo / seq), CTRL = pages of the current scene, FN = functions.
  bool EscOverlay() const;
  bool TabOverlay() const;
  bool FnOverlay() const;
  bool CtrlOverlay() const;

  std::size_t PageCount(Scene scene) const;
  std::uint8_t CurrentPage() const { return state_.page[static_cast<std::size_t>(state_.scene)]; }
  const InstrumentPage* CurrentInstrumentPage() const;
  InstrumentPageList InstrumentPages() const;
  std::uint8_t InstrumentFocus() const;
  std::string PageTab(Scene scene, std::size_t page) const;
  // Short page description for the CTRL page map.
  std::string PageName(Scene scene, std::size_t page) const;

  bool PadNoteFor(std::size_t row, std::size_t column, int& note) const;
  bool IsPadDown(std::size_t row, std::size_t column) const;
  bool IsNoteHeld(int note) const;

  // Fn shortcut table for the current scene (for the Fn overlay).
  struct FnEntry {
    const char* key;
    const char* label;
    bool available;
  };
  std::vector<FnEntry> FnEntries() const;
  // FN + GRID functions, one per GRID key (label nullptr = none).
  struct FnGridEntry {
    const char* label = nullptr;
    bool available = false;
  };
  std::array<FnGridEntry, kGridCount> FnGridEntries() const;

  // Presentation helpers shared by renderer and LEDs.
  bool SlotHasValue(const Slot& slot) const;
  bool SlotValue(const Slot& slot, synth::SynthParameterInfo& info, std::string& text) const;

private:
  // Input routing
  void PressLeft(std::size_t row);
  void ReleaseLeft(std::size_t row);
  void PressRight(KeyRole role);
  void ReleaseRight(KeyRole role);
  void PressPad(std::size_t row, std::size_t column);
  void ReleasePad(std::size_t row, std::size_t column);
  void PressGrid(std::size_t index);
  void ReleaseGrid(std::size_t index);
  bool HandleFnKey(std::size_t row, std::size_t column);
  bool HandleTextKey(std::size_t row, std::size_t column);
  bool HandleSearchKey(std::size_t row, std::size_t column);

  // Navigation
  void GoToScene(Scene scene);
  void SetPage(std::uint8_t page);
  void StepPage(int delta);
  void Back();
  void SelectTrack(std::size_t track);
  void Adjust(int delta, bool coarse);
  void Enter();
  void FnEnter();
  void TogglePlay();
  void ToggleRecord();
  void ArrowKey(Arrow arrow);

  // Scene handlers (GrooveScenes.cpp)
  void ProjectGrid(std::size_t index);
  void ProjectAdjust(int delta, bool coarse);
  void ProjectEnter();
  void InstrumentGrid(std::size_t index);
  void InstrumentAdjust(int delta, bool coarse);
  void InstrumentEnter();
  void InstrumentReset();
  void SequenceGrid(std::size_t index);
  void SequenceGridRelease(std::size_t index);
  void SequenceAdjust(int delta, bool coarse);
  void SequenceEnter();
  void SongGrid(std::size_t index);
  void SongAdjust(int delta, bool coarse);
  void SongEnter();
  void SongFnEnter();
  void SongArrow(int horizontal, int vertical);
  void MixGrid(std::size_t index);
  void MixAdjust(int delta, bool coarse);
  void MixEnter();
  void SendsGrid(std::size_t index);
  void SendsAdjust(int delta, bool coarse);
  void SendsEnter();
  void SendsReset();
  void FxGrid(std::size_t index);
  void FxAdjust(int delta, bool coarse);
  void FxEnter();
  void FxReset();
  void SetSendValue(std::size_t track, const char* id, int value);
  void RunSlotAction(SlotAction action);
  void AdjustKeysSetting(const char* id, int delta);
  void StepPreset(int delta);
  bool PadForSequence(int note, bool pressed);

  // Fn functions
  void Copy();
  void Paste();
  void Clear();
  void Quantize();
  void Duplicate();
  void TapTempo();
  void NudgeTempo(int delta);
  void StepTrackPattern(int delta);
  void QuickSave();
  void CycleLayout();
  void AdjustOctave(int delta);
  void ToggleMute(std::size_t track);

  // Modals (GrooveModals.cpp)
  void OpenModal(Modal modal);
  void CloseModal();
  Modal* Top();
  void OpenTextEntry(TextPurpose purpose, const std::string& title, const std::string& initial, std::size_t maxLength);
  void OpenConfirm(ConfirmPurpose purpose, const std::string& title, const std::string& detail, bool danger, const std::string& payload = {});
  void OpenPresetBrowser();
  void RefreshPresetBrowser();
  void OpenEnginePicker();
  void OpenSampleBrowser();
  void RefreshSampleBrowser();
  void OpenSampleRecord();
  void ModalAdjust(int delta, bool coarse);
  void ModalEnter();
  void ModalFnEnter();
  void ModalGrid(std::size_t index);
  void ModalCancel();
  void CommitText();
  void Confirmed();
  void SampleRecordEnter();
  void SampleRecordGrid(std::size_t index);

  // Project actions
  void SaveProject(bool saveAs);
  void SaveProjectNamed(const std::string& name, bool overwrite);
  void LoadProjectNamed(const std::string& name);
  void NewProject();
  void RefreshProjects();

  bool SongPage() const;
  void ShowToast(const std::string& text, ToastKind kind = ToastKind::Info);
  void MarkDirty() { state_.dirty = true; }
  void ReleaseAllPads();
  std::size_t CollectHeldNotes(std::array<std::uint8_t, kRows * kPadColumns>& notes) const;
  synth::ISynthEngine* ActiveEngine();
  const synth::ISynthEngine* ActiveEngine() const;
  synth::SynthEngineKind ActiveKind() const;
  void ClampPages();

  Host& host_;
  UiState state_{};
};

}  // namespace groove

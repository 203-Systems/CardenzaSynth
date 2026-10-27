#include "Applications/Synth/UI/SynthUiModel.h"
#include "Library/Synth/Core/SynthStorage.h"
#include "Applications/Synth/UI/Dx7/Dx7UiModel.h"
#include "Applications/Synth/UI/Juno/JunoUiModel.h"
#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

namespace Device {
std::string BackendName() { return "test"; }
}
namespace synth::storage {
void NotifyWriteFailure() {}
std::string SampleDirectory() { return "missing-sample-directory"; }
bool EnsureLayout() { return false; }
}
namespace synth {
std::string ToString(SynthEngineKind) { return "TEST"; }
}

// Storage failure injected through the actual engine interface used by both SAVE paths.
class FailingSaveEngine final : public synth::ISynthEngine {
public:
  synth::SynthDescriptor descriptor{synth::SynthEngineKind::Juno, "test", "Juno"};
  int saveCalls = 0;
  std::string savedId;
  std::string savedName;
  bool Init(const synth::SynthEngineInit&) override { return true; }
  std::uint16_t PortId() const override { return midi::MIDI_PORT_INVALID; }
  void DispatchPendingMessages() override {}
  const synth::SynthDescriptor& Descriptor() const override { return descriptor; }
  synth::SynthParameterPage ListParameters(const synth::SynthPageRequest&) const override { return {}; }
  synth::SynthPresetPage ListPresets(const synth::SynthPageRequest&) const override { return {}; }
  bool GetParameterInfo(const std::string&, synth::SynthParameterInfo&) const override { return false; }
  void ProcessMidi(const midi::MidiPacket&) override {}
  bool SetParameterValue(const std::string&, int) override { return false; }
  bool AdjustParameterValue(const std::string&, int) override { return false; }
  std::string DescribeParameterValue(const std::string&) const override { return {}; }
  bool LoadPreset(const std::string&) override { return false; }
  bool RefreshPresetCatalog() override { return true; }
  bool SavePreset(const std::string& id, const std::string& name) override {
    ++saveCalls; savedId = id; savedName = name; return false;
  }
  std::string CurrentPresetId() const override { return "original.id"; }
  std::string CurrentPresetName() const override { return "Original"; }
  synth::SynthEngineState CaptureState() const override { return {}; }
  bool RestoreState(const synth::SynthEngineState&) override { return false; }
  bool CanReplace(const synth::ISynthEngine&) const override { return false; }
  void ReplaceBinding(synth::ISynthEngine&) override {}
  std::string DebugState() const override { return {}; }
private:
  bool HasBinding() const override { return false; }
  RuntimeBinding TakeBinding() override { std::abort(); }
};

int main()
{
#if defined(_MSC_VER)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _set_error_mode(_OUT_TO_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  using namespace synth;
  SynthUiModel ui;
  ISynthEngine* engine = nullptr;
  auto tap = [&](std::size_t row, std::size_t col) {
    ui.PressMatrixKey(engine, row, col);
    ui.ReleaseMatrixKey(engine, row, col);
  };
  auto checkReachable = [&]() {
    const auto menu = ui.CurrentMenuCells(engine);
    const auto columns = ui.ContextMenuColumnCount();
    assert(columns == 1 || columns == 2);
    for (std::size_t i = columns * 4; i < menu.size(); ++i) assert(menu[i].type == MenuCellType::Empty);
  };
  auto select = [&](ContextActionId action, const std::string& id = "") {
    checkReachable();
    const auto menu = ui.CurrentMenuCells(engine);
    const auto columns = ui.ContextMenuColumnCount();
    for (std::size_t i = 0; i < columns * 4; ++i) {
      if ((action != ContextActionId::None && menu[i].actionId == action) || (!id.empty() && menu[i].itemId == id)) {
        const auto row = columns == 1 ? i : i / 2;
        const auto col = columns == 1 ? kHomeRightBarColumn : kContextMenuFirstColumn + i % 2;
        tap(row, col);
        checkReachable();
        return;
      }
    }
    std::cerr << "Missing menu action " << static_cast<int>(action) << " / " << id << " in";
    for (const auto& cell : menu) std::cerr << ' ' << cell.label;
    std::cerr << '\n';
    assert(false && "Menu action is not reachable");
  };
  auto command = [&](UiCommandType expected, const std::string& text = "") {
    UiCommand next;
    assert(ui.ConsumePendingCommand(next) && next.type == expected && next.text == text);
    assert(!ui.ConsumePendingCommand(next));
  };
  tap(2, kHomeRightBarColumn); command(UiCommandType::SequencerStop);
  tap(3, kHomeRightBarColumn); command(UiCommandType::SequencerTogglePlayback);
  const auto originalKeyboard = ui.KeyboardSettings();
  assert(!ui.RestoreKeyboardSettings({0, 12, 0, 0}));
  assert(ui.KeyboardSettings() == originalKeyboard);
  assert(ui.RestoreKeyboardSettings({2, 11, 7, 2}));
  assert((ui.KeyboardSettings() == std::array<int, 4>{2, 11, 7, 2}));

  ui.PressMatrixKey(nullptr, 3, kHomeLeftBarColumn);
  std::this_thread::sleep_for(std::chrono::milliseconds(210));
  ui.Tick(nullptr);
  select(ContextActionId::None, "sys.project");
  ui.ReleaseMatrixKey(nullptr, 3, kHomeLeftBarColumn);
  select(ContextActionId::ProjectSave);
  assert(ui.ContextMenuColumnCount() == 1);
  assert(ui.CurrentMenuCells(nullptr)[2].actionId == ContextActionId::Back);
  assert(ui.CurrentMenuCells(nullptr)[3].actionId == ContextActionId::ProjectNameSave);
  tap(1, 1); // q: the project name field consumes typing rather than playing.
  select(ContextActionId::ProjectNameSave); command(UiCommandType::ProjectSave, "Projectq");
  ui.ProjectResult("Projectq", "SAVE ERR", false);
  assert(ui.CurrentScreenSnapshot(nullptr).primaryText == "SAVE ERR");
  ui.ConfirmProjectOverwrite("Projectq");
  assert(ui.ContextMenuColumnCount() == 1);
  UiCommand noCommand;
  assert(!ui.ConsumePendingCommand(noCommand));
  select(ContextActionId::Back); // Overwrite cancel preserves the proposed name.
  assert(ui.CurrentScreenSnapshot(nullptr).secondaryText == "Projectq_");
  ui.ConfirmProjectOverwrite("Projectq");
  select(ContextActionId::ProjectConfirm); command(UiCommandType::ProjectOverwrite, "Projectq");
  ui.ProjectResult("Projectq", "SAVED", true);
  select(ContextActionId::ProjectSave); command(UiCommandType::ProjectSave, "Projectq");
  ui.ProjectResult("Projectq", "SAVED", true);
  select(ContextActionId::ProjectSaveAs);
  select(ContextActionId::ProjectNameBackspace);
  assert(ui.CurrentScreenSnapshot(nullptr).secondaryText == "Project_");
  select(ContextActionId::Back);
  select(ContextActionId::ProjectLoad); command(UiCommandType::ProjectList);
  assert(ui.CurrentMenuCells(nullptr)[3].disabled);
  select(ContextActionId::ProjectLoad); assert(!ui.ConsumePendingCommand(noCommand));
  ui.SetProjectList({"Alpha", "Beta"});
  select(ContextActionId::PresetNext);
  select(ContextActionId::PresetPrev);
  select(ContextActionId::PresetNext);
  select(ContextActionId::ProjectLoad);
  assert(!ui.ConsumePendingCommand(noCommand)); // Destructive replacement requires on-device confirmation.
  select(ContextActionId::Back); assert(!ui.ConsumePendingCommand(noCommand));
  assert(ui.CurrentMenuCells(nullptr)[3].actionId == ContextActionId::ProjectLoad);
  select(ContextActionId::ProjectLoad);
  select(ContextActionId::ProjectConfirm); command(UiCommandType::ProjectLoad, "Beta");
  ui.ProjectResult("Beta", "LOAD FAILED", false);
  assert(ui.CurrentScreenSnapshot(nullptr).primaryText == "LOAD FAILED");
  select(ContextActionId::ProjectLoad);
  select(ContextActionId::ProjectConfirm); command(UiCommandType::ProjectLoad, "Beta");
  ui.ProjectResult("Beta", "LOADED", true);
  select(ContextActionId::ProjectNew);
  select(ContextActionId::Back); assert(!ui.ConsumePendingCommand(noCommand));
  select(ContextActionId::ProjectNew);
  select(ContextActionId::ProjectConfirm); command(UiCommandType::ProjectNew);
  ui.ProjectResult("", "NEW", true);
  select(ContextActionId::Back);
  select(ContextActionId::None, "sys.seq");
  assert(!ui.HomeShowsSequencerGrid() && !ui.CurrentScreenSnapshot(nullptr).showSequencerGrid);
  assert(ui.CurrentMenuCells(nullptr)[6].label == "GRID");
  select(ContextActionId::SequencerToggleView); command(UiCommandType::SequencerToggleView);
  ui.ToggleHomeView();
  assert(ui.HomeShowsSequencerGrid() && ui.CurrentScreenSnapshot(nullptr).showSequencerGrid);
  assert(ui.CurrentMenuCells(nullptr)[6].label == "KEYS");
  select(ContextActionId::SequencerToggleView); command(UiCommandType::SequencerToggleView);
  ui.ToggleHomeView();
  assert(!ui.HomeShowsSequencerGrid() && !ui.CurrentScreenSnapshot(nullptr).showSequencerGrid);
  select(ContextActionId::SequencerTogglePlayback); command(UiCommandType::SequencerTogglePlayback);
  select(ContextActionId::SequencerStop); command(UiCommandType::SequencerStop);
  select(ContextActionId::SequencerResume); command(UiCommandType::SequencerResume);
  select(ContextActionId::None, "sys.seq.tempo");
  assert(ui.CurrentScreenSnapshot(nullptr).focusItemId == "sys.seq.tempo");
  assert(ui.CurrentScreenSnapshot(nullptr).primaryText == "Tempo");
  assert(ui.CurrentScreenSnapshot(nullptr).secondaryText == "120 BPM");
  select(ContextActionId::SequencerTempoIncLarge); command(UiCommandType::SequencerTempoIncLarge);
  select(ContextActionId::SequencerTempoDecLarge); command(UiCommandType::SequencerTempoDecLarge);
  select(ContextActionId::SequencerTempoInc); command(UiCommandType::SequencerTempoInc);
  select(ContextActionId::SequencerTempoDec); command(UiCommandType::SequencerTempoDec);
  select(ContextActionId::Back);
  select(ContextActionId::None, "sys.seq.track");
  assert(ui.ContextMenuColumnCount() == 2);
  assert(ui.CurrentMenuCells(nullptr)[7].actionId == ContextActionId::Back);
  select(ContextActionId::SequencerTrackPrev); command(UiCommandType::SequencerTrackPrev);
  select(ContextActionId::SequencerTrackNext); command(UiCommandType::SequencerTrackNext);
  assert(ui.CurrentScreenSnapshot(nullptr).focusItemId == "sys.seq.track");
  assert(ui.CurrentScreenSnapshot(nullptr).primaryText == "Track 1");
  select(ContextActionId::SequencerMute); command(UiCommandType::SequencerMute);
  select(ContextActionId::SequencerEnable); command(UiCommandType::SequencerEnable);
  select(ContextActionId::Back);
  select(ContextActionId::None, "sys.seq.pattern");
  assert(ui.CurrentScreenSnapshot(nullptr).primaryText == "Pattern");
  select(ContextActionId::SequencerPatternPrev); command(UiCommandType::SequencerPatternPrev);
  select(ContextActionId::SequencerPatternNext); command(UiCommandType::SequencerPatternNext);
  assert(ui.CurrentMenuCells(nullptr)[2].disabled);
  select(ContextActionId::SequencerPatternCountDec);
  assert(!ui.ConsumePendingCommand(noCommand)); // The only pattern cannot be removed.
  SequencerUiState sequence;
  sequence.patternCount = 2;
  ui.SetSequencerUiState(sequence);
  select(ContextActionId::SequencerPatternCountDec); command(UiCommandType::SequencerPatternCountDec);
  UiCommand deletion;
  sequence.patternCount = 3;
  sequence.patternHasEvents[2] = true;
  ui.SetActiveTrack(2);
  ui.SetSequencerUiState(sequence);
  select(ContextActionId::SequencerPatternCountDec);
  assert(ui.CurrentScreenSnapshot(nullptr).focusItemId == "sys.seq.delete");
  assert(ui.CurrentScreenSnapshot(nullptr).headerSecondary == "P03");
  assert(!ui.ConsumePendingCommand(noCommand));
  select(ContextActionId::Back);
  assert(ui.CurrentScreenSnapshot(nullptr).focusItemId == "sys.seq.pattern" && !ui.ConsumePendingCommand(noCommand));
  select(ContextActionId::SequencerPatternCountDec);
  ui.SetActiveTrack(2); // Routine sync of the same track must retain confirmation.
  assert(ui.CurrentScreenSnapshot(nullptr).focusItemId == "sys.seq.delete");
  ui.SetActiveTrack(1); // Changing the actual target track cancels rather than retargets.
  assert(ui.CurrentScreenSnapshot(nullptr).focusItemId == "sys.seq.pattern" && !ui.ConsumePendingCommand(noCommand));
  ui.SetActiveTrack(2);
  select(ContextActionId::SequencerPatternCountDec);
  select(ContextActionId::SequencerPatternDeleteConfirm);
  assert(ui.ConsumePendingCommand(deletion) && deletion.type == UiCommandType::SequencerPatternDelete);
  assert(deletion.trackIndex == 2 && deletion.patternIndex == 2 && !ui.ConsumePendingCommand(noCommand));
  assert(ui.CurrentScreenSnapshot(nullptr).focusItemId == "sys.seq.pattern");
  ui.SetActiveTrack(0);
  select(ContextActionId::SequencerPatternCountInc); command(UiCommandType::SequencerPatternCountInc);
  select(ContextActionId::Back);
  assert(ui.CurrentScreenSnapshot(nullptr).primaryText == "Stopped");
  assert(ui.CurrentScreenSnapshot(nullptr).secondaryText == "120 BPM");

  FailingSaveEngine failingEngine;
  engine = &failingEngine;
  ui.ExitToHome();
  ui.InitializeTrack(0, SynthEngineKind::Juno);
  ui.SyncTrackState(0, engine);
  ui.PressMatrixKey(engine, 0, kHomeLeftBarColumn);
  std::this_thread::sleep_for(std::chrono::milliseconds(210));
  ui.Tick(engine);
  select(ContextActionId::None, "snd.save");
  ui.ReleaseMatrixKey(engine, 0, kHomeLeftBarColumn);
  assert(failingEngine.saveCalls == 1 && failingEngine.savedId == "original.id");
  assert(failingEngine.savedName == "Original");
  assert(ui.CurrentScreenSnapshot(engine).secondaryText == "SAVE ERR");
  assert(ui.PatchName() == "Original");
  select(ContextActionId::None, "snd.saveas");
  select(ContextActionId::PresetNameBackspace);
  select(ContextActionId::PresetNameSave);
  assert(failingEngine.saveCalls == 2 && failingEngine.savedId.empty());
  assert(failingEngine.savedName == "Origina");
  assert(ui.CurrentScreenSnapshot(engine).primaryText == "SAVE ERR");
  assert(ui.CurrentScreenSnapshot(engine).secondaryText == "Origina_");
  assert(ui.PatchName() == "Original");
  select(ContextActionId::Back);
  assert(failingEngine.saveCalls == 2 && ui.CurrentScreenSnapshot(engine).primaryText == "Juno");

  ui.ExitToHome();
  failingEngine.descriptor.kind = SynthEngineKind::Sampler;
  ui.InitializeTrack(0, SynthEngineKind::Sampler);
  ui.SyncTrackState(0, engine);
  SamplerTrackUiState sampler;
  ui.SyncSamplerTrackState(0, sampler);
  ui.PressMatrixKey(engine, 1, kHomeLeftBarColumn);
  std::this_thread::sleep_for(std::chrono::milliseconds(210));
  ui.Tick(engine);
  select(ContextActionId::SamplerRecord);
  ui.ReleaseMatrixKey(engine, 1, kHomeLeftBarColumn);
  assert(ui.CurrentMenuCells(engine)[2].actionId == ContextActionId::Back);
  assert(ui.CurrentMenuCells(engine)[3].actionId == ContextActionId::SamplerRecordStart);
  select(ContextActionId::SamplerRecordStart); command(UiCommandType::SamplerStartRecord);
  sampler.recording = true;
  sampler.phase = SamplerTrackPhase::Recording;
  ui.SyncSamplerTrackState(0, sampler);
  sampler.recording = false;
  sampler.hasDraft = true;
  sampler.phase = SamplerTrackPhase::DraftTrim;
  ui.SyncSamplerTrackState(0, sampler); // Automatic stop leaves the draft FILE group open.
  checkReachable();
  assert(ui.ContextMenuColumnCount() == 1);
  assert(ui.CurrentMenuCells(engine)[2].actionId == ContextActionId::Back);
  assert(ui.CurrentMenuCells(engine)[3].actionId == ContextActionId::SamplerName);
  sampler.overwritePending = true;
  ui.SyncSamplerTrackState(0, sampler);
  checkReachable();
  assert(ui.CurrentMenuCells(engine)[2].actionId == ContextActionId::Back);
  assert(ui.CurrentMenuCells(engine)[3].actionId == ContextActionId::SamplerConfirmOverwrite);
  select(ContextActionId::Back); command(UiCommandType::SamplerCancelOverlay);
  sampler.overwritePending = false;
  sampler.deleteConfirm = true;
  ui.SyncSamplerTrackState(0, sampler);
  checkReachable();
  assert(ui.CurrentMenuCells(engine)[2].actionId == ContextActionId::Back);
  assert(ui.CurrentMenuCells(engine)[3].actionId == ContextActionId::SamplerDelete);
  select(ContextActionId::Back); command(UiCommandType::SamplerCancelOverlay);

  for (const auto& names : std::array<std::array<const char*, 2>, 4>{{
           {{"Rate 1", "R1"}}, {{"Rate 2", "R2"}}, {{"Level 3", "L3"}}, {{"Rate 4", "R4"}}}}) {
    SynthParameterInfo parameter;
    parameter.label = names[0];
    assert(ui_model::Dx7ParameterMenuLabel(parameter) == names[1]);
    assert(ui_model::Dx7ParameterHelpDescription(parameter).find(names[1]) == 0);
  }
  SynthParameterInfo junoAttack;
  junoAttack.id = "env.attack";
  assert(ui_model::JunoParameterMenuLabel(junoAttack) == "ATK");
  // Narrow sidebars must contain labels even when the ellipsis itself is too wide.
  ::ui::Canvas textCanvas({});
  for (int scale : {1, 2, 3, 4})
    for (int width : {0, 5, 12, 24, 30, 34, 88})
      for (const char* label : {"TRANSPOSE", "SAVEAS", "TRACK\nCHANNEL", "..."})
        assert(textCanvas.MeasureTextWidth(ui_render::TruncateText(textCanvas, label, width, scale), scale) <= width);

  // Capital-only small cells keep filenames on the same baseline without descenders.
  std::array<Device::Display::Pixel, 70 * 12> lowerPixels{};
  std::array<Device::Display::Pixel, 70 * 12> upperPixels{};
  ::ui::Canvas lowerCanvas({lowerPixels.data(), lowerPixels.size(), 70, 12});
  ::ui::Canvas upperCanvas({upperPixels.data(), upperPixels.size(), 70, 12});
  lowerCanvas.DrawText(0, 0, "qafile.wav", 0xFFFF);
  upperCanvas.DrawText(0, 0, "QAFILE.WAV", 0xFFFF);
  assert(lowerPixels == upperPixels);

  // Home exposes the performance map: Piano leaves the sharps' gaps empty, scale layouts map every key.
  ui.ExitToHome();
  ui.SetActiveTrack(0);
  assert(ui.RestoreKeyboardSettings({0, 0, 0, 0}));
  const auto pianoHome = ui.CurrentScreenSnapshot(nullptr);
  assert(pianoHome.keyboardLayoutMode == KeyboardLayoutMode::Piano && pianoHome.keyboardRootNote == 0);
  assert(pianoHome.performanceKeyNotes[3 * kHomePerformanceColumnCount] == kHomePerformanceBaseMidiNote);
  assert(pianoHome.performanceKeyNotes[2 * kHomePerformanceColumnCount] < 0);

  // Mini piano with every key held: twelve separate lit keys, equal whites, black keys centred on their gap.
  {
    constexpr int kWidth = 160;
    constexpr int kHeight = 60;
    std::vector<Device::Display::Pixel> pixels(kWidth * kHeight, 0);
    ::ui::Canvas pianoCanvas({pixels.data(), pixels.size(), kWidth, kHeight});
    std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns> allKeys{};
    allKeys.fill(true);
    const ::ui::Rect pianoRect{4, 4, 132, 50};
    ui_render::DrawMiniPiano(pianoCanvas, pianoRect, pianoHome, allKeys, 1.0f);
    const auto lit = [&](int x, int y) { return pixels[static_cast<std::size_t>(y * kWidth + x)] == ui_render::kAccent; };

    std::vector<bool> seen(pixels.size(), false);
    std::vector<std::array<int, 4>> keys;  // minX, maxX, minY, maxY per lit region
    for (int y = 0; y < kHeight; ++y)
      for (int x = 0; x < kWidth; ++x)
      {
        if (!lit(x, y) || seen[static_cast<std::size_t>(y * kWidth + x)]) continue;
        std::array<int, 4> bounds{x, x, y, y};
        std::vector<std::pair<int, int>> stack{{x, y}};
        seen[static_cast<std::size_t>(y * kWidth + x)] = true;
        while (!stack.empty())
        {
          const auto [px, py] = stack.back();
          stack.pop_back();
          bounds = {std::min(bounds[0], px), std::max(bounds[1], px), std::min(bounds[2], py), std::max(bounds[3], py)};
          for (const auto& [dx, dy] : std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}})
          {
            const int nx = px + dx;
            const int ny = py + dy;
            if (nx < 0 || ny < 0 || nx >= kWidth || ny >= kHeight || !lit(nx, ny) || seen[static_cast<std::size_t>(ny * kWidth + nx)]) continue;
            seen[static_cast<std::size_t>(ny * kWidth + nx)] = true;
            stack.push_back({nx, ny});
          }
        }
        keys.push_back(bounds);
      }
    assert(keys.size() == 12);

    const int bottom = std::max_element(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a[3] < b[3]; })->at(3);
    int whiteWidth = -1;
    for (const auto& key : keys)
    {
      if (key[3] == bottom)
      {
        assert(whiteWidth < 0 || key[1] - key[0] == whiteWidth);
        whiteWidth = key[1] - key[0];
        continue;
      }
      // A black key spans an odd width whose centre column is the gap between two lit whites.
      const int centre = (key[0] + key[1]) / 2;
      assert((key[1] - key[0]) % 2 == 0);
      assert(!lit(centre, bottom) && lit(centre - 1, bottom) && lit(centre + 1, bottom));
    }
  }
  assert(ui.RestoreKeyboardSettings({0, 2, 0, 1}));
  const auto scaleHome = ui.CurrentScreenSnapshot(nullptr);
  assert(scaleHome.keyboardRootNote == 2 && scaleHome.title == "TRACK 1");
  for (std::size_t i = 0; i < kHomePerformanceKeyCount; ++i) assert(scaleHome.performanceKeyNotes[i] >= 0);

  // A held key is "pressed"; a latched choice is "active"; play turns on only while running.
  const auto homeBar = ui.CurrentScreenSnapshot(nullptr).rightBarItems;
  assert(homeBar[2].icon == UiBarIcon::Stop && !homeBar[3].playing && !homeBar[0].pressed);
  ui.PressMatrixKey(engine, 0, kHomeRightBarColumn);
  assert(ui.CurrentScreenSnapshot(nullptr).rightBarItems[0].pressed && !ui.CurrentScreenSnapshot(nullptr).rightBarItems[0].active);
  ui.ReleaseMatrixKey(engine, 0, kHomeRightBarColumn);
  std::cout << "UI transport, project confirmations/cancels, failed preset saves, menu reachability and sequencer controls passed\n";
}

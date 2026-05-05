#include "Applications/Synth/SynthApp.h"

#include <array>
#include <iomanip>
#include <sstream>

#include "Applications/Application.h"
#include "Devices/Device.h"
#include "Library/Synth/SynthUiRenderer.h"
#include "OS/MIDI/MIDI.h"
#include "OS/UI/Canvas.h"

namespace apps {

namespace {

constexpr std::size_t KeyIndex(std::size_t row, std::size_t col) {
  return row * synth::kKeyboardMatrixColumns + col;
}

std::unique_ptr<app::Application> CreateSynthApplication() {
  return std::make_unique<SynthApp>();
}

app::ApplicationInfo BuildSynthInfo() {
  app::ApplicationInfo info;
  info.id = app::MakeApplicationId("ComputeCard", "Synth");
  info.name = "Synth";
  info.author = "ComputeCard";
  info.factory = &CreateSynthApplication;
  return info;
}

}  // namespace

void RegisterSynthApplication() {
  static const bool registered = app::RegisterApplication(BuildSynthInfo());
  (void)registered;
}

void SynthApp::Setup(const std::vector<std::string>& args) {
  (void)args;
  if (initialized_)
  {
    return;
  }

  os::midi::Initialize();
  rack_.AttachEngine(0, synth::SynthEngineKind::Dx7);
  ui_.InitializeTrack(0, synth::SynthEngineKind::Dx7);
  rack_.AttachEngine(1, synth::SynthEngineKind::Juno);
  ui_.InitializeTrack(1, synth::SynthEngineKind::Juno);
  rack_.AttachEngine(2, synth::SynthEngineKind::Sampler);
  ui_.InitializeTrack(2, synth::SynthEngineKind::Sampler);
  rack_.AttachEngine(3, synth::SynthEngineKind::Dx7);
  ui_.InitializeTrack(3, synth::SynthEngineKind::Dx7);
  ui_.SetActiveTrack(0);
  SyncUiTracks();
  initialized_ = true;
}

void SynthApp::Tick() {
  ui_.Tick(ActiveEngine());
  rack_.DispatchPendingMessages();
  SyncUiTracks();
}

void SynthApp::RenderFrame() {
  ui::Canvas canvas(Device::Display::MutableFrameBuffer());
  const std::size_t activeTrack = ui_.ActiveTrack();
  const auto& slot = rack_.Slot(activeTrack);
  synth::RenderSynthUiFrame(canvas, ui_, slot.engine.get(), activeKeys_[activeTrack]);
}

void SynthApp::HandleMatrixKeyEvent(std::size_t row, std::size_t col, bool pressed) {
  if (pressed)
  {
    (void)PressMatrixKey(row, col);
    return;
  }

  ReleaseMatrixKey(row, col);
}

synth::MatrixKeyHandling SynthApp::PressMatrixKey(std::size_t row, std::size_t col) {
  const auto handling = ui_.PressMatrixKey(ActiveEngineMutable(), row, col);
  SyncUiTracks();
  if (handling == synth::MatrixKeyHandling::PerformanceKey)
  {
    PressPerformanceKey(row, col);
  }
  return handling;
}

void SynthApp::ReleaseMatrixKey(std::size_t row, std::size_t col) {
  const std::size_t contextMenuColumns = ui_.ContextMenuColumnCount();
  const std::size_t contextMenuFirstColumn = contextMenuColumns == 1 ? synth::kHomeRightBarColumn : synth::kContextMenuFirstColumn;
  const bool isContextMenuKey = ui_.Mode() == synth::UiMode::ContextMenu &&
                                col >= contextMenuFirstColumn &&
                                col < contextMenuFirstColumn + contextMenuColumns;
  if (col == synth::kHomeLeftBarColumn || col == synth::kHomeRightBarColumn || isContextMenuKey)
  {
    ui_.ReleaseMatrixKey(ActiveEngine(), row, col);
    SyncUiTracks();
    return;
  }

  ReleasePerformanceKey(row, col);
}

synth::ISynthEngine* SynthApp::ActiveEngineMutable() {
  const std::size_t trackIndex = ui_.ActiveTrack();
  return trackIndex < synth::kSynthTrackCount ? rack_.Slot(trackIndex).engine.get() : nullptr;
}

const synth::ISynthEngine* SynthApp::ActiveEngine() const {
  const std::size_t trackIndex = ui_.ActiveTrack();
  return trackIndex < synth::kSynthTrackCount ? rack_.Slot(trackIndex).engine.get() : nullptr;
}

void SynthApp::SyncUiTracks() {
  for (std::size_t index = 0; index < synth::kSynthTrackCount; ++index)
  {
    ui_.SyncTrackState(index, rack_.Slot(index).engine.get());
  }
}

bool SynthApp::IsKeyActive(std::size_t trackIndex, std::size_t keyIndex) const {
  if (trackIndex >= activeKeys_.size() || keyIndex >= activeKeys_[trackIndex].size())
  {
    return false;
  }

  return activeKeys_[trackIndex][keyIndex];
}

const std::array<bool, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns>& SynthApp::ActiveKeyStates(std::size_t trackIndex) const {
  static const std::array<bool, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns> kEmpty{};
  if (trackIndex >= activeKeys_.size())
  {
    return kEmpty;
  }
  return activeKeys_[trackIndex];
}

void SynthApp::PressPerformanceKey(std::size_t row, std::size_t col) {
  const std::size_t keyIndex = KeyIndex(row, col);
  const std::size_t trackIndex = ui_.ActiveTrack();
  if (trackIndex >= activeKeys_.size() || keyIndex >= activeKeys_[trackIndex].size() || activeKeys_[trackIndex][keyIndex])
  {
    return;
  }

  activeKeys_[trackIndex][keyIndex] = true;
  os::midi::Send(midi::MidiPacket::NoteOn(0, synth::HomePerformanceMidiNote(row, col), 110), rack_.PortForTrack(trackIndex));
}

void SynthApp::ReleasePerformanceKey(std::size_t row, std::size_t col) {
  const std::size_t keyIndex = KeyIndex(row, col);
  for (std::size_t trackIndex = 0; trackIndex < activeKeys_.size(); ++trackIndex)
  {
    if (keyIndex >= activeKeys_[trackIndex].size() || !activeKeys_[trackIndex][keyIndex])
    {
      continue;
    }

    activeKeys_[trackIndex][keyIndex] = false;
    os::midi::Send(midi::MidiPacket::NoteOff(0, synth::HomePerformanceMidiNote(row, col), 0), rack_.PortForTrack(trackIndex));
    return;
  }
}

std::string SynthApp::DebugView() const {
  std::ostringstream stream;

  const std::size_t activeTrack = ui_.ActiveTrack();
  const auto& slot = rack_.Slot(activeTrack);
  const synth::ISynthEngine* engine = slot.engine.get();

  stream << ui_.DebugSummary(engine) << "\n";
  stream << ui_.HeaderEngine(engine) << "  " << ui_.HeaderTrack() << "\n";
  stream << ui_.PatchName() << "\n";

  if (ui_.Mode() == synth::UiMode::ContextMenu)
  {
    stream << ui_.ContextSectionTitle() << "\n";
    stream << ui_.ContextParameterName(engine) << " = " << ui_.ContextValueText(engine) << "\n";

    const auto menu = ui_.CurrentMenuCells(engine);
    for (std::size_t row = 0; row < synth::kContextMenuRows; ++row)
    {
      for (std::size_t column = 0; column < synth::kContextMenuColumns; ++column)
      {
        const std::size_t index = row * synth::kContextMenuColumns + column;
        stream << '[' << menu[index].label;
        if (ui_.IsMenuCellActive(row, column))
        {
          stream << '*';
        }
        stream << ']';
        if (column + 1 < synth::kContextMenuColumns)
        {
          stream << ' ';
        }
      }
      stream << "\n";
    }
  }

  for (const std::string& summary : rack_.DescribeSlots())
  {
    stream << summary << "\n";
  }

  return stream.str();
}

}  // namespace apps

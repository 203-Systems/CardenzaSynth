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

constexpr std::uint8_t kActiveLedBrightness = Device::LED::kBrightnessMax;
constexpr std::uint8_t kHintLedBrightness = 5;

bool HasVisibleBarItem(const synth::UiBarItemSnapshot& item) {
  return !item.label.empty() || item.icon != synth::UiBarIcon::None;
}

std::uint8_t LedBrightnessForBarItem(const synth::UiBarItemSnapshot& item) {
  if (!HasVisibleBarItem(item)) {
    return Device::LED::kBrightnessOff;
  }

  return item.active ? kActiveLedBrightness : kHintLedBrightness;
}

void WriteActiveBarLeds(const synth::UiScreenSnapshot& snapshot) {
  for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
  {
    const std::uint8_t brightness = LedBrightnessForBarItem(snapshot.leftBarItems[row]);
    if (brightness != Device::LED::kBrightnessOff)
    {
      Device::LED::SetKey(row, synth::kHomeLeftBarColumn, brightness);
    }
  }

  if (!snapshot.rightBarVisible)
  {
    return;
  }

  if (snapshot.mode == synth::UiMode::Home)
  {
    for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
    {
      const std::uint8_t brightness = LedBrightnessForBarItem(snapshot.rightBarItems[row]);
      if (brightness != Device::LED::kBrightnessOff)
      {
        Device::LED::SetKey(row, synth::kHomeRightBarColumn, brightness);
      }
    }
    return;
  }

  const std::size_t columns = snapshot.rightBarColumns;
  const std::size_t firstColumn = synth::kKeyboardMatrixColumns - columns;
  for (std::size_t row = 0; row < synth::kContextMenuRows; ++row)
  {
    for (std::size_t column = 0; column < columns; ++column)
    {
      const std::size_t index = columns == 1 ? row : row * synth::kContextMenuColumns + column;
      const std::uint8_t brightness = LedBrightnessForBarItem(snapshot.rightBarItems[index]);
      if (brightness != Device::LED::kBrightnessOff)
      {
        Device::LED::SetKey(row, firstColumn + column, brightness);
      }
    }
  }
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

void SynthApp::UpdateLeds() {
  Device::LED::SetGlobalBrightness(Device::LED::kBrightnessMax);
  Device::LED::Clear();

  const std::size_t activeTrack = ui_.ActiveTrack();
  if (activeTrack < activeKeys_.size())
  {
    for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
    {
      for (std::size_t column = synth::kHomePerformanceFirstColumn; column <= synth::kHomePerformanceLastColumn; ++column)
      {
        if (activeKeys_[activeTrack][KeyIndex(row, column)])
        {
          Device::LED::SetKey(row, column, kActiveLedBrightness);
        }
      }
    }
  }

  WriteActiveBarLeds(ui_.CurrentScreenSnapshot(ActiveEngine()));
  Device::LED::SetEnabled(true);
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
  ApplyPendingUiActions();
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

void SynthApp::ApplyPendingUiActions()
{
  synth::SynthEngineKind nextEngineKind = synth::SynthEngineKind::None;
  if (ui_.ConsumePendingEngineKind(nextEngineKind))
  {
    SwitchTrackEngine(ui_.ActiveTrack(), nextEngineKind);
  }
}

synth::ISynthEngine* SynthApp::ActiveEngineMutable() {
  const std::size_t trackIndex = ui_.ActiveTrack();
  return trackIndex < synth::kSynthTrackCount ? rack_.Slot(trackIndex).engine.get() : nullptr;
}

const synth::ISynthEngine* SynthApp::ActiveEngine() const {
  const std::size_t trackIndex = ui_.ActiveTrack();
  return trackIndex < synth::kSynthTrackCount ? rack_.Slot(trackIndex).engine.get() : nullptr;
}

void SynthApp::SwitchTrackEngine(std::size_t trackIndex, synth::SynthEngineKind kind)
{
  if (trackIndex >= synth::kSynthTrackCount || rack_.Slot(trackIndex).kind == kind)
  {
    return;
  }

  ReleaseTrackPerformanceKeys(trackIndex);
  (void)rack_.AttachEngine(trackIndex, kind);
}

void SynthApp::SyncUiTracks() {
  for (std::size_t index = 0; index < synth::kSynthTrackCount; ++index)
  {
    ui_.SyncTrackState(index, rack_.Slot(index).engine.get());
  }
}

void SynthApp::ReleaseTrackPerformanceKeys(std::size_t trackIndex)
{
  if (trackIndex >= activeKeys_.size())
  {
    return;
  }

  for (std::size_t row = 0; row < synth::kKeyboardMatrixRows; ++row)
  {
    for (std::size_t col = synth::kHomePerformanceFirstColumn; col <= synth::kHomePerformanceLastColumn; ++col)
    {
      const std::size_t keyIndex = KeyIndex(row, col);
      if (!activeKeys_[trackIndex][keyIndex])
      {
        continue;
      }

      activeKeys_[trackIndex][keyIndex] = false;
      os::midi::Send(midi::MidiPacket::NoteOff(0, synth::HomePerformanceMidiNote(row, col), 0), rack_.PortForTrack(trackIndex));
    }
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
  if (trackIndex >= activeKeys_.size() || keyIndex >= activeKeys_[trackIndex].size())
  {
    return;
  }

  const std::uint8_t note = synth::HomePerformanceMidiNote(row, col);
  for (std::size_t heldTrackIndex = 0; heldTrackIndex < activeKeys_.size(); ++heldTrackIndex)
  {
    if (keyIndex >= activeKeys_[heldTrackIndex].size() || !activeKeys_[heldTrackIndex][keyIndex])
    {
      continue;
    }

    activeKeys_[heldTrackIndex][keyIndex] = false;
    os::midi::Send(midi::MidiPacket::NoteOff(0, note, 0), rack_.PortForTrack(heldTrackIndex));
  }

  activeKeys_[trackIndex][keyIndex] = true;
  os::midi::Send(midi::MidiPacket::NoteOn(0, note, 110), rack_.PortForTrack(trackIndex));
}

void SynthApp::ReleasePerformanceKey(std::size_t row, std::size_t col) {
  const std::size_t keyIndex = KeyIndex(row, col);
  const std::uint8_t note = synth::HomePerformanceMidiNote(row, col);
  for (std::size_t trackIndex = 0; trackIndex < activeKeys_.size(); ++trackIndex)
  {
    if (keyIndex >= activeKeys_[trackIndex].size() || !activeKeys_[trackIndex][keyIndex])
    {
      continue;
    }

    activeKeys_[trackIndex][keyIndex] = false;
    os::midi::Send(midi::MidiPacket::NoteOff(0, note, 0), rack_.PortForTrack(trackIndex));
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

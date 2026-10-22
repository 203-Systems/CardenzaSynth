#include "Applications/Synth/SynthApp.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
#include "OS/System/System.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Exercise the same device facade, OS input dispatch and RGB565 renderer as the
// hardware. An optional output directory captures real frames on every host OS.
namespace {
using namespace synth;
std::filesystem::path output;

void Key(std::size_t row, std::size_t column, bool down)
{
  CardenzaSim::Input::SetMatrixKey(row, column, down);
  os::system::RunFrame();
}
void Tap(std::size_t row, std::size_t column)
{
  Key(row, column, true);
  Key(row, column, false);
}
void Home()
{
  CardenzaSim::Input::SetHomeButton(true);
  os::system::RunFrame();
  CardenzaSim::Input::SetHomeButton(false);
  os::system::RunFrame();
}
void Wait(int milliseconds)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
  os::system::RunFrame();
}
std::vector<Device::Display::Pixel> Pixels()
{
  const auto frame = Device::Display::CurrentFrameBuffer();
  return {frame.pixels, frame.pixels + frame.pixelCount};
}
void Capture(const std::string& name)
{
  if (output.empty()) return;
  const auto frame = Device::Display::CurrentFrameBuffer();
  std::ofstream file(output / (name + ".ppm"), std::ios::binary);
  file << "P6\n" << frame.width << ' ' << frame.height << "\n255\n";
  for (std::size_t i = 0; i < frame.pixelCount; ++i)
  {
    const auto pixel = frame.pixels[i];
    const unsigned red = (pixel >> 11) & 31;
    const unsigned green = (pixel >> 5) & 63;
    const unsigned blue = pixel & 31;
    const char rgb[] = {static_cast<char>((red * 255) / 31), static_cast<char>((green * 255) / 63), static_cast<char>((blue * 255) / 31)};
    file.write(rgb, sizeof(rgb));
  }
  assert(file.good());
  std::ofstream leds(output / (name + ".leds.json"));
  leds << "{\"master\":" << static_cast<unsigned>(Device::LED::GlobalBrightness()) << ",\"rows\":[";
  for (std::size_t row = 0; row < kKeyboardMatrixRows; ++row)
  {
    if (row) leds << ',';
    leds << '[';
    for (std::size_t column = 0; column < kKeyboardMatrixColumns; ++column)
      leds << (column ? "," : "") << static_cast<unsigned>(Device::LED::Brightness(row, column));
    leds << ']';
  }
  leds << "]}\n";
  assert(leds.good());
}
}

int main(int argc, char** argv)
{
  if (argc > 1)
  {
    output = argv[1];
    std::filesystem::create_directories(output);
  }
  assert(os::system::Begin());
  auto* app = dynamic_cast<apps::SynthApp*>(os::system::ActiveApplication());
  assert(app != nullptr);
  const auto screen = [&] { return app->Ui().CurrentScreenSnapshot(app->Rack().Slot(app->Ui().ActiveTrack()).engine.get()); };
  const auto stepLed = [](std::size_t step) { return Device::LED::Brightness(SequencerStepRow(step), SequencerStepColumn(step)); };
  assert(stepLed(0) > 0 && stepLed(0) == stepLed(15));
  assert(Device::LED::Brightness(0, kHomeRightBarColumn) > stepLed(0));
  assert(Device::LED::Brightness(0, 0) > Device::LED::Brightness(1, 0));
  assert(Device::LED::Brightness(3, 1) > Device::LED::Brightness(3, 2));
  // Staging an update keeps the previous complete frame visible and enabled.
  const auto previousHint = Device::LED::Brightness(0, 0);
  Device::LED::BeginFrame();
  Device::LED::Clear();
  Device::LED::SetEnabled(false);
  assert(Device::LED::Enabled() && Device::LED::Brightness(0, 0) == previousHint);
  app->UpdateLeds();
  Device::LED::Present();
  assert(Device::LED::Enabled() && Device::LED::Brightness(0, 0) == previousHint);
  Capture("01-keys");
  Home();
  assert(screen().showSequencerGrid && screen().sequencerReveal < 1.0f);
  assert(Device::LED::Brightness(3, 1) > 0 && Device::LED::Brightness(3, 2) == 0);
  Capture("02-grid-enter");
  const auto entering = Pixels();
  if (!output.empty())
    for (int i = 0; i < 12; ++i)
    {
      Wait(16);
      Capture("enter-" + std::to_string(1000 + i));
    }
  Wait(200);
  assert(screen().sequencerReveal == 1.0f && entering != Pixels());
  Capture("03-grid-empty");

  Key(SequencerStepRow(0), SequencerStepColumn(0), true);
  const auto pressedLed = stepLed(0);
  Key(SequencerStepRow(0), SequencerStepColumn(0), false);
  assert(screen().sequencer.selectionLatched && !screen().sequencer.steps[0].hasEvents);
  const auto latchedLed = stepLed(0);
  assert(pressedLed > latchedLed && latchedLed > stepLed(1));
  for (int frame = 0; frame < 20; ++frame)
  {
    Wait(16);
    assert(Device::LED::Enabled() && stepLed(0) == latchedLed && stepLed(1) > 0);
  }
  Capture("04-step-selected");
  for (const auto column : {1u, 3u, 5u}) Tap(3, column);
  assert(screen().sequencer.steps[0].noteCount == 3 && screen().sequencer.steps[0].editFlash > 0.0f);
  Capture("05-chord-feedback");
  const auto flash = Pixels();
  if (!output.empty())
    for (int i = 0; i < 14; ++i)
    {
      Wait(16);
      Capture("edit-" + std::to_string(1000 + i));
    }
  Wait(250);
  assert(screen().sequencer.steps[0].editFlash == 0.0f && flash != Pixels());
  Capture("06-chord");
  Tap(2, kHomeLeftBarColumn);
  Tap(3, 5);
  assert(screen().sequencer.editValueIndex == 4 && screen().sequencer.valueLabel == "VEL 88");
  Wait(250);
  assert(Device::LED::Brightness(2, 0) > Device::LED::Brightness(1, 0));
  assert(Device::LED::Brightness(3, 5) > Device::LED::Brightness(3, 4));
  assert(Device::LED::Brightness(0, 5) > 0 && Device::LED::Brightness(0, 4) == 0);
  Capture("07-velocity");
  Tap(1, kHomeLeftBarColumn);
  Tap(3, 4);
  Wait(250);
  Capture("08-length");
  Tap(3, kHomeLeftBarColumn);
  Tap(3, 2);
  Wait(250);
  assert(Device::LED::Brightness(3, 8) == 0);
  Capture("09-timing");
  Home();
  assert(screen().showSequencerGrid && !screen().sequencer.hasHeldSteps);
  assert(stepLed(0) > stepLed(1) && stepLed(0) < latchedLed);
  for (const auto step : {4u, 8u, 12u})
  {
    Tap(SequencerStepRow(step), SequencerStepColumn(step));
    Tap(3, step == 8 ? 5 : 1);
    Home();
  }
  Wait(250);
  Capture("10-pattern");
  Tap(3, kHomeRightBarColumn);
  assert(screen().sequencer.playing);
  const auto firstStep = screen().sequencer.currentStep;
  Wait(160);
  assert(screen().sequencer.currentStep != firstStep);
  assert(stepLed(screen().sequencer.currentStep) > latchedLed);
  Capture("11-playing");
  if (!output.empty())
  {
    for (int i = 0; i < 130; ++i)
    {
      Wait(16);
      Capture("play-" + std::to_string(1000 + i));
    }
  }
  Tap(2, kHomeRightBarColumn);
  assert(!screen().sequencer.playing);
  Capture("12-stopped");
  Key(0, kHomeRightBarColumn, true);
  Wait(210);
  assert(screen().sequencer.patternSelectorVisible);
  assert(Device::LED::Brightness(0, 1) > Device::LED::Brightness(0, 2));
  assert(Device::LED::Brightness(2, 1) == 0 && Device::LED::Brightness(3, 8) == 0);
  Capture("13-pattern-leds");
  Key(0, kHomeRightBarColumn, false);
  assert(!screen().sequencer.patternSelectorVisible);
  Tap(1, 0);
  assert(app->Ui().ActiveTrack() == 1);
  assert(Device::LED::Brightness(1, 0) > Device::LED::Brightness(0, 0));
  Capture("14-track-leds");
  Key(1, 0, true);
  Wait(210);
  assert(app->Ui().Mode() == UiMode::ContextMenu);
  assert(Device::LED::Brightness(1, 0) > Device::LED::Brightness(0, 0));
  assert(Device::LED::Brightness(0, kContextMenuFirstColumn) > Device::LED::Brightness(0, 0));
  Capture("15-context-leds");
  Key(1, 0, false);
  std::cout << "Native input, latched step editing, value lanes, timed feedback and playback rendering passed\n";
}

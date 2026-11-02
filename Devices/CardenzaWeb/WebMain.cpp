// CardenzaWeb: the browser host for the CardenzaSim device facade.
//
// Boots the production runtime exactly like the device and CardenzaSimShot
// (os::system::Begin, then os::system::RunFrame at the display rate) from an
// emscripten_set_main_loop, and exposes a small C API to the page
// (web/cardenza-web.js): key and HOME input through CardenzaSim::Input, the RGB565
// framebuffer, the LED frame, and audio rendering for Web Audio.
//
// The build has no threads, so the sequencer clock that runs on its own thread everywhere else
// is pumped from the audio render path (see cardenza_web_render). Everything runs on the
// browser's main thread.

#include <emscripten/emscripten.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

#include "Devices/LedLight.h"
#include "Applications/Synth/GrooveApp.h"
#include "Applications/Synth/UI/Groove/GrooveHost.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Device.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Library/Synth/Core/SynthStorage.h"
#include "OS/Storage/Settings.h"
#include "OS/Storage/Storage.h"
#include "OS/System/System.h"

namespace {

constexpr int kRows = Device::Keyboard::kRows;
constexpr int kColumns = Device::Keyboard::kColumns;
// Same 16 ms period as the device runtime task and CardenzaSimShot.
constexpr double kFramePeriodMs = 16.0;
// Largest single audio request from the page (about 186 ms at 44.1 kHz).
constexpr int kMaxRenderFrames = 8192;
// AMY renders 256-frame blocks; the sequencer is pumped before each one.
constexpr int kClockBlockFrames = 256;

// LED snapshot for the page: the light of each of the 56 keys as the device gives it off (0..255,
// Devices/LedLight.h), then master (0..15), enabled, backlight (0..255).
constexpr std::size_t kLedSnapshotSize = static_cast<std::size_t>(Device::LED::kLedCount) + 3;

bool g_booted = false;
double g_nextFrameMs = 0.0;
std::uint32_t g_frameCounter = 0;
std::uint64_t g_renderedFrames = 0;
std::array<std::uint8_t, kLedSnapshotSize> g_ledSnapshot{};
std::array<std::int16_t, kMaxRenderFrames> g_renderPcm{};
std::array<float, kMaxRenderFrames> g_renderFloat{};
std::string g_sampleDirectory;
std::string g_projectDirectory;

// On every other target the sequencer runs on its own clock thread. Here the host drives it, on
// the audio timeline: the time of the frame about to be rendered. Pulses then land on exact block
// positions in the output however the main thread is scheduled, and the sequencer never runs
// ahead of the audio it is heard in. The host reaches it through the app's groove::Host.
void PumpSequencerClock()
{
  if (auto* host = dynamic_cast<groove::Host*>(os::system::ActiveApplication()))
  {
    const std::uint64_t audioMicros = g_renderedFrames * 1000000ULL / synth::audio::kSampleRate;
    host->Sequencer().PumpClock(audioMicros);
  }
}

void SnapshotLeds()
{
  // The light each key gives off on the device (its PWM tier at the current master brightness), so
  // the page's keys look like the real ones.
  const std::uint8_t master = Device::LED::GlobalBrightness();
  const bool enabled = Device::LED::Enabled();
  for (int row = 0; row < kRows; ++row)
  {
    for (int column = 0; column < kColumns; ++column)
    {
      const std::uint8_t level = Device::LED::Brightness(static_cast<std::size_t>(row), static_cast<std::size_t>(column));
      const unsigned light = enabled ? Device::LED::LightPermille(level, master) : 0u;
      g_ledSnapshot[static_cast<std::size_t>(row * kColumns + column)] = static_cast<std::uint8_t>((light * 255u + 500u) / 1000u);
    }
  }
  g_ledSnapshot[Device::LED::kLedCount] = master;
  g_ledSnapshot[Device::LED::kLedCount + 1] = enabled ? 1 : 0;
  g_ledSnapshot[Device::LED::kLedCount + 2] = Device::Display::Backlight();
}

// One device frame: RunFrame dispatches input, ticks the app, updates the LEDs and renders the
// screen, exactly as on the device.
void RunDeviceFrame()
{
  os::system::RunFrame();
  SnapshotLeds();
  ++g_frameCounter;
}

void MainLoopTick()
{
  // requestAnimationFrame follows the display; keep the device's ~60 fps on faster displays.
  const double now = emscripten_get_now();
  if (now + 4.0 < g_nextFrameMs)
  {
    return;
  }
  g_nextFrameMs += kFramePeriodMs;
  if (g_nextFrameMs < now)
  {
    g_nextFrameMs = now + kFramePeriodMs;
  }

  RunDeviceFrame();
  // The page copies the framebuffer and LEDs and feeds the audio output.
  EM_ASM({
    if (typeof Module.cardenzaPresent === 'function') Module.cardenzaPresent();
  });
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE std::uint32_t cardenza_web_frame_counter() { return g_frameCounter; }

// Device::Display framebuffer: 240 x 135 RGB565, row major.
EMSCRIPTEN_KEEPALIVE const std::uint16_t* cardenza_web_framebuffer()
{
  return Device::Display::CurrentFrameBuffer().pixels;
}

// Device::LED state of the last frame, see kLedSnapshotSize.
EMSCRIPTEN_KEEPALIVE const std::uint8_t* cardenza_web_leds() { return g_ledSnapshot.data(); }

EMSCRIPTEN_KEEPALIVE int cardenza_web_sample_rate() { return static_cast<int>(synth::audio::kSampleRate); }

// Shared/Samples of the storage protocol (docs/app-storage-protocol.md): "Load WAV" writes here and
// every app's sample browser reads it.
EMSCRIPTEN_KEEPALIVE const char* cardenza_web_sample_directory()
{
  (void)os::storage::EnsureSystemLayout();
  g_sampleDirectory = os::storage::SharedSamplesDirectory();
  return g_sampleDirectory.c_str();
}

// The groovebox's Projects folder: "Load project" writes here.
EMSCRIPTEN_KEEPALIVE const char* cardenza_web_project_directory()
{
  (void)synth::storage::EnsureLayout();
  g_projectDirectory = synth::storage::ProjectDirectory();
  return g_projectDirectory.c_str();
}

// Opens a project of that folder the way FILES does.
EMSCRIPTEN_KEEPALIVE int cardenza_web_open_project(const char* name)
{
  auto* groove = dynamic_cast<apps::GrooveApp*>(os::system::ActiveApplication());
  if (groove == nullptr) return 0;
  groove->Ui().OpenProject(name);
  return 1;
}

// Writes pending settings now instead of a second after the last change. The page calls it when
// it is hidden or closed; IDBFS (autoPersist) then commits the files to IndexedDB.
EMSCRIPTEN_KEEPALIVE int cardenza_web_flush_storage()
{
  if (!g_booted)
  {
    return 0;
  }
  return os::storage::FlushAllSettings() ? 1 : 0;
}

// Like CardenzaSimShot and the Win32 host, every key change runs a frame right away, so a press
// and release that arrive between two display frames are both dispatched.
EMSCRIPTEN_KEEPALIVE void cardenza_web_set_key(int row, int column, int pressed)
{
  if (!g_booted || row < 0 || row >= kRows || column < 0 || column >= kColumns)
  {
    return;
  }
  CardenzaSim::Input::SetMatrixKey(static_cast<std::size_t>(row), static_cast<std::size_t>(column), pressed != 0);
  RunDeviceFrame();
}

EMSCRIPTEN_KEEPALIVE void cardenza_web_set_home(int pressed)
{
  if (!g_booted)
  {
    return;
  }
  CardenzaSim::Input::SetHomeButton(pressed != 0);
  RunDeviceFrame();
}

// Focus loss: nothing may stay held.
EMSCRIPTEN_KEEPALIVE void cardenza_web_release_all()
{
  if (!g_booted)
  {
    return;
  }
  CardenzaSim::Input::ClearMatrixKeys();
  CardenzaSim::Input::SetHomeButton(false);
  RunDeviceFrame();
}

// Renders the next `frames` mono frames of the device output at cardenza_web_sample_rate() and
// returns them as floats in -1..1 with the device output volume applied (the firmware applies it
// after the synth mix). The page calls this for the audio output and, while no output runs, at
// the wall-clock rate with the result discarded, so the synth and sequencer always run in real
// time.
EMSCRIPTEN_KEEPALIVE const float* cardenza_web_render(int frames)
{
  const int count = std::clamp(frames, 0, kMaxRenderFrames);
  if (!g_booted)
  {
    std::fill(g_renderFloat.begin(), g_renderFloat.begin() + count, 0.0f);
    return g_renderFloat.data();
  }

  int done = 0;
  while (done < count)
  {
    const int offset = static_cast<int>(g_renderedFrames % kClockBlockFrames);
    const int length = std::min(count - done, kClockBlockFrames - offset);
    if (offset == 0)
    {
      PumpSequencerClock();
    }
    synth::audio::RenderFrames(g_renderPcm.data() + done, static_cast<std::size_t>(length));
    done += length;
    g_renderedFrames += static_cast<std::uint64_t>(length);
  }

  const float gain = static_cast<float>(Device::Audio::OutputVolume()) / (100.0f * 32768.0f);
  for (int index = 0; index < count; ++index)
  {
    g_renderFloat[static_cast<std::size_t>(index)] = static_cast<float>(g_renderPcm[static_cast<std::size_t>(index)]) * gain;
  }
  return g_renderFloat.data();
}

}  // extern "C"

int main()
{
  // The page mounts the persistent /sdcard (IDBFS) and loads it before main runs, so the storage
  // layout and any saved projects are in place when the app boots.
  if (!os::system::Begin())
  {
    std::fprintf(stderr, "CardenzaWeb: failed to boot the Cardenza runtime\n");
    EM_ASM({
      if (typeof Module.cardenzaFailed === 'function') Module.cardenzaFailed('The Cardenza runtime failed to boot');
    });
    return 1;
  }
  g_booted = true;
  SnapshotLeds();
  std::printf("CardenzaWeb: %s [%s] %dx%d, storage %s\n", Device::Name().c_str(), Device::BackendName().c_str(),
              Device::Display::kWidth, Device::Display::kHeight, Device::Storage::RootPath().c_str());

  g_nextFrameMs = emscripten_get_now();
  EM_ASM({
    if (typeof Module.cardenzaBooted === 'function') Module.cardenzaBooted();
  });
  // fps 0 = requestAnimationFrame; MainLoopTick paces the device frames itself.
  emscripten_set_main_loop(MainLoopTick, 0, true);
  return 0;
}

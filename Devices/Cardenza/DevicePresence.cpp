#include "Devices/Cardenza/DevicePresence.h"

#include <atomic>
#include <cstdint>
#include <cstdio>

#include <esp_timer.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Devices/Device.h"
#include "Library/Synth/Audio/SampleStream.h"
#include "net/net.h"
#include "net/presence.h"

// AMY's oscillator state and event blocks that went to PSRAM for want of internal RAM
// (Library/Synth/Audio/Amy/AmyEmbeddedStubs.c).
extern "C" std::uint32_t cardenza_amy_fast_misses(void);

namespace device::presence {

namespace {

namespace Presence = CardenzaHAL::Presence;

// What the synth keeps of internal RAM beside Wi-Fi: its effects allocate only with 20 KB free, an
// engine with 16 KB beside its own size (a project's swap one engine at a time), the autosave with 12 KB
// (AGENT.md, Memory). Below it the service lets Wi-Fi go. 32 KB until 2026-11-18: Wi-Fi up (30 KB with its
// frames in PSRAM) and a request (10 KB) beside 32 KB did not fit the 70-80 KB the synth leaves, and the
// voices do not take from it while Wi-Fi is up (they give way, below).
constexpr std::size_t kRamFloorBytes = 24 * 1024;
// Sound this recent keeps the activity RealTime (a phrase's rests, a pattern's quiet steps).
constexpr std::uint32_t kSoundHoldMs = 15000;
// The longest a handover waits for the app to be shut down (the card may be slow); the working state
// is on the card already in any case (the autosave), this only adds the last moments.
constexpr std::uint32_t kHandoverWaitMs = 8000;

std::atomic<std::uint32_t> g_lastSoundMs{0};
std::atomic<bool> g_soundSeen{false};
std::atomic<bool> g_recording{false};
// The system loop's: the voices wanted internal RAM while Wi-Fi held it (AMY put an oscillator or an
// event block in PSRAM): Quiet until the sound stops, so they get it back.
std::uint32_t g_fastMisses = 0;
std::uint32_t g_missedAtMs = 0;  // the last such miss with Wi-Fi up
bool g_missed = false;
bool g_voicesFirst = false;
std::uint32_t g_voicesFirstCount = 0;
// A miss counts for sound that starts this soon after it (a note's voice is set up before its first block).
constexpr std::uint32_t kMissForSoundMs = 2000;
// 0 nothing, 1 asked of the system loop, 2 done.
std::atomic<int> g_handover{0};
Presence::Activity g_activity = Presence::Activity::Idle;  // the system loop's

std::uint32_t NowMs()
{
  return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

// On the service's task: the app's state to the card, then the restart into the Launcher.
bool BeforeHandover(const Presence::Handover& handover, void*)
{
  std::printf("Cardenza: the community sent %s \"%s\" %s: shutting down for the Launcher\n", handover.action,
              handover.firmwareName, handover.version);
  g_handover.store(1);
  const std::uint32_t start = NowMs();
  while (g_handover.load() != 2 && NowMs() - start < kHandoverWaitMs) vTaskDelay(pdMS_TO_TICKS(20));
  if (g_handover.load() != 2) std::printf("Cardenza: the app did not shut down in %u ms: restarting anyway\n", static_cast<unsigned>(kHandoverWaitMs));
  return true;
}

const char* StateName(Presence::State state)
{
  switch (state)
  {
  case Presence::State::Off: return "off";
  case Presence::State::Idle: return "idle";
  case Presence::State::Connecting: return "connecting";
  case Presence::State::Online: return "online";
  case Presence::State::Waiting: return "waiting";
  case Presence::State::SignedOut: return "signed out";
  case Presence::State::HandingOver: return "handing over";
  }
  return "?";
}

const char* WaitName(Presence::Wait wait)
{
  switch (wait)
  {
  case Presence::Wait::None: return "";
  case Presence::Wait::Activity: return "activity";
  case Presence::Wait::AudioLate: return "audio";
  case Presence::Wait::Ram: return "ram";
  case Presence::Wait::FirmwareRequest: return "request";
  case Presence::Wait::Failure: return "failure";
  }
  return "?";
}

}  // namespace

void Begin()
{
  Presence::SetRamFloor(kRamFloorBytes);
  Presence::SetHandoverHook(BeforeHandover, nullptr);
  // Once a start: whether this board goes online in the community while the synth runs.
#if CONFIG_CARDENZA_HAL_PRESENCE
  std::printf("Cardenza: community presence %s\n",
              Presence::Planned() ? "planned (bound, Wi-Fi saved, receiving on)" : "not planned in this run");
#else
  std::printf("Cardenza: community presence off in this build (CONFIG_CARDENZA_HAL_PRESENCE)\n");
#endif
}

void NoteSound()
{
  g_lastSoundMs.store(NowMs(), std::memory_order_relaxed);
  g_soundSeen.store(true, std::memory_order_relaxed);
}

void SetRecording(bool recording)
{
  g_recording.store(recording, std::memory_order_relaxed);
}

void Frame()
{
  const bool sounding =
      g_soundSeen.load(std::memory_order_relaxed) && NowMs() - g_lastSoundMs.load(std::memory_order_relaxed) < kSoundHoldMs;
  // The voices come first: AMY keeps a voice's state in internal RAM while 64 KB stay free (in PSRAM a
  // voice renders about a sixth slower), which Wi-Fi's 30 KB rules out while it is up. When AMY had to
  // use PSRAM with Wi-Fi up, Wi-Fi is let go until the sound has stopped (then the service joins again).
  // In silence (a preset or a project set up) it does not: Wi-Fi would come and go for nothing; the
  // voices' next allocations while they sound decide.
  const std::uint32_t now = NowMs();
  const std::uint32_t misses = cardenza_amy_fast_misses();
  if (misses != g_fastMisses)
  {
    g_fastMisses = misses;
    if (CardenzaHAL::Net::Connected())
    {
      g_missed = true;
      g_missedAtMs = now;
    }
  }
  if (!g_voicesFirst && sounding && g_missed && now - g_missedAtMs < kMissForSoundMs)
  {
    g_voicesFirst = true;
    // News once a run (it explains a device offline while it plays); then counted.
    if (g_voicesFirstCount++ == 0)
      std::printf("Cardenza: community presence gives way: the voices want the internal RAM Wi-Fi holds (until the sound stops)\n");
  }
  if (g_voicesFirst && !sounding)
  {
    g_voicesFirst = false;
    g_missed = false;
  }
  Presence::Activity activity = Presence::Activity::Idle;
  // Quiet while the microphone has the speaker's pin, while a sample plays from the card (its blocks are
  // internal RAM above a 40 KB floor: the card's DMA cannot reach PSRAM) and while the voices come first.
  if (g_recording.load(std::memory_order_relaxed) || synth::audio::SampleStreamsReading() || g_voicesFirst)
    activity = Presence::Activity::Quiet;
  else if (sounding)
    activity = Presence::Activity::RealTime;
  if (activity == g_activity) return;
  g_activity = activity;
  Presence::SetActivity(activity);
}

bool HandoverRequested()
{
  return g_handover.load() == 1;
}

void HandoverPrepared()
{
  g_handover.store(2);
}

}  // namespace device::presence

namespace Device::Community {

Status GetStatus()
{
  namespace Presence = CardenzaHAL::Presence;
  const Presence::Report report = Presence::GetReport();
  Status status;
  status.planned = Presence::Planned();
  status.state = device::presence::StateName(report.state);
  status.waiting = device::presence::WaitName(report.waiting);
  status.asked = report.asked;
  status.failed = report.failed;
  status.joins = report.joins;
  status.letGo = report.letGo;
  status.wifiBytes = report.wifiBytes;
  status.lastAskMs = report.lastAskMs;
  return status;
}

}  // namespace Device::Community

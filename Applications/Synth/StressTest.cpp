#include "Applications/Synth/StressTest.h"

#if defined(CARDENZA_STRESS_TEST) && defined(ESP_PLATFORM)

#include "Applications/Synth/GrooveApp.h"
#include "Applications/Synth/Runtime/ProjectStorage.h"
#include "Applications/Synth/Runtime/Sampler/SamplerWav.h"
#include "Devices/Device.h"
#include "Library/Memory/Memory.h"
#include "Library/Synth/Audio/SampleStream.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"
#include "Library/Synth/Core/SynthStorage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include <esp_app_desc.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#if defined(CARDENZA_STRESS_TRACE) && defined(CONFIG_HEAP_TRACING_STANDALONE)
#include <esp_heap_trace.h>
#include <esp_memory_utils.h>
#define CARDENZA_STRESS_HEAP_TRACE 1
#endif

extern "C" const char* cardenza_previous_crash();
extern "C" unsigned cardenza_failed_allocations();

namespace apps {
namespace {

#include "Applications/Synth/StressTestFixtures.inc"

using synth::SynthEngineKind;
using synth::SynthSequencer;

#if defined(CARDENZA_STRESS_START)
constexpr std::int64_t kStartAfterUs = static_cast<std::int64_t>(CARDENZA_STRESS_START) * 1000000;
#else
constexpr std::int64_t kStartAfterUs = 20 * 1000000;
#endif  // the USB console is there by then
constexpr std::size_t kMaxScenarios = 24;
constexpr const char* kPrefix = "ZZ-ST-";
constexpr std::array<std::uint8_t, 8> kKitNotes{{36, 38, 42, 46, 39, 43, 56, 70}};  // the drum machine's lanes

// What survives a reset (RTC slow memory, beside the log ring): where the run is and what it found.
// Plain members without initialisers: with them Persist had a constructor, which ran at every start
// and cleared the results in RTC memory (RTC_NOINIT_ATTR only skips the loader), keeping only the
// header: a run read after a restart showed every scenario "not run". Result{} still zeroes one.
struct Result {
  std::uint8_t state;  // 0 not run, 1 running, 2 done, 3 the device restarted during it
  std::uint8_t ok;
  std::uint16_t avgRenderUs;
  std::uint16_t maxRenderUs;
  std::uint16_t overruns;
  std::uint16_t underruns;
  std::uint16_t faded;
  std::uint16_t shed;
  std::uint16_t lostTails;    // let-go tails faded early
  std::uint16_t lostHeld;     // held notes faded: a note missing
  std::uint16_t lostUnheard;  // notes left out before they sounded: a note missing
  std::uint16_t lostStolen;   // held notes whose voice a newer one took (the track's voices all held)
  std::uint16_t dx7Us, junoUs, drumsUs, overheadUs;  // the budget's model at the end
  std::uint16_t scalePercent;
  std::uint16_t uiMaxMs;
  std::uint16_t uiDrops;
  std::uint16_t seqLatestUs;
  std::uint16_t seqLate;
  std::uint16_t ramMinKb;
  std::uint16_t psramMinKb;
  std::uint16_t opMaxMs;
  std::uint16_t opUnderruns;
  char note[24];
};
struct Persist {
  std::uint32_t magic;
  std::uint32_t build;
  std::uint32_t next;
  std::uint32_t reboots;
  Result results[kMaxScenarios];
};
static_assert(std::is_trivially_default_constructible_v<Persist>, "a constructor would clear it at every start");
constexpr std::uint32_t kMagic = 0x53545231u;  // "STR1"
RTC_NOINIT_ATTR Persist g_persist;

enum class Kind : std::uint8_t { Idle, Fixture, Generated, Card, LiveOps, UiStorm, Worst };
enum class Variant : std::uint8_t { None, Dx7Chords, JunoPads, DrumsFast, Arps, Sampler, FxMax, MaxPatterns, StreamFour, StreamSix };

struct Scenario {
  std::string name;
  Kind kind = Kind::Idle;
  Variant variant = Variant::None;
  std::string project;  // what it loads
  std::uint32_t seconds = 20;
};

std::uint32_t BuildId()
{
  char elf[9] = {};
  esp_app_get_elf_sha256(elf, sizeof(elf));
  return static_cast<std::uint32_t>(std::strtoul(elf, nullptr, 16));
}

std::uint32_t NowMs() { return static_cast<std::uint32_t>(esp_timer_get_time() / 1000); }

std::uint16_t Cap16(std::uint64_t value) { return static_cast<std::uint16_t>(value > 65535u ? 65535u : value); }

// ---------------------------------------------------------------------------------------------------
// Projects

std::string ProjectName(Variant variant)
{
  switch (variant)
  {
    case Variant::Dx7Chords: return std::string(kPrefix) + "DX7";
    case Variant::JunoPads: return std::string(kPrefix) + "JUNO";
    case Variant::DrumsFast: return std::string(kPrefix) + "FAST";
    case Variant::Arps: return std::string(kPrefix) + "ARPS";
    case Variant::Sampler: return std::string(kPrefix) + "SAMPLER";
    case Variant::FxMax: return std::string(kPrefix) + "FXMAX";
    case Variant::MaxPatterns: return std::string(kPrefix) + "MAX";
    case Variant::StreamFour: return std::string(kPrefix) + "STREAM4";
    case Variant::StreamSix: return std::string(kPrefix) + "STREAM6";
    case Variant::None: break;
  }
  return std::string(kPrefix) + "NONE";
}

bool StreamVariant(Variant variant) { return variant == Variant::StreamFour || variant == Variant::StreamSix; }

constexpr const char* kShortSample = "ZZ-ST-SHORT.wav";  // 1.5 s: read into PSRAM
constexpr const char* kLongSample = "ZZ-ST-LONG.wav";    // 20 s: plays from the card
// Four 24-second samples, one a track, each played once a pattern (a note at the start of 128 steps
// at 80 BPM, 24 s): the use that broke streaming (the third track crackled, the fourth never sounded).
constexpr std::array<const char*, 4> kLoopSamples{{"ZZ-ST-LOOP1.wav", "ZZ-ST-LOOP2.wav", "ZZ-ST-LOOP3.wav", "ZZ-ST-LOOP4.wav"}};
constexpr std::size_t kLoopFrames = 24 * 44100;

bool WriteSample(const char* name, std::size_t frames, float frequency)
{
  const std::string path = synth::storage::SampleDirectory() + "/" + name;
  if (std::FILE* file = std::fopen(path.c_str(), "rb"))
  {
    std::fclose(file);
    return true;
  }
  return synth::sampler::WriteMono16WavFrom(path, frames, 44100, [&](std::int16_t* chunk, std::size_t offset, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index)
    {
      const float time = static_cast<float>(offset + index) / 44100.0f;
      const float envelope = 1.0f - 0.7f * std::fmod(time, 2.0f) / 2.0f;
      const float value = 0.45f * std::sin(6.2831853f * frequency * time) + 0.2f * std::sin(6.2831853f * frequency * 2.01f * time);
      chunk[index] = static_cast<std::int16_t>(32767.0f * envelope * value);
    }
    return true;
  });
}

struct TrackSpec {
  SynthEngineKind kind = SynthEngineKind::Juno;
  const char* preset = "juno.a68";
  const char* sample = "";
  int notesPerStep = 4;     // a chord on the steps that play
  int stepEvery = 1;        // every step, every second one...
  int lengthPulses = 48;
  int baseNote = 48;
  bool drums = false;       // the eight drums of the kit, not a chord
  synth::ArpSettings arp{};
  int chorus = 1, drive = 0, filter = 64, resonance = 20, crush = 0, delay = 0, reverb = 0;
};

synth::ProjectData MakeProject(Variant variant)
{
  synth::ProjectData project;
  project.sequence.reset(new (memory::slow) SynthSequencer::ProjectState);
  std::array<TrackSpec, synth::kSynthTrackCount> tracks{};
  int patterns = 1;
  int steps = 16;
  std::uint16_t bpm = 120;
  auto& fx = project.masterFx;
  using MP = synth::fx::MasterParam;
  const auto master = [&](MP param, int value) { fx[static_cast<std::size_t>(param)] = static_cast<std::uint8_t>(value); };
  switch (variant)
  {
    case Variant::Dx7Chords:
    case Variant::FxMax:
    {
      const char* presets[] = {"dx7.rom1a.01", "dx7.rom1a.11", "dx7.rom1a.04", "dx7.rom1a.20"};
      for (std::size_t track = 0; track < tracks.size(); ++track)
      {
        tracks[track] = TrackSpec{SynthEngineKind::Dx7, presets[track], "", 6, 1, 72, 40 + 7 * static_cast<int>(track)};
        tracks[track].chorus = 2;
        tracks[track].drive = 40;
        tracks[track].reverb = 60;
        tracks[track].delay = 40;
      }
      bpm = 140;
      if (variant == Variant::FxMax)
      {
        for (auto& track : tracks)
        {
          track.drive = 127;
          track.resonance = 127;
          track.filter = 40;
          track.crush = 90;
          track.reverb = 127;
          track.delay = 127;
        }
        master(MP::DelayTime, 11);
        master(MP::DelayFeedback, 120);
        master(MP::DelayLevel, 127);
        master(MP::ReverbSize, 127);
        master(MP::ReverbLevel, 127);
        master(MP::Compressor, 127);
        master(MP::EqLow, 127);
        master(MP::EqHigh, 127);
        master(MP::Tape, 127);
      }
      break;
    }
    case Variant::JunoPads:
    {
      const char* presets[] = {"juno.a68", "juno.a11", "juno.b78", "juno.a51"};
      for (std::size_t track = 0; track < tracks.size(); ++track)
      {
        tracks[track] = TrackSpec{SynthEngineKind::Juno, presets[track], "", 8, 2, 96, 36 + 9 * static_cast<int>(track)};
        tracks[track].chorus = 2;
        tracks[track].reverb = 90;
        tracks[track].delay = 60;
      }
      master(MP::DelayTime, 11);
      master(MP::ReverbSize, 127);
      break;
    }
    case Variant::DrumsFast:
    {
      tracks[0] = TrackSpec{SynthEngineKind::Drums, "drums.808", "", 8, 1, 12, 36, true};
      tracks[1] = TrackSpec{SynthEngineKind::Dx7, "dx7.rom1a.15", "", 1, 1, 12, 36};
      tracks[2] = TrackSpec{SynthEngineKind::Juno, "juno.a68", "", 4, 1, 12, 60};
      tracks[3] = TrackSpec{SynthEngineKind::Drums, "drums.808", "", 8, 1, 12, 36, true};
      bpm = 300;
      break;
    }
    case Variant::Arps:
    {
      const SynthEngineKind kinds[] = {SynthEngineKind::Juno, SynthEngineKind::Dx7, SynthEngineKind::Juno, SynthEngineKind::Dx7};
      const char* presets[] = {"juno.a11", "dx7.rom1a.11", "juno.a68", "dx7.rom1a.01"};
      for (std::size_t track = 0; track < tracks.size(); ++track)
      {
        tracks[track] = TrackSpec{kinds[track], presets[track], "", 4, 16, 384, 48 + 5 * static_cast<int>(track)};
        tracks[track].arp.mode = synth::ArpMode::UpDown;
        tracks[track].arp.rate = 5;  // 1/32
        tracks[track].arp.octaves = 4;
        tracks[track].arp.gate = 90;
      }
      bpm = 160;
      break;
    }
    case Variant::Sampler:
    {
      tracks[0] = TrackSpec{SynthEngineKind::Drums, "drums.808", "", 3, 1, 24, 36, true};
      tracks[1] = TrackSpec{SynthEngineKind::Juno, "juno.a68", "", 4, 2, 96, 48};
      tracks[2] = TrackSpec{SynthEngineKind::Sampler, "sampler.empty", kShortSample, 4, 2, 96, 60};
      tracks[3] = TrackSpec{SynthEngineKind::Sampler, "sampler.empty", kLongSample, 4, 4, 192, 60};
      break;
    }
    case Variant::StreamFour:
    case Variant::StreamSix:
    {
      // One note at step 0 lasting the whole pattern; with six voices, tracks 1 and 2 play two notes a
      // minor third apart (two voices on one file at different speeds).
      for (std::size_t track = 0; track < tracks.size(); ++track)
      {
        const int notes = variant == Variant::StreamSix && track < 2 ? 2 : 1;
        tracks[track] = TrackSpec{SynthEngineKind::Sampler, "sampler.empty", kLoopSamples[track], notes, 128, 128 * synth::kSequencerPulsesPerStep, 60};
      }
      steps = 128;
      bpm = 80;
      break;
    }
    case Variant::MaxPatterns:
    {
      const SynthEngineKind kinds[] = {SynthEngineKind::Dx7, SynthEngineKind::Juno, SynthEngineKind::Dx7, SynthEngineKind::Juno};
      const char* presets[] = {"dx7.rom1a.01", "juno.a68", "dx7.rom1a.11", "juno.a11"};
      for (std::size_t track = 0; track < tracks.size(); ++track)
        tracks[track] = TrackSpec{kinds[track], presets[track], "", 8, 1, 12, 40 + 6 * static_cast<int>(track)};
      patterns = static_cast<int>(synth::kSequencerMaxPatternCount);
      steps = static_cast<int>(synth::kSequencerMaxSteps);
      break;
    }
    case Variant::None: break;
  }

  auto& sequence = *project.sequence;
  sequence.bpm = bpm;
  project.hasPlay = true;
  for (std::size_t track = 0; track < tracks.size(); ++track)
  {
    const TrackSpec& spec = tracks[track];
    project.kinds[track] = spec.kind;
    auto& engine = project.engines[track];
    engine.basePresetId = spec.preset;
    engine.presetId = spec.preset;
    engine.presetName = "STRESS";
    engine.values = {{"fx.chorus", spec.chorus}, {"fx.drive", spec.drive}, {"fx.filter", spec.filter}, {"fx.resonance", spec.resonance},
                     {"fx.crush", spec.crush},   {"fx.delay", spec.delay}, {"fx.reverb", spec.reverb}};
    project.samples[track] = spec.sample;
    project.play[track].arp = spec.arp;
    auto& data = sequence.tracks[track];
    data.patternCount = static_cast<std::size_t>(patterns);
    data.activePattern = 0;
    for (int pattern = 0; pattern < patterns; ++pattern)
    {
      data.patternSteps[static_cast<std::size_t>(pattern)] = static_cast<std::uint16_t>(steps);
      auto& store = data.patterns[static_cast<std::size_t>(pattern)].steps;
      for (int step = 0; step < steps; step += spec.stepEvery)
      {
        auto& notes = store.Edit(static_cast<std::size_t>(step)).notes;
        for (int index = 0; index < spec.notesPerStep && index < static_cast<int>(notes.size()); ++index)
        {
          auto& note = notes[static_cast<std::size_t>(index)];
          const int pitch = spec.drums ? kKitNotes[static_cast<std::size_t>(index)]
                                       : spec.baseNote + 3 * index + (step / 4 + pattern) % 5;
          note.note = static_cast<std::uint8_t>(std::clamp(pitch, 0, 127));
          note.velocity = static_cast<std::uint8_t>(90 + (step * 7 + index * 3) % 37);
          note.lengthPulses = static_cast<std::uint16_t>(spec.lengthPulses);
          note.microstep = static_cast<std::uint8_t>(spec.drums ? 0 : (index % 3));
          note.active = true;
        }
      }
    }
  }
  sequence.songLength = static_cast<std::size_t>(patterns);
  for (int row = 0; row < patterns; ++row)
  {
    for (auto& pattern : sequence.song[static_cast<std::size_t>(row)].patterns) pattern = static_cast<std::int8_t>(row);
    sequence.song[static_cast<std::size_t>(row)].repeats = 1;
  }
  return project;
}

bool WriteFixture(const std::string& name, const char* text)
{
  const std::string path = synth::storage::ProjectDirectory() + "/" + name + ".ccproj";
  std::ofstream file(path, std::ios::trunc);
  file << text;
  return file.good();
}

// ---------------------------------------------------------------------------------------------------
// The run

std::vector<Scenario> BuildScenarios()
{
  std::vector<Scenario> list;
  const auto add = [&](const char* name, Kind kind, Variant variant, std::uint32_t seconds, std::string project = "") {
    if (list.size() >= kMaxScenarios) return;
#if defined(CARDENZA_STRESS_ONLY)
    // `make DEVICE=Cardenza stress STRESS_ONLY=card:DEMO-2,sampler STRESS_SECONDS=60`: those alone.
    const std::string only = "," CARDENZA_STRESS_ONLY ",";
    if (only.find("," + std::string(name) + ",") == std::string::npos) return;
#if defined(CARDENZA_STRESS_SECONDS)
    seconds = CARDENZA_STRESS_SECONDS;
#endif
#endif
    Scenario scenario;
    scenario.name = name;
    scenario.kind = kind;
    scenario.variant = variant;
    scenario.seconds = seconds;
    scenario.project = project.empty() && variant != Variant::None ? ProjectName(variant) : project;
    list.push_back(scenario);
  };
  add("idle-new", Kind::Idle, Variant::None, 10);
  add("old-v1", Kind::Fixture, Variant::None, 8, std::string(kPrefix) + "OLD-V1");
  add("old-0.1.0", Kind::Fixture, Variant::None, 15, std::string(kPrefix) + "OLD-DEMO2");
  add("dx7-chords", Kind::Generated, Variant::Dx7Chords, 30);
  add("juno-pads", Kind::Generated, Variant::JunoPads, 30);
  add("drums-300bpm", Kind::Generated, Variant::DrumsFast, 25);
  add("arps-1/32", Kind::Generated, Variant::Arps, 25);
  add("sampler", Kind::Generated, Variant::Sampler, 25);
  add("fx-max", Kind::Generated, Variant::FxMax, 25);
  add("max-patterns", Kind::Generated, Variant::MaxPatterns, 25);
  add("stream-4", Kind::Generated, Variant::StreamFour, 60);
  add("stream-6", Kind::Generated, Variant::StreamSix, 60);
  add("live-ops", Kind::LiveOps, Variant::Dx7Chords, 45);
  add("ui-storm", Kind::UiStorm, Variant::JunoPads, 30);
  add("worst", Kind::Worst, Variant::FxMax, 40);
  for (const std::string& name : synth::ListProjects())
  {
    if (name.rfind(kPrefix, 0) == 0) continue;
    const std::string label = "card:" + name;
    add(label.c_str(), Kind::Card, Variant::None, 10, name);
  }
#if defined(CARDENZA_STRESS_LOOPS)
  // `STRESS_LOOPS=4`: the list that many times over in one run (`worst#2` ...), for what happens only
  // now and then.
  const std::size_t once = list.size();
  for (int loop = 2; loop <= CARDENZA_STRESS_LOOPS; ++loop)
    for (std::size_t index = 0; index < once && list.size() < kMaxScenarios; ++index)
    {
      Scenario again = list[index];
      again.name += "#" + std::to_string(loop);
      list.push_back(again);
    }
#endif
  return list;
}

struct Window {
  Device::Audio::RenderTiming audio;
  std::uint32_t faded = 0;
  std::uint32_t shed = 0;
  synth::audio::GivenUpVoices lost;
  std::uint32_t lastTickMs = 0;
  std::uint32_t uiMaxMs = 0;
  std::uint32_t uiDrops = 0;
  std::uint32_t seqLatest = 0;
  std::uint32_t seqLate = 0;
  std::uint32_t opMaxMs = 0;
  std::uint32_t opUnderruns = 0;
  synth::audio::StreamCounters stream;  // samples played from the card, from when the scenario plays
};

struct Runner {
  std::vector<Scenario> scenarios;
  bool started = false;
  bool active = false;      // a scenario is set up and being measured
  std::uint32_t endMs = 0;
  std::uint32_t nextOpMs = 0;
  std::uint32_t op = 0;
  std::uint32_t random = 0x12345678u;
  std::array<bool, 56> held{};
  Window window;
  std::uint32_t summaryMs = 0;
  std::size_t summaryLine = 0;
  TaskHandle_t watcher = nullptr;
  volatile std::uint32_t frames = 0;
};
Runner g_runner;

std::uint32_t Random()
{
  std::uint32_t x = g_runner.random;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return g_runner.random = x;
}

void CopyNote(Result& result, const std::string& text)
{
  std::snprintf(result.note, sizeof(result.note), "%s", text.c_str());
}

// A frozen UI: what every task is doing (the watchdog restarts the device after 5 s, and the next run
// prints its backtrace).
void WatchTask(void*)
{
  std::uint32_t last = 0;
  std::uint32_t still = 0;
  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
    const std::uint32_t now = g_runner.frames;
    still = now == last ? still + 1 : 0;
    last = now;
    if (still != 3) continue;
    std::printf("stress: UI stopped for 3 s, internal %u psram %u\n", static_cast<unsigned>(memory::FastFree()), static_cast<unsigned>(memory::SlowFree()));
    static TaskStatus_t tasks[32];
    const UBaseType_t count = uxTaskGetSystemState(tasks, 32, nullptr);
    for (UBaseType_t index = 0; index < count; ++index)
    {
      std::printf("stress: task %-14s state=%d prio=%u stack_left=%u\n", tasks[index].pcTaskName, static_cast<int>(tasks[index].eCurrentState),
                  static_cast<unsigned>(tasks[index].uxCurrentPriority), static_cast<unsigned>(tasks[index].usStackHighWaterMark));
      vTaskDelay(pdMS_TO_TICKS(30));
    }
  }
}

// Each task's share of the time since the last call (FreeRTOS run-time stats, on in stress builds):
// which task keeps a core busy, and whether its idle task ever runs (the task watchdog checks both).
void PrintTaskLoad(const char* label)
{
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
  static TaskStatus_t tasks[32];
  static TaskHandle_t lastHandles[32];
  static configRUN_TIME_COUNTER_TYPE lastCounters[32];
  static UBaseType_t lastCount = 0;
  static configRUN_TIME_COUNTER_TYPE lastTotal = 0;
  configRUN_TIME_COUNTER_TYPE total = 0;
  const UBaseType_t count = uxTaskGetSystemState(tasks, 32, &total);
  const configRUN_TIME_COUNTER_TYPE elapsed = total - lastTotal;
  char line[320];
  int length = std::snprintf(line, sizeof(line), "stress: load %s:", label);
  for (UBaseType_t index = 0; index < count && elapsed != 0; ++index)
  {
    configRUN_TIME_COUNTER_TYPE before = 0;
    for (UBaseType_t old = 0; old < lastCount; ++old)
      if (lastHandles[old] == tasks[index].xHandle) before = lastCounters[old];
    const unsigned percent = static_cast<unsigned>((tasks[index].ulRunTimeCounter - before) * 100u / elapsed);
    if (percent > 0 && length < static_cast<int>(sizeof(line)) - 24)
      length += std::snprintf(line + length, sizeof(line) - static_cast<std::size_t>(length), " %s %u%%", tasks[index].pcTaskName, percent);
  }
  std::printf("%s\n", line);
  for (UBaseType_t index = 0; index < count; ++index)
  {
    lastHandles[index] = tasks[index].xHandle;
    lastCounters[index] = tasks[index].ulRunTimeCounter;
  }
  lastCount = count;
  lastTotal = total;
#else
  (void)label;
#endif
}

bool Load(GrooveApp& app, const std::string& name, std::string& message)
{
  groove::ProjectUiSettings settings;
  if (!app.LoadProject(name, settings, message)) return false;
  app.Ui().Restore(settings, name);
  return true;
}

// Runs `work` as one operation: its time, and the underruns while it ran.
template <typename Work>
void Operation(Work&& work)
{
  const std::uint32_t underruns = Device::Audio::Timing().underruns;
  const std::uint32_t start = NowMs();
  work();
  const std::uint32_t took = NowMs() - start;
  g_runner.window.opMaxMs = (std::max)(g_runner.window.opMaxMs, took);
  g_runner.window.opUnderruns += Device::Audio::Timing().underruns - underruns;
  g_runner.window.lastTickMs = NowMs();  // the operation's own time is not a dropped frame
}

void StartWindow()
{
  PrintTaskLoad("before");  // and the start of this window's
  Window& window = g_runner.window;
  window = Window{};
  window.audio = Device::Audio::Timing();
  window.faded = synth::audio::FadedVoiceCount();
  window.shed = synth::audio::ShedVoiceCount();
  window.lost = synth::audio::GivenUpVoiceCounts();
  window.lastTickMs = NowMs();
  heap_caps_monitor_local_minimum_free_size_start();
}

void PrintResult(std::size_t index)
{
  const Result& r = g_persist.results[index];
  static const char* const kStates[] = {"not run", "running", "done", "RESTARTED"};
  std::printf("stress: %2u %-18s %-9s %s render %u/%u us under %u | notes lost: held %u unheard %u stolen %u (tails %u) | ui max %u ms drops %u | "
              "seq late %u us n %u | ram min %u KB psram %u KB | op %u ms under %u | voice us dx7 %u juno %u drums %u, other %u, scale %u%% | %s\n",
              static_cast<unsigned>(index), index < g_runner.scenarios.size() ? g_runner.scenarios[index].name.c_str() : "?", kStates[r.state & 3],
              r.state == 2 ? (r.ok ? "ok  " : "FAIL") : "    ", r.avgRenderUs, r.maxRenderUs, r.underruns, r.lostHeld, r.lostUnheard, r.lostStolen,
              r.lostTails, r.uiMaxMs, r.uiDrops, r.seqLatestUs, r.seqLate, r.ramMinKb, r.psramMinKb, r.opMaxMs, r.opUnderruns, r.dx7Us, r.junoUs,
              r.drumsUs, r.overheadUs, r.scalePercent, r.note);
}

// One short line a scenario for the summary repeated at the end: the USB console drops long bursts.
// `make DEVICE=Cardenza stress STRESS_TRACE=worst`: while that scenario plays, ESP-IDF's heap trace
// keeps every allocation not given back yet (records and their map in PSRAM, eight callers each).
// When internal RAM first drops below 10 KB, and at the scenario's end, the internal blocks still
// held are summed by their nearest callers; that report and the internal RAM left each second come
// back with the summary (`stress: H`, `stress: M` lines; decode the addresses with addr2line).
#if CARDENZA_STRESS_HEAP_TRACE
constexpr std::size_t kTraceRecords = 3000;
constexpr std::size_t kTraceLineBytes = 168;
// The report survives a restart (the device has restarted after a run that got low): RTC memory,
// beside the log ring, which is smaller in stress builds for it.
struct TraceKeep {
  std::uint32_t magic;
  std::uint32_t build;
  std::uint32_t length;
  char text[1500];
};
static_assert(std::is_trivially_default_constructible_v<TraceKeep>, "a constructor would clear it at every start");
constexpr std::uint32_t kTraceKeepMagic = 0x54524331u;  // "TRC1"
RTC_NOINIT_ATTR TraceKeep g_traceKeep;
constexpr std::size_t kTraceGroups = 64;
constexpr std::uint32_t kTraceLowBytes = 10 * 1024;
struct TraceGroup {
  std::uint32_t key;
  std::uint32_t bytes;
  std::uint32_t count;
  void* frames[CONFIG_HEAP_TRACING_STACK_DEPTH];
};
struct Trace {
  heap_trace_record_t* records = nullptr;
  TraceGroup* groups = nullptr;
  bool active = false;
  bool lowTaken = false;
  bool done = false;  // a run got low: its report is kept and the test ends there
  std::uint32_t startMs = 0;
  std::uint32_t second = 0;
  std::uint32_t secondMin = 0xFFFFFFFFu;
  std::uint32_t secondLargest = 0;
  std::array<std::uint16_t, 128> minTenths{};  // internal RAM left, the lowest each second, in 100 bytes
  std::array<std::uint8_t, 128> largestKb{};   // the largest internal block then
  std::size_t seconds = 0;
};
Trace g_trace;

bool TraceKept() { return g_traceKeep.magic == kTraceKeepMagic && g_traceKeep.build == g_persist.build && g_traceKeep.length < sizeof(g_traceKeep.text); }

void TraceClear()
{
  g_traceKeep.magic = kTraceKeepMagic;
  g_traceKeep.build = g_persist.build;
  g_traceKeep.length = 0;
}

// One line of the report: printed now and kept.
void TraceLine(const char* format, ...) __attribute__((format(printf, 1, 2)));
void TraceLine(const char* format, ...)
{
  char line[kTraceLineBytes];
  va_list args;
  va_start(args, format);
  const int length = std::vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  std::printf("%s\n", line);
  if (!TraceKept() || length <= 0) return;
  const std::size_t size = (std::min<std::size_t>)(static_cast<std::size_t>(length), sizeof(line) - 1);
  if (g_traceKeep.length + size + 1 >= sizeof(g_traceKeep.text)) return;
  std::memcpy(g_traceKeep.text + g_traceKeep.length, line, size);
  g_traceKeep.length += static_cast<std::uint32_t>(size);
  g_traceKeep.text[g_traceKeep.length++] = '\n';
}

void TraceSummarize(const char* when)
{
  (void)heap_trace_stop();
  std::size_t used = 0;
  std::uint32_t internalBytes = 0, internalCount = 0, slowBytes = 0, otherBytes = 0;
  heap_trace_record_t record;
  const std::size_t count = heap_trace_get_count();
  for (std::size_t index = 0; index < count; ++index)
  {
    if (heap_trace_get(index, &record) != ESP_OK || record.address == nullptr) continue;
    if (!esp_ptr_internal(record.address))
    {
      slowBytes += record.size;
      continue;
    }
    internalBytes += record.size;
    ++internalCount;
    std::uint32_t key = 0;
    for (int frame = 1; frame < 5 && frame < CONFIG_HEAP_TRACING_STACK_DEPTH; ++frame)
      key = key * 2654435761u + reinterpret_cast<std::uintptr_t>(record.alloced_by[frame]);
    std::size_t group = 0;
    while (group < used && g_trace.groups[group].key != key) ++group;
    if (group == used)
    {
      if (used == kTraceGroups)
      {
        otherBytes += record.size;
        continue;
      }
      TraceGroup& fresh = g_trace.groups[used++];
      fresh.key = key;
      fresh.bytes = 0;
      fresh.count = 0;
      std::memcpy(fresh.frames, record.alloced_by, sizeof(fresh.frames));
    }
    g_trace.groups[group].bytes += record.size;
    ++g_trace.groups[group].count;
  }
  std::sort(g_trace.groups, g_trace.groups + used, [](const TraceGroup& a, const TraceGroup& b) { return a.bytes > b.bytes; });
  TraceLine("stress: H %s at %u s: internal %u B held in %u blocks (%u B past the groups), PSRAM %u B; records %u of %u; free %u largest %u; failed %u",
            when, static_cast<unsigned>((NowMs() - g_trace.startMs) / 1000), static_cast<unsigned>(internalBytes), static_cast<unsigned>(internalCount),
            static_cast<unsigned>(otherBytes), static_cast<unsigned>(slowBytes), static_cast<unsigned>(count), static_cast<unsigned>(kTraceRecords),
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)), cardenza_failed_allocations());
  for (std::size_t group = 0; group < used && group < 8; ++group)
  {
    const TraceGroup& g = g_trace.groups[group];
    char frames[96];
    int length = 0;
    // Frames 0 and 1 are heap_caps_malloc and operator new / malloc.
    for (int frame = 2; frame < CONFIG_HEAP_TRACING_STACK_DEPTH && length < static_cast<int>(sizeof(frames)) - 12; ++frame)
      length += std::snprintf(frames + length, sizeof(frames) - static_cast<std::size_t>(length), " %08x",
                              static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(g.frames[frame])));
    TraceLine("stress: H %s %u B x%u:%s", when, static_cast<unsigned>(g.bytes), static_cast<unsigned>(g.count), frames);
  }
  (void)heap_trace_resume();
}

void TraceStart()
{
  if (g_trace.records == nullptr)
  {
    g_trace.records = static_cast<heap_trace_record_t*>(heap_caps_calloc(kTraceRecords, sizeof(heap_trace_record_t), MALLOC_CAP_SPIRAM));
    g_trace.groups = static_cast<TraceGroup*>(heap_caps_calloc(kTraceGroups, sizeof(TraceGroup), MALLOC_CAP_SPIRAM));
    if (g_trace.records == nullptr || g_trace.groups == nullptr ||
        heap_trace_init_standalone(g_trace.records, kTraceRecords) != ESP_OK)
    {
      std::printf("stress: heap trace not started (no PSRAM for it)\n");
      return;
    }
  }
  TraceClear();
  g_trace.lowTaken = false;
  g_trace.seconds = 0;
  g_trace.second = 0;
  g_trace.secondMin = 0xFFFFFFFFu;
  g_trace.startMs = NowMs();
  g_trace.active = heap_trace_start(HEAP_TRACE_LEAKS) == ESP_OK;
  std::printf("stress: heap trace %s\n", g_trace.active ? "on" : "failed to start");
}

// Each frame of the traced scenario: the internal RAM left, and the report at the first low.
void TraceTick()
{
  if (!g_trace.active) return;
  const std::uint32_t free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (free < g_trace.secondMin)
  {
    g_trace.secondMin = free;
    g_trace.secondLargest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  const std::uint32_t second = (NowMs() - g_trace.startMs) / 1000;
  if (second != g_trace.second)
  {
    // A frame may span seconds (an engine switch holds the UI): those seconds share its reading.
    while (g_trace.seconds < second && g_trace.seconds < g_trace.minTenths.size())
    {
      g_trace.minTenths[g_trace.seconds] = static_cast<std::uint16_t>((std::min<std::uint32_t>)(g_trace.secondMin / 100, 65535u));
      g_trace.largestKb[g_trace.seconds] = static_cast<std::uint8_t>((std::min<std::uint32_t>)(g_trace.secondLargest / 1024, 255u));
      ++g_trace.seconds;
    }
    g_trace.second = second;
    g_trace.secondMin = 0xFFFFFFFFu;
  }
  if (!g_trace.lowTaken && free < kTraceLowBytes)
  {
    g_trace.lowTaken = true;
    TraceSummarize("low");
  }
}

void TraceFinish()
{
  if (!g_trace.active) return;
  if (!g_trace.lowTaken) TraceClear();  // nothing to see: the next run's report from scratch
  TraceSummarize("end");
  (void)heap_trace_stop();
  g_trace.active = false;
  // Internal RAM left each second (KB, the lowest that second / the largest block then).
  for (std::size_t first = 0; first < g_trace.seconds; first += 10)
  {
    char text[kTraceLineBytes];
    int length = std::snprintf(text, sizeof(text), "stress: M %u:", static_cast<unsigned>(first));
    for (std::size_t index = first; index < g_trace.seconds && index < first + 10; ++index)
      length += std::snprintf(text + length, sizeof(text) - static_cast<std::size_t>(length), " %u.%u/%u", g_trace.minTenths[index] / 10u,
                              g_trace.minTenths[index] % 10u, static_cast<unsigned>(g_trace.largestKb[index]));
    TraceLine("%s", text);
  }
  g_trace.done = g_trace.lowTaken;
}

bool Traced(std::size_t index)
{
  if (g_trace.done || index >= g_runner.scenarios.size()) return false;
  const std::string& name = g_runner.scenarios[index].name;
  const std::string traced = CARDENZA_STRESS_TRACE;
  return name == traced || name.rfind(traced + "#", 0) == 0;
}
#endif

void PrintShortResult(std::size_t index)
{
  const Result& r = g_persist.results[index];
  static const char* const kStates[] = {"-", "running", "done", "RESTARTED"};
  std::printf("stress: S %2u %-14s %s %s avg %u max %u under %u held %u unheard %u stolen %u ui %u late %u ram %u dx7 %u juno %u\n",
              static_cast<unsigned>(index), index < g_runner.scenarios.size() ? g_runner.scenarios[index].name.c_str() : "?", kStates[r.state & 3],
              r.state == 2 ? (r.ok ? "ok" : "FAIL") : "-", r.avgRenderUs, r.maxRenderUs, r.underruns, r.lostHeld, r.lostUnheard, r.lostStolen,
              r.uiMaxMs, r.seqLatestUs, r.ramMinKb, r.dx7Us, r.junoUs);
}

void FinishWindow(GrooveApp& app, std::size_t index)
{
  Result& result = g_persist.results[index];
  const Window& window = g_runner.window;
  const Device::Audio::RenderTiming audio = Device::Audio::Timing();
  const std::uint32_t blocks = audio.blocks - window.audio.blocks;
  result.avgRenderUs = static_cast<std::uint16_t>(blocks != 0 ? (audio.renderMicros - window.audio.renderMicros) / blocks : 0);
  result.maxRenderUs = Cap16(audio.longestRenderMicros);
  result.overruns = Cap16(audio.overruns - window.audio.overruns);
  result.underruns = Cap16(audio.underruns - window.audio.underruns);
  result.faded = Cap16(synth::audio::FadedVoiceCount() - window.faded);
  result.shed = Cap16(synth::audio::ShedVoiceCount() - window.shed);
  const synth::audio::GivenUpVoices lost = synth::audio::GivenUpVoiceCounts();
  result.lostTails = Cap16(lost.tails - window.lost.tails);
  result.lostHeld = Cap16(lost.held - window.lost.held);
  result.lostUnheard = Cap16(lost.unheard - window.lost.unheard);
  result.lostStolen = Cap16(lost.stolen - window.lost.stolen);
  const synth::audio::RenderModel model = synth::audio::CurrentRenderModel();
  result.dx7Us = Cap16(static_cast<std::uint32_t>(model.dx7Micros));
  result.junoUs = Cap16(static_cast<std::uint32_t>(model.junoMicros));
  result.drumsUs = Cap16(static_cast<std::uint32_t>(model.drumsMicros));
  result.overheadUs = Cap16(static_cast<std::uint32_t>(model.overheadMicros));
  result.scalePercent = Cap16(static_cast<std::uint32_t>(model.budgetScale * 100.0f));
  result.uiMaxMs = Cap16(window.uiMaxMs);
  result.uiDrops = Cap16(window.uiDrops);
  result.seqLatestUs = Cap16(window.seqLatest);
  result.seqLate = Cap16(window.seqLate);
  result.ramMinKb = static_cast<std::uint16_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024);
  result.psramMinKb = static_cast<std::uint16_t>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024);
  heap_caps_monitor_local_minimum_free_size_stop();
  result.opMaxMs = Cap16(window.opMaxMs);
  result.opUnderruns = Cap16(window.opUnderruns);
  // Samples from the card: blocks read, voices that reached a block before the card had it (each a
  // gap: a click), files reopened (more files than file slots), the slowest read, the cache.
  const synth::audio::StreamCounters stream = synth::audio::SampleStreamCounters();
  const std::uint32_t streamReads = stream.reads - window.stream.reads;
  const std::uint32_t streamLate = stream.lateBlocks - window.stream.lateBlocks;
  if (streamReads != 0 || streamLate != 0)
    std::printf("stress: stream %s reads %u late %u opens %u failures %u slowest %u us cache %u KB\n", g_runner.scenarios[index].name.c_str(),
                static_cast<unsigned>(streamReads), static_cast<unsigned>(streamLate), static_cast<unsigned>(stream.opens - window.stream.opens),
                static_cast<unsigned>(stream.failures - window.stream.failures), static_cast<unsigned>(stream.slowestReadMicros),
                static_cast<unsigned>(stream.cacheBlocks * 8));
  if (StreamVariant(g_runner.scenarios[index].variant) && streamLate != 0 && result.note[0] == 0)
    CopyNote(result, "stream late " + std::to_string(streamLate));
  // Passing: nothing a user hears or sees go wrong while it plays.
  // Let-go tails fading early are polyphony giving way, which is allowed; a note that was played and
  // not heard is not (stolen voices are the tracks' voice count, reported apart).
  result.ok = result.underruns == 0 && result.seqLate == 0 && result.uiMaxMs < 250 && result.ramMinKb >= 12 && result.lostHeld == 0 &&
                      result.lostUnheard == 0 && result.note[0] == 0
                  ? 1
                  : 0;
  result.state = 2;
  PrintResult(index);
  PrintTaskLoad(g_runner.scenarios[index].name.c_str());
}

void ReleaseKeys(GrooveApp& app)
{
  for (std::size_t key = 0; key < g_runner.held.size(); ++key)
    if (g_runner.held[key])
    {
      app.HandleMatrixKeyEvent(key / 14, key % 14, false);
      g_runner.held[key] = false;
    }
}

// A storm of keys: pads held and let go in clusters, the GRID tapped, scenes, tracks and pages changed.
// Never ENTER, ADD, SUB, FN or PLAY: nothing that confirms, edits values or opens menus.
void KeyStorm(GrooveApp& app, std::uint32_t now)
{
  const std::uint32_t roll = Random();
  const std::size_t row = roll % 4;
  const std::size_t column = 1 + (roll >> 4) % 12;  // pads 1-8, GRID 9-12
  const std::size_t key = row * 14 + column;
  if (g_runner.held[key] || (roll >> 12) % 3 == 0)
  {
    if (g_runner.held[key]) app.HandleMatrixKeyEvent(row, column, false);
    g_runner.held[key] = false;
  }
  else if (std::count(g_runner.held.begin(), g_runner.held.end(), true) < 10)
  {
    app.HandleMatrixKeyEvent(row, column, true);
    g_runner.held[key] = true;
  }
  if (now % 700 < 17)
  {
    const std::size_t edge = (now / 700) % 3 == 0 ? 0 : ((now / 700) % 3 == 1 ? 1 : 3);  // ESC, TAB, CTRL
    ReleaseKeys(app);
    app.HandleMatrixKeyEvent(edge, 0, true);
    app.HandleMatrixKeyEvent(edge, 0, false);
  }
}

// One live operation while the project plays, in turn: what a user does in the middle of a song.
void LiveOperation(GrooveApp& app)
{
  std::string message;
  const SynthEngineKind cycle[] = {SynthEngineKind::Juno, SynthEngineKind::Sampler, SynthEngineKind::Drums, SynthEngineKind::Dx7};
  switch (g_runner.op++ % 10)
  {
    case 0: case 1: case 2: case 3:
      Operation([&] { (void)app.SwitchEngine(3, cycle[(g_runner.op - 1) % 4], message); });
      break;
    case 4:
      Operation([&] {
        if (synth::ISynthEngine* engine = app.Engine(2))
        {
          char id[24];
          std::snprintf(id, sizeof(id), "dx7.rom1a.%02u", static_cast<unsigned>(1 + Random() % 32));
          (void)engine->LoadPreset(id);
        }
      });
      break;
    case 5:
      Operation([&] {
        if (Load(app, ProjectName(Variant::JunoPads), message)) app.Sequencer().Play();
      });
      break;
    case 6:
      Operation([&] {
        groove::ProjectUiSettings settings;
        if (app.NewProject(settings, message)) app.Ui().Restore(settings, "");
        if (Load(app, ProjectName(Variant::Dx7Chords), message)) app.Sequencer().Play();
      });
      break;
    case 7:
      Operation([&] { (void)app.SaveProject(std::string(kPrefix) + "SAVED", true, app.Ui().Settings(), message); });
      break;
    case 8:
      Operation([&] { app.FlushSession(); });
      break;
    default:
      Operation([&] {
        for (std::size_t param = 0; param < synth::fx::kMasterParamCount; ++param)
          app.SetMasterFx(param, static_cast<int>(Random() % (static_cast<std::uint32_t>(synth::fx::kMasterParams[param].maxValue) + 1)));
        for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track) app.SetTrackLevel(track, static_cast<std::uint8_t>(40 + Random() % 61));
      });
      break;
  }
}

bool SetUp(GrooveApp& app, const Scenario& scenario, Result& result)
{
  std::string message;
  app.Sequencer().Stop();
  switch (scenario.kind)
  {
    case Kind::Idle:
    {
      groove::ProjectUiSettings settings;
      if (!app.NewProject(settings, message))
      {
        CopyNote(result, "new: " + message);
        return false;
      }
      app.Ui().Restore(settings, "");
      return true;
    }
    case Kind::Fixture:
    case Kind::Card:
    {
      if (scenario.kind == Kind::Fixture)
        (void)WriteFixture(scenario.project, scenario.name == "old-v1" ? kStressFixtureV1 : kStressFixtureRelease);
      bool loaded = false;
      Operation([&] { loaded = Load(app, scenario.project, message); });
      if (!loaded)
      {
        // Refusing an old file cleanly is a pass for stability, but worth reading.
        std::printf("stress: %s did not load: %s\n", scenario.project.c_str(), message.c_str());
        return true;
      }
      app.Sequencer().Play();
      return true;
    }
    case Kind::Generated:
    case Kind::LiveOps:
    case Kind::UiStorm:
    case Kind::Worst:
    {
      if (scenario.variant == Variant::Sampler || scenario.kind == Kind::Worst)
      {
        if (!WriteSample(kShortSample, 66150, 220.0f) || !WriteSample(kLongSample, 20 * 44100, 110.0f))
        {
          CopyNote(result, "sample write failed");
          return false;
        }
      }
      if (StreamVariant(scenario.variant))
      {
        const float frequencies[] = {110.0f, 146.8f, 196.0f, 261.6f};
        for (std::size_t index = 0; index < kLoopSamples.size(); ++index)
          if (!WriteSample(kLoopSamples[index], kLoopFrames, frequencies[index]))
          {
            CopyNote(result, "sample write failed");
            return false;
          }
      }
      synth::ProjectData project = MakeProject(scenario.variant);
      if (scenario.kind == Kind::Worst)
      {
        // Track 4 as the sampler scenario has it: the sample streamed from the card.
        const synth::ProjectData sampler = MakeProject(Variant::Sampler);
        project.kinds[3] = sampler.kinds[3];
        project.engines[3] = sampler.engines[3];
        project.samples[3] = sampler.samples[3];
        project.play[1].arp.mode = synth::ArpMode::Up;
        project.play[1].arp.rate = 5;
        project.play[1].arp.octaves = 4;
      }
      const std::uint32_t start = NowMs();
      const synth::ProjectResult saved = synth::SaveProjectFile(scenario.project, project, true);
      std::printf("stress: %s written: %d in %u ms\n", scenario.project.c_str(), static_cast<int>(saved), static_cast<unsigned>(NowMs() - start));
      if (saved != synth::ProjectResult::Ok)
      {
        CopyNote(result, "save " + std::to_string(static_cast<int>(saved)));
        return false;
      }
      if (scenario.kind == Kind::LiveOps)
      {
        synth::ProjectData pads = MakeProject(Variant::JunoPads);
        (void)synth::SaveProjectFile(ProjectName(Variant::JunoPads), pads, true);
      }
      bool loaded = false;
      Operation([&] { loaded = Load(app, scenario.project, message); });
      if (!loaded)
      {
        CopyNote(result, "load: " + message);
        return false;
      }
      // The samples' starts are on their way as the project loads; a user presses PLAY later than this.
      if (StreamVariant(scenario.variant)) vTaskDelay(pdMS_TO_TICKS(500));
      app.Sequencer().Play();
      return true;
    }
  }
  return false;
}

void Summary(std::uint32_t now)
{
  if (now < g_runner.summaryMs) return;
  if (g_runner.summaryLine == 0) std::printf("stress: ===== summary (build %08x, %u restarts) =====\n", static_cast<unsigned>(g_persist.build), static_cast<unsigned>(g_persist.reboots));
  if (g_runner.summaryLine < g_runner.scenarios.size())
  {
    PrintShortResult(g_runner.summaryLine++);
    g_runner.summaryMs = now + 40;
    return;
  }
#if CARDENZA_STRESS_HEAP_TRACE
  if (TraceKept())
  {
    // The kept report, a line a tick.
    std::size_t start = 0, line = g_runner.summaryLine - g_runner.scenarios.size();
    while (line > 0 && start < g_traceKeep.length)
    {
      if (g_traceKeep.text[start++] == '\n') --line;
    }
    if (start < g_traceKeep.length)
    {
      std::size_t end = start;
      while (end < g_traceKeep.length && g_traceKeep.text[end] != '\n') ++end;
      std::printf("%.*s\n", static_cast<int>(end - start), g_traceKeep.text + start);
      ++g_runner.summaryLine;
      g_runner.summaryMs = now + 40;
      return;
    }
  }
#endif
  g_runner.summaryLine = 0;
  g_runner.summaryMs = now + 20000;
}

}  // namespace

void StressTestTick(GrooveApp& app)
{
  Runner& runner = g_runner;
  ++runner.frames;
  // The USB console starts after the boot banner: how this start came about, where it can be read.
  static bool bootReported = false;
  if (!bootReported && esp_timer_get_time() > 3000000)
  {
    bootReported = true;
    const char* crash = cardenza_previous_crash();
    std::printf("stress: this start: reset reason %d, %u restarts in this run; the previous crash: %s\n", static_cast<int>(esp_reset_reason()),
                static_cast<unsigned>(g_persist.magic == kMagic ? g_persist.reboots : 0u), crash != nullptr && crash[0] != 0 ? crash : "(none recorded)\n");
#if CARDENZA_STRESS_HEAP_TRACE
    if (g_persist.magic == kMagic && g_traceKeep.magic == kTraceKeepMagic && g_traceKeep.build == g_persist.build && g_traceKeep.length != 0)
    {
      char first[112] = {};
      if (crash != nullptr)
        for (std::size_t index = 0; index + 1 < sizeof(first) && crash[index] != 0; ++index) first[index] = crash[index] == '\n' ? ' ' : crash[index];
      TraceLine("stress: H restarted after it: reset reason %d; %s", static_cast<int>(esp_reset_reason()), first[0] != 0 ? first : "no crash recorded");
    }
#endif
  }
  // The cores' load while the device starts (where the task watchdog has struck), then per scenario.
  static std::int64_t nextLoadUs = 2000000;
  if (!runner.started && esp_timer_get_time() > nextLoadUs && nextLoadUs <= 16000000)
  {
    char label[16];
    std::snprintf(label, sizeof(label), "at %u s", static_cast<unsigned>(nextLoadUs / 1000000));
    PrintTaskLoad(label);
    nextLoadUs += 2000000;
  }
  if (esp_timer_get_time() < kStartAfterUs) return;
  const std::uint32_t now = NowMs();
  if (!runner.started)
  {
    runner.started = true;
    runner.scenarios = BuildScenarios();
    const std::uint32_t build = BuildId();
    if (g_persist.magic != kMagic || g_persist.build != build)
    {
      std::memset(&g_persist, 0, sizeof(g_persist));
      g_persist.magic = kMagic;
      g_persist.build = build;
    }
    else if (g_persist.next < kMaxScenarios && g_persist.results[g_persist.next].state == 1)
    {
      // The device restarted during this one: a crash or the watchdog.
      g_persist.results[g_persist.next].state = 3;
      ++g_persist.reboots;
      const char* crash = cardenza_previous_crash();
      std::printf("stress: restarted during %u; the crash: %s\n", static_cast<unsigned>(g_persist.next), crash != nullptr && crash[0] != 0 ? crash : "(none recorded)\n");
#if !defined(CARDENZA_STRESS_REPEAT)
      ++g_persist.next;
#endif
    }
    xTaskCreatePinnedToCore(WatchTask, "stress_watch", 4096, nullptr, 22, &runner.watcher, 1);
    std::printf("stress: begin, build %08x, %u scenarios, at %u\n", static_cast<unsigned>(build), static_cast<unsigned>(runner.scenarios.size()),
                static_cast<unsigned>(g_persist.next));
    synth::storage::EnsureLayout();
  }
  const std::size_t index = g_persist.next;
  if (index >= runner.scenarios.size())
  {
    if (app.Sequencer().Playing()) app.Sequencer().Stop();
    Summary(now);
    return;
  }
  const Scenario& scenario = runner.scenarios[index];
  Result& result = g_persist.results[index];
  if (!runner.active)
  {
    std::printf("stress: --- %u %s (%u s)\n", static_cast<unsigned>(index), scenario.name.c_str(), static_cast<unsigned>(scenario.seconds));
    result = Result{};
    result.state = 1;  // a restart from here on is this scenario's
    StartWindow();
    if (!SetUp(app, scenario, result))
    {
      FinishWindow(app, index);
      ++g_persist.next;
      return;
    }
    (void)app.Sequencer().TakeClockLateness();
    runner.window.stream = synth::audio::SampleStreamCounters(true);
    runner.active = true;
#if CARDENZA_STRESS_HEAP_TRACE
    if (Traced(index)) TraceStart();
#endif
    runner.endMs = NowMs() + scenario.seconds * 1000;
    runner.nextOpMs = NowMs() + 1500;
    runner.op = 0;
    runner.window.lastTickMs = NowMs();
    return;
  }

#if CARDENZA_STRESS_HEAP_TRACE
  TraceTick();
#endif
  // Measuring: the frame time first (what came before this tick).
  const std::uint32_t frame = now - runner.window.lastTickMs;
  runner.window.uiMaxMs = (std::max)(runner.window.uiMaxMs, frame);
  if (frame > 34) ++runner.window.uiDrops;
  runner.window.lastTickMs = now;
  // Late clock pulses as they come, to be read next to what the log says happened then.
  const auto clock = app.Sequencer().TakeClockLateness();
  runner.window.seqLatest = (std::max)(runner.window.seqLatest, clock.latestMicros);
  runner.window.seqLate += clock.latePulses;
  if (clock.latePulses != 0)
    std::printf("stress: clock %u pulses late, the latest by %u us\n", static_cast<unsigned>(clock.latePulses), static_cast<unsigned>(clock.latestMicros));

  if (scenario.kind == Kind::LiveOps && now >= runner.nextOpMs)
  {
    LiveOperation(app);
    runner.nextOpMs = NowMs() + 1500;
  }
  if (scenario.kind == Kind::UiStorm || scenario.kind == Kind::Worst) KeyStorm(app, now);
  if (scenario.kind == Kind::Worst && now >= runner.nextOpMs)
  {
    // A lighter set of operations on top: engine switches and autosaves.
    std::string message;
    if ((runner.op++ & 1) == 0) Operation([&] { (void)app.SwitchEngine(2, (runner.op & 2) ? SynthEngineKind::Juno : SynthEngineKind::Dx7, message); });
    else Operation([&] { app.FlushSession(); });
    runner.nextOpMs = NowMs() + 3000;
  }
  if (scenario.variant == Variant::MaxPatterns && now >= runner.nextOpMs)
  {
    for (std::size_t track = 0; track < synth::kSynthTrackCount; ++track)
      (void)app.Sequencer().QueuePattern(track, Random() % synth::kSequencerMaxPatternCount);
    runner.nextOpMs = now + 2000;
  }
  if (now < runner.endMs) return;

  ReleaseKeys(app);
#if CARDENZA_STRESS_HEAP_TRACE
  if (Traced(index)) TraceFinish();
#endif
  FinishWindow(app, index);
  runner.active = false;
  ++g_persist.next;
#if CARDENZA_STRESS_HEAP_TRACE
  if (g_trace.done) g_persist.next = static_cast<std::uint32_t>(runner.scenarios.size());
#endif
}

}  // namespace apps

#else

namespace apps {
void StressTestTick(GrooveApp&) {}
}  // namespace apps

#endif

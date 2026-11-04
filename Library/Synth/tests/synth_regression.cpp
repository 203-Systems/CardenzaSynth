#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <numeric>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#include "Devices/DeviceAudio.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/Engines/Juno/JunoSynthEngine.h"
#include "Library/Synth/Core/SynthStorage.h"
// Include the runtime once to check both rendered PCM and mapping state against AMY.
#include "Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp"

namespace amy_embed { extern "C" {
extern std::uint16_t* voice_to_base_osc;
extern std::uint8_t* osc_to_voice;
} }

namespace { std::filesystem::path storageRoot; }

// Lets the effects tests run the render thread out of memory: while set, nothrow allocations of
// at least this many bytes fail (4096: delay and reverb memory; 1024: track buses and choruses too).
namespace { std::size_t failAllocationsFrom = 0; }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
  if (failAllocationsFrom != 0 && size >= failAllocationsFrom) return nullptr;
  try { return ::operator new(size); } catch (...) { return nullptr; }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
  if (failAllocationsFrom != 0 && size >= failAllocationsFrom) return nullptr;
  try { return ::operator new[](size); } catch (...) { return nullptr; }
}
namespace synth::storage {
std::string PresetDirectory(std::string_view instrument) { return (storageRoot / std::string(instrument)).string(); }
bool EnsureLayout()
{
  for (const char* engine : {"DX7", "Juno", "Sampler", "Drums"}) std::filesystem::create_directories(storageRoot / engine);
  return true;
}
}
namespace device {
void SetSynthAudioRenderProvider(SynthAudioRenderProvider) {}
SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t index) { return {index, true, false, "regression"}; }
}

int main()
{
#if defined(_MSC_VER)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _set_error_mode(_OUT_TO_STDERR);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  using namespace synth;
  using namespace synth::audio;
  storageRoot = std::filesystem::current_path() / ("synth-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  synth::storage::EnsureLayout();
  std::array<std::int16_t, 2048> pcm{};
  const auto energy = [&]() { return std::accumulate(pcm.begin(), pcm.end(), std::uint64_t{0}, [](auto n, auto v) { return n + std::abs(int(v)); }); };
  const auto oscillator = [](std::uint8_t synthId, int oscillatorId) {
    void* cursor = nullptr;
    amy_event event;
    do {
      cursor = amy_embed::yield_synth_events(synthId, &event, false, cursor);
      if (event.osc == oscillatorId) return event;
    } while (cursor);
    assert(false);
    return event;
  };

  EnsureEngine(0, SynthEngineKind::Sampler);
  NoteOn(0, 60, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0); // Empty slots never synthesize a fallback tone.
  auto sample = std::make_shared<SampleBuffer>();
  sample->frameCount = 32;
  sample->frames = std::make_unique<std::int16_t[]>(sample->frameCount);
  std::fill_n(sample->frames.get(), sample->frameCount, 20000);
  SetSamplerFrames(0, sample, kSampleRate);
  NoteOn(0, 60, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() > 0);
  assert(std::all_of(pcm.begin() + 32, pcm.end(), [](auto v) { return v == 0; })); // No release-time wrap.
  SetEngineParameter(0, "sample.loop", 1);
  NoteOn(0, 60, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(std::any_of(pcm.begin() + 128, pcm.end(), [](auto v) { return v != 0; }));
  SetSamplerFrames(0, {}, kSampleRate);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0 && sample.use_count() == 1);
  assert(!g_fx.core && !g_fx.floatPath); // Effects allocate nothing until a setting needs them.

  Dx7SynthEngine dx7;
  assert(dx7.Init({0, 0x4100, "regression-dx7"}));
  // Validate the complete ROM bank, including offsets after the operator blocks.
  for (const auto& preset : Dx7FactoryPresets())
  {
    assert(dx7.LoadPreset(std::string(preset.id)));
    assert(dx7.RestoreState(dx7.CaptureState()));
    RenderFrames(pcm.data(), pcm.size());
    assert(g_engines[0].noteOffset >= -24 && g_engines[0].noteOffset <= 24);
  }
  assert(dx7.LoadPreset("dx7.rom1a.01"));
  NoteOn(0, 60, 100);
  RenderFrames(pcm.data(), pcm.size());
  const auto brassEnergy = energy();
  assert(brassEnergy > 0);
  for (int op = 1; op <= 6; ++op) assert(dx7.SetParameterValue("op" + std::to_string(op) + ".output", 0));
  RenderFrames(pcm.data(), pcm.size());
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() < brassEnergy / 10);
  assert(dx7.LoadPreset("dx7.rom1a.01"));
  SynthParameterInfo parameter;
  assert(dx7.GetParameterInfo("global.algorithm", parameter) && parameter.value == factory::kDx7[0][134] + 1);
  assert(dx7.GetParameterInfo("op1.output", parameter) && parameter.value == factory::kDx7[0][121]);
  assert(!dx7.GetParameterInfo("op1.velocity", parameter));
  assert(dx7.SetParameterValue("global.algorithm", 32));
  assert(dx7.SetParameterValue("op1.output", 41));
  assert(dx7.SetParameterValue("global.transpose", -7));
  assert(dx7.SavePreset("", "Restorable Brass"));
  const SynthEngineState saved = dx7.CaptureState();
  assert(saved.basePresetId == "dx7.rom1a.01" && saved.presetId == "user.1");
  assert(dx7.LoadPreset("dx7.rom1a.11"));
  assert(dx7.LoadPreset(saved.presetId));
  assert(dx7.CaptureState().values == saved.values);
  RenderFrames(pcm.data(), pcm.size());
  assert(g_engines[0].patchNumber == 128 && g_engines[0].dx7[134] == 31 && g_engines[0].dx7[121] == 41 && g_engines[0].noteOffset == -7);
  assert(oscillator(1, 0).algorithm == 32);
  assert(std::abs(oscillator(1, 7).amp_coefs[amy_embed::COEF_CONST] - 2.0f * Dx7Level(41)) < 0.0001f);
  SynthEngineState invalid = saved;
  invalid.values["op1.output"] = 100;
  assert(!dx7.RestoreState(invalid) && dx7.CaptureState().values == saved.values);
  Dx7SynthEngine reopened;
  assert(reopened.Init({1, 0x4101, "regression-reopen"}));
  assert(reopened.LoadPreset("user.1"));
  assert(reopened.CaptureState().basePresetId == saved.basePresetId && reopened.CaptureState().values == saved.values);
  assert(reopened.SavePreset("", "Second Track"));
  assert(reopened.CurrentPresetId() == "user.2");
  assert(dx7.SavePreset("", "Third Track"));
  assert(dx7.CurrentPresetId() == "user.3");
  const auto bank = storageRoot / "DX7" / "user.ccpreset";
  std::filesystem::rename(bank, bank.string() + ".bak");
  Dx7SynthEngine recovered;
  assert(recovered.Init({2, 0x4102, "regression-recovery"}));
  assert(recovered.LoadPreset("user.3"));
  // Failed writes and corrupt existing banks leave the current sound and disk data intact.
  const auto failedWriteState = recovered.CaptureState();
  std::filesystem::create_directory(bank.string() + ".tmp");
  assert(!recovered.SavePreset("user.3", "Must Fail"));
  assert(recovered.CaptureState().values == failedWriteState.values && recovered.CurrentPresetName() == failedWriteState.presetName);
  std::filesystem::remove(bank.string() + ".tmp");
  { std::ofstream corrupt(bank); corrupt << "INVALID BANK"; }
  assert(!recovered.SavePreset("user.3", "Must Still Fail"));
  { std::ifstream unchanged(bank); std::string text; std::getline(unchanged, text); assert(text == "INVALID BANK"); }

  // Staged project engines take the existing route without a new MIDI port allocation.
  Dx7SynthEngine staged;
  assert(staged.RestoreState(saved));
  std::filesystem::remove(bank); // The valid backup remains available for catalog refresh.
  assert(staged.RefreshPresetCatalog());
  assert(staged.ListPresets({0, 200}).totalCount == 131);
  assert(staged.CanReplace(dx7));
  const auto previousPort = dx7.PortId();
  staged.ReplaceBinding(dx7);
  assert(staged.PortId() == previousPort && dx7.PortId() == midi::MIDI_PORT_INVALID);
  assert(staged.CaptureState().values == saved.values && staged.CurrentPresetName() == saved.presetName);
  assert(!recovered.CanReplace(staged) && !dx7.CanReplace(dx7));

  // Render the real AMY backend: mute removes a Juno voice after the queued block drains.
  EnsureEngine(0, SynthEngineKind::None);
  EnsureEngine(1, SynthEngineKind::None);
  EnsureEngine(2, SynthEngineKind::None);
  RenderFrames(pcm.data(), pcm.size());
  JunoSynthEngine juno;
  assert(juno.Init({3, 0x4103, "regression-juno"}));
  for (const auto& preset : JunoFactoryPresets())
  {
    assert(juno.LoadPreset(std::string(preset.id)));
    assert(juno.RestoreState(juno.CaptureState()));
    RenderFrames(pcm.data(), pcm.size());
  }
  assert(juno.LoadPreset("juno.a51"));
  assert(juno.GetParameterInfo("vcf.cutoff", parameter) && parameter.value == factory::kJuno[32][5]);
  NoteOn(3, 60, 100);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() > 0);
  assert(juno.SetParameterValue("vca.level", 0));
  RenderFrames(pcm.data(), pcm.size());
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0);
  assert(juno.SetParameterValue("dco.range", 2));
  assert(juno.SetParameterValue("dco.pwm", 80));
  assert(juno.SetParameterValue("env.attack", 127));
  RenderFrames(pcm.data(), pcm.size());
  assert((g_engines[3].juno[16] & 7) == 4 && g_engines[3].juno[3] == 80 && g_engines[3].juno[11] == 127);
  assert(std::abs(oscillator(4, 2).freq_coefs[amy_embed::COEF_CONST] - 880.0f) < 0.01f);
  const auto envelope = oscillator(4, 0);
  assert(((g_engines[3].juno[17] & 4) ? envelope.eg1_times[0] : envelope.eg0_times[0]) == 1022);
  // Juno A12 carries +7/-3/-3 dB EQ and A13 carries -8/0/0 dB in AMY's ROM.
  // Loading either track must leave the shared master flat, independent of order.
  const auto assertFlatMaster = []() {
    for (const auto gain : amy_embed::amy_global.eq) assert(std::abs(S2F(gain) - 1.0f) < 0.000001f);
  };
  EnsureEngine(0, SynthEngineKind::Juno);
  SetEnginePreset(0, "juno.a12");
  RenderFrames(pcm.data(), pcm.size());
  assertFlatMaster();
  const float firstTrackLevel = oscillator(1, 0).amp_coefs[amy_embed::COEF_CONST];
  SetEnginePreset(3, "juno.a13");
  RenderFrames(pcm.data(), pcm.size());
  assertFlatMaster();
  assert(oscillator(1, 0).amp_coefs[amy_embed::COEF_CONST] == firstTrackLevel);
  SetEnginePreset(3, "juno.a13");
  SetEnginePreset(0, "juno.a12");
  RenderFrames(pcm.data(), pcm.size());
  assertFlatMaster();
  assert(oscillator(1, 0).amp_coefs[amy_embed::COEF_CONST] == firstTrackLevel);
  // Engine switches can enqueue sampler cleanup after the new synth has already
  // received a sequencer note. Cleanup must not erase AMY's note-off bookkeeping.
  EnsureEngine(3, SynthEngineKind::None);
  SetEnginePreset(0, "juno.a51");
  NoteOn(0, 60, 100);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() > 0);
  SetSamplerFrames(0, {}, kSampleRate);
  RenderFrames(pcm.data(), pcm.size());
  assert(g_engines[0].activeNotes[60]);
  AllNotesOff(0);
  // Allow the AMY filter/release tail to decay before checking the output.
  for (int block = 0; block < 24; ++block) RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0);

  DrumsSynthEngine drums;
  assert(drums.Init({0, 0x4104, "regression-drums"}));
  assert(drums.ListParameters({0, 32}).totalCount == 16); // 8 kit levels, master and 7 track effects.
  assert(drums.CurrentPresetId() == "drums.808");
  const auto stopDrums = [&]() {
    AllNotesOff(0);
    for (int block = 0; block < 4; ++block) RenderFrames(pcm.data(), pcm.size());
    assert(energy() == 0);
  };
  std::array<std::array<std::int16_t, 2048>, 8> drumAudio;
  for (std::size_t i = 0; i < kDrumNotes.size(); ++i)
  {
    stopDrums();
    assert(drums.SetParameterValue(kDrumParameterIds[i], 127));
    NoteOn(0, kDrumNotes[i], 127);
    RenderFrames(pcm.data(), pcm.size());
    const auto loud = energy();
    assert(loud > 0);
    drumAudio[i] = pcm;
    for (std::size_t j = 0; j < i; ++j) assert(drumAudio[i] != drumAudio[j]);
    stopDrums();
    assert(drums.SetParameterValue(kDrumParameterIds[i], 32));
    NoteOn(0, kDrumNotes[i], 127);
    RenderFrames(pcm.data(), pcm.size());
    assert(energy() > 0 && energy() < loud / 2);
    stopDrums();
    assert(drums.SetParameterValue(kDrumParameterIds[i], 0));
    NoteOn(0, kDrumNotes[i], 127);
    RenderFrames(pcm.data(), pcm.size());
    assert(energy() == 0);
    assert(drums.SetParameterValue(kDrumParameterIds[i], 100));
  }
  assert(drums.SetParameterValue("master.level", 0));
  for (auto note : kDrumNotes) NoteOn(0, note, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0);
  stopDrums();
  assert(drums.SetParameterValue("master.level", 100));
  NoteOn(0, 46, 127);
  RenderFrames(pcm.data(), pcm.size());
  NoteOff(0, 46);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() > 0 && g_engines[0].activeNotes[46]); // Open-hat tail survives key release.
  NoteOn(0, 42, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(!g_engines[0].activeNotes[46]);
  for (int block = 0; block < 4; ++block) RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0); // Closed hat chokes the longer open-hat voice.
  NoteOn(0, 46, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() > 0);
  NoteOff(0, 46);
  stopDrums(); // Explicit stop cancels even a released key's remaining tail.
  NoteOn(0, 60, 127); // Unsupported pads and malformed MIDI notes are silent.
  NoteOn(0, 128, 127);
  NoteOff(0, 255);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0 && std::none_of(g_engines[0].activeNotes.begin(), g_engines[0].activeNotes.end(), [](bool active) { return active; }));

  assert(drums.SetParameterValue("kick.level", 37));
  assert(drums.SetParameterValue("master.level", 83));
  assert(drums.SavePreset("", "Quiet Kick"));
  const auto savedDrums = drums.CaptureState();
  assert(savedDrums.basePresetId == "drums.808");
  assert(drums.LoadPreset("drums.808"));
  assert(drums.LoadPreset(savedDrums.presetId));
  RenderFrames(pcm.data(), pcm.size());
  assert(g_engines[0].drumLevels[0] == 37 && g_engines[0].drumMaster == 83);
  DrumsSynthEngine restoredDrums;
  assert(restoredDrums.RefreshPresetCatalog() && restoredDrums.LoadPreset(savedDrums.presetId));
  assert(restoredDrums.CaptureState().values == savedDrums.values);
  assert(restoredDrums.CanReplace(drums));
  restoredDrums.ReplaceBinding(drums);
  NoteOn(0, 46, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(restoredDrums.LoadPreset("drums.808"));
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0 && g_engines[0].drumMaster == 100 && g_engines[0].drumLevels[0] == 100);

  // Stress all voice-budget extremes and repeated reallocations without raising
  // the 24-voice/160-osc limits. Each track must retain its assigned voice count.
  const std::array<SynthEngineKind, 4> kinds{{SynthEngineKind::Drums, SynthEngineKind::Juno, SynthEngineKind::Dx7, SynthEngineKind::Sampler}};
  for (int pass = 0; pass < 12; ++pass)
  {
    for (std::size_t track = 0; track < kSynthTrackCount; ++track)
    {
      const auto kind = pass < 4 ? kinds[pass] : kinds[(track + pass) % kinds.size()];
      EnsureEngine(track, kind);
      if (kind == SynthEngineKind::Drums) SetEnginePreset(track, "drums.808");
      else if (kind == SynthEngineKind::Juno) SetEnginePreset(track, "juno.a51");
      else if (kind == SynthEngineKind::Dx7) SetEnginePreset(track, "dx7.rom1a.01");
      else SetSamplerFrames(track, sample, kSampleRate);
      NoteOn(track, kind == SynthEngineKind::Drums ? 46 : 60, 100);
    }
    RenderFrames(pcm.data(), pcm.size());
    assert(energy() > 0);
    std::array<bool, kAmyMaxOscs> occupied{};
    for (std::size_t track = 0; track < kSynthTrackCount; ++track)
    {
      const auto& state = g_engines[track];
      if (UsesAmy(state.kind))
      {
        std::array<std::uint16_t, kAmyMaxVoices> voices{};
        const int count = amy_embed::instrument_get_num_voices(static_cast<std::uint8_t>(track + 1), voices.data());
        assert(count == VoiceCountFor(state.kind));
        const int oscs = state.kind == SynthEngineKind::Dx7 ? 8 : state.kind == SynthEngineKind::Juno ? 6 : 1;
        for (int voice = 0; voice < count; ++voice)
        {
          const auto base = amy_embed::voice_to_base_osc[voices[voice]];
          assert(base + oscs <= kAmyMaxOscs);
          for (int osc = base; osc < base + oscs; ++osc)
          {
            assert(!occupied[osc] && amy_embed::osc_to_voice[osc] == voices[voice]);
            occupied[osc] = true;
          }
        }
      }
      AllNotesOff(track);
    }
  }
  for (std::size_t track = 0; track < kSynthTrackCount; ++track) EnsureEngine(track, SynthEngineKind::None);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0);
  EnsureEngine(0, SynthEngineKind::Juno);
  NoteOn(0, 128, 127);
  NoteOff(0, 255);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0 && std::none_of(g_engines[0].activeNotes.begin(), g_engines[0].activeNotes.end(), [](bool active) { return active; }));

  // Track mixer: real-time gain and mute on every engine kind, measured on rendered audio.
  const auto rms = [&]() {
    double sum = 0.0;
    for (const auto v : pcm) sum += double(v) * double(v);
    return std::sqrt(sum / double(pcm.size()));
  };
  const auto settle = [&](int buffers) { for (int i = 0; i < buffers; ++i) RenderFrames(pcm.data(), pcm.size()); };
  const auto near = [](double value, double expected, double tolerance) { return std::abs(value - expected) <= tolerance; };
  const auto resetMixTracks = [&]() {
    for (std::size_t track = 0; track < kSynthTrackCount; ++track)
    {
      AllNotesOff(track);
      EnsureEngine(track, SynthEngineKind::None);
      SetTrackGain(track, 1.0f);
      SetTrackMuted(track, false);
    }
    settle(4);
    assert(energy() == 0);
  };
  // Track 0's AMY voices (AMY synth 1) are routed through the render hook at the given settled gain.
  const auto amyTrackGainIs = [](float expected) -> bool {
    std::array<std::uint16_t, MAX_VOICES_PER_INSTRUMENT> voices{};
    const int count = amy_embed::instrument_get_num_voices(1, voices.data());
    for (int voice = 0; voice < count; ++voice)
    {
      if (g_amyOscTrack[amy_embed::voice_to_base_osc[voices[voice]]] != 1) return false; // Track 0.
    }
    const auto expectedQ16 = GainToQ16(expected);
    return count > 0 && g_trackMix[0].amyBlockStartQ16 == expectedQ16 && g_trackMix[0].amyBlockEndQ16 == expectedQ16;
  };
  const auto sounding = [](std::uint8_t note) {
    const auto& engine = g_engines[0];
    if (engine.kind == SynthEngineKind::Sampler)
      return std::any_of(engine.samplerVoices.begin(), engine.samplerVoices.end(), [&](const auto& voice) { return voice.active && !voice.releasing && voice.note == note; });
    return bool(engine.activeNotes[note]);
  };
  const auto setupTrack = [&](SynthEngineKind kind, const char* preset, SharedSampleBuffer frames) {
    EnsureEngine(0, kind);
    if (frames) { SetSamplerFrames(0, frames, kSampleRate); SetEngineParameter(0, "sample.loop", 1); }
    else SetEnginePreset(0, preset);
    settle(1);
  };

  resetMixTracks();
  SetTrackGain(0, 2.0f);
  assert(TrackGain(0) == 1.0f);
  SetTrackGain(0, -1.0f);
  assert(TrackGain(0) == 0.0f);
  SetTrackGain(0, 0.25f);
  assert(TrackGain(0) == 0.25f);
  SetTrackGain(0, std::nanf(""));
  assert(TrackGain(0) == 0.0f);
  SetTrackGain(kSynthTrackCount, 0.5f);
  SetTrackMuted(kSynthTrackCount, true);
  assert(TrackGain(kSynthTrackCount) == 1.0f && !TrackMuted(kSynthTrackCount) && TrackActivity(kSynthTrackCount) == 0.0f);
  SetTrackGain(0, 1.0f);

  // A looped tone with a whole number of periods keeps the sampler level steady.
  auto tone = std::make_shared<SampleBuffer>();
  tone->frameCount = 4400;
  tone->frames = std::make_unique<std::int16_t[]>(tone->frameCount);
  for (std::size_t i = 0; i < tone->frameCount; ++i) tone->frames[i] = static_cast<std::int16_t>(12000.0 * std::sin(6.283185307179586 * double(i) / 100.0));
  struct SustainedCase { SynthEngineKind kind; const char* preset; SharedSampleBuffer frames; };
  const std::array<SustainedCase, 3> sustainedCases{{
      {SynthEngineKind::Dx7, "dx7.rom1a.17", {}},
      {SynthEngineKind::Juno, "juno.a51", {}},
      {SynthEngineKind::Sampler, nullptr, tone},
  }};
  // Each run holds note 60 for 12 buffers (~0.56 s) on a fixed schedule; LFOs and operator beating
  // make the level drift within a note, so runs are compared window by window with a reference.
  constexpr int kRunBuffers = 12;
  const auto runNote = [&](auto beforeBuffer) {
    std::array<double, kRunBuffers> levels{};
    NoteOn(0, 60, 100);
    for (int buffer = 0; buffer < kRunBuffers; ++buffer)
    {
      beforeBuffer(buffer);
      RenderFrames(pcm.data(), pcm.size());
      levels[buffer] = rms();
    }
    if (sounding(60)) NoteOff(0, 60);
    SetTrackMuted(0, false);
    SetTrackGain(0, 1.0f);
    settle(16);
    assert(energy() == 0);
    return levels;
  };
  const auto ratio = [](const auto& run, const auto& reference, int from, int to) {
    double runSum = 0.0, referenceSum = 0.0;
    for (int buffer = from; buffer <= to; ++buffer) { runSum += run[buffer]; referenceSum += reference[buffer]; }
    return runSum / referenceSum;
  };
  for (const auto& mixCase : sustainedCases)
  {
    resetMixTracks();
    setupTrack(mixCase.kind, mixCase.preset, mixCase.frames);
    const auto reference = runNote([](int) {});
    assert(ratio(reference, reference, 0, kRunBuffers - 1) == 1.0 && reference[2] > 100.0);
    assert(near(ratio(runNote([](int) {}), reference, 2, kRunBuffers - 1), 1.0, 0.05)); // Runs repeat.
    // New notes start at the track gain.
    SetTrackGain(0, 0.5f);
    settle(1);
    const auto half = runNote([&](int buffer) {
      if (buffer == 2) assert(mixCase.frames ? g_trackMix[0].samplerGain == 0.5f : amyTrackGainIs(0.5f));
    });
    assert(near(ratio(half, reference, 0, kRunBuffers - 1), 0.5, 0.05));
    // A held note follows the fader down and back up.
    const auto held = runNote([&](int buffer) {
      if (buffer == 3) SetTrackGain(0, 0.5f);
      if (buffer == 7) SetTrackGain(0, 1.0f);
    });
    assert(near(ratio(held, reference, 0, 2), 1.0, 0.05));
    assert(near(ratio(held, reference, 4, 6), 0.5, 0.06));
    assert(near(ratio(held, reference, 8, kRunBuffers - 1), 1.0, 0.06));
    // Mute silences the held note for 24 AMY blocks, longer than AMY's ten-block silence
    // suspension, and drops new notes; unmute brings the held note back.
    const auto muted = runNote([&](int buffer) {
      if (buffer == 3) { SetTrackMuted(0, true); assert(TrackMuted(0) && TrackGain(0) == 1.0f); }
      if (buffer >= 5 && buffer <= 7) assert(energy() == 0 && !sounding(67)); // Buffers 4..6.
      if (buffer == 5) NoteOn(0, 67, 100);
      if (buffer == 7) SetTrackMuted(0, false);
      if (buffer >= 8) assert(sounding(60));
    });
    assert(near(ratio(muted, reference, 8, kRunBuffers - 1), 1.0, 0.06));
    // Note-offs pass while muted, so nothing hangs after unmute.
    const auto released = runNote([&](int buffer) {
      if (buffer == 2) SetTrackMuted(0, true);
      if (buffer == 3) NoteOff(0, 60);
      if (buffer == 8) SetTrackMuted(0, false);
    });
    assert(released[kRunBuffers - 1] == 0.0 && !sounding(60));
    // Back at unity the hook leaves AMY voices untouched and the sampler multiplies by exactly 1.
    assert(mixCase.frames ? g_trackMix[0].samplerGain == 1.0f : amyTrackGainIs(1.0f));
  }

  // Juno vca.level edits made with the fader down compose with the track gain, and preset
  // reloads and engine switches keep the gain.
  resetMixTracks();
  setupTrack(SynthEngineKind::Juno, "juno.a51", {});
  const auto junoReference = runNote([](int) {});
  const auto junoEdited = runNote([&](int buffer) {
    if (buffer == 3) SetEngineParameter(0, "vca.level", 50);
  });
  assert(ratio(junoEdited, junoReference, 5, kRunBuffers - 1) < 0.8);
  SetEnginePreset(0, "juno.a51");
  const auto junoEditedHalf = runNote([&](int buffer) {
    if (buffer == 0) SetTrackGain(0, 0.5f);
    if (buffer == 3) SetEngineParameter(0, "vca.level", 50);
  });
  assert(near(ratio(junoEditedHalf, junoEdited, 4, kRunBuffers - 1), 0.5, 0.05));
  SetTrackGain(0, 0.5f);
  SetEnginePreset(0, "juno.a51");
  EnsureEngine(0, SynthEngineKind::Dx7);
  EnsureEngine(0, SynthEngineKind::Juno);
  SetEnginePreset(0, "juno.a51");
  settle(1);
  assert(amyTrackGainIs(0.5f));
  assert(near(ratio(runNote([](int) {}), junoReference, 0, kRunBuffers - 1), 0.5, 0.05));

  // Drums: one-shots scale with the gain, master.level composes with it, mute drops pads.
  resetMixTracks();
  setupTrack(SynthEngineKind::Drums, "drums.808", {});
  const auto hit = [&](std::uint8_t note) {
    NoteOn(0, note, 100);
    RenderFrames(pcm.data(), pcm.size());
    const auto first = pcm;
    stopDrums();
    return first;
  };
  const auto rmsOf = [&](const std::array<std::int16_t, 2048>& audio) {
    double sum = 0.0;
    for (const auto v : audio) sum += double(v) * double(v);
    return std::sqrt(sum / double(audio.size()));
  };
  const auto kick = hit(36);
  SetTrackGain(0, 0.5f);
  settle(1);
  assert(near(rmsOf(hit(36)) / rmsOf(kick), 0.5, 0.05));
  SetEngineParameter(0, "master.level", 60);
  const double quietKickHalf = rmsOf(hit(36));
  SetTrackGain(0, 1.0f);
  settle(1);
  const double quietKick = rmsOf(hit(36));
  assert(quietKick < rmsOf(kick) * 0.8 && near(quietKickHalf / quietKick, 0.5, 0.05));
  SetEngineParameter(0, "master.level", 100);
  assert(hit(36) == kick); // A unity round trip renders the deterministic PCM path bit-identically.
  SetTrackMuted(0, true);
  NoteOn(0, 36, 127);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0 && !g_engines[0].activeNotes[36]);
  SetTrackMuted(0, false);
  settle(1);
  assert(hit(36) == kick);
  // A muted open-hat tail keeps running silently and resumes on unmute.
  NoteOn(0, 46, 127);
  RenderFrames(pcm.data(), pcm.size());
  SetTrackMuted(0, true);
  settle(1);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0);
  SetTrackMuted(0, false);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() > 0);
  stopDrums();

  // Meters: activity follows note-ons and the effective gain, then decays; the master peak decays too.
  resetMixTracks();
  setupTrack(SynthEngineKind::Sampler, nullptr, tone);
  NoteOn(0, 60, 127);
  settle(1);
  const float heldActivity = TrackActivity(0);
  assert(heldActivity > 0.5f && heldActivity <= 1.0f);
  settle(20);
  assert(near(TrackActivity(0), 0.5, 0.01)); // Held keys keep half the note-on bump.
  SetTrackGain(0, 0.5f);
  settle(8);
  assert(near(TrackActivity(0), 0.25, 0.01));
  const float peakWhilePlaying = OutputPeak();
  NoteOff(0, 60);
  settle(10);
  assert(TrackActivity(0) < 0.1f && OutputPeak() < peakWhilePlaying);
  settle(60);
  assert(TrackActivity(0) == 0.0f && OutputPeak() == 0.0f);
  SetTrackMuted(0, true);
  NoteOn(0, 60, 127);
  settle(1);
  assert(TrackActivity(0) == 0.0f);
  resetMixTracks();

  // Effects. Settings clamp and publish at once; delay times follow the tempo.
  using fx::MasterParam;
  const auto resetMasterFx = []() {
    for (std::size_t param = 0; param < fx::kMasterParamCount; ++param) SetMasterFx(static_cast<MasterParam>(param), fx::kMasterParams[param].defaultValue);
  };
  const auto neutralTrackFx = [](std::size_t track) {
    for (const char* id : fx::kTrackParameterIds) SetEngineParameter(track, id, fx::TrackParameterDefault(id));
  };
  // Renders until the effects path has handed back to the plain paths (all tails gone).
  const auto drainFx = [&](int maxBuffers) {
    RenderFrames(pcm.data(), pcm.size());
    for (int buffer = 0; buffer < maxBuffers && g_fx.floatPath; ++buffer) RenderFrames(pcm.data(), pcm.size());
    return !g_fx.floatPath;
  };
  const auto finiteFx = []() {
    return !g_fx.core || (std::all_of(g_fx.core->mix.begin(), g_fx.core->mix.end(), [](float v) { return std::isfinite(v); }) &&
                          std::isfinite(FxReturnLevel(fx::kBusDelay)) && std::isfinite(FxReturnLevel(fx::kBusReverb)));
  };
  const auto difference = [](const auto& audio, const auto& reference) {
    double diff = 0.0, sum = 0.0;
    for (std::size_t i = 0; i < reference.size(); ++i) { diff += std::abs(double(audio[i]) - double(reference[i])); sum += std::abs(double(reference[i])); }
    return diff / (std::max)(sum, 1.0);
  };
  assert(MasterFx(MasterParam::DelayTime) == fx::kMasterParams[0].defaultValue && MasterFx(MasterParam::EqLow) == 64);
  SetMasterFx(MasterParam::DelayFeedback, 500);
  SetMasterFx(MasterParam::DelayTime, -3);
  assert(MasterFx(MasterParam::DelayFeedback) == 127 && MasterFx(MasterParam::DelayTime) == 0);
  resetMasterFx();
  assert(FxBusAvailable(fx::kBusDelay) && FxBusAvailable(fx::kBusReverb) && !FxBusAvailable(fx::kBusCount) && FxReturnLevel(fx::kBusCount) == 0.0f);
  assert(near(fx::dsp::DelaySeconds(5, 120.0f), 0.25, 1e-6) && near(fx::dsp::DelaySeconds(7, 120.0f), 0.375, 1e-6));
  assert(near(fx::dsp::DelaySeconds(11, 30.0f), 0.5, 1e-6)); // A 4 s half note halves until it fits the line.

  // Neutral settings render bit-identically; a flat master EQ is an exact bypass, so a boost
  // changes the kick and returning to flat restores it bit for bit.
  setupTrack(SynthEngineKind::Drums, "drums.808", {});
  const auto fxHit = [&](std::uint8_t note) {
    NoteOn(0, note, 100);
    RenderFrames(pcm.data(), pcm.size());
    const auto first = pcm;
    AllNotesOff(0);
    settle(2);
    return first;
  };
  const auto dryKick = fxHit(36);
  neutralTrackFx(0);
  resetMasterFx();
  assert(fxHit(36) == dryKick && !g_fx.floatPath);
  SetMasterFx(MasterParam::EqLow, 100);
  const auto boostedKick = fxHit(36);
  assert(g_fx.floatPath && boostedKick != dryKick && rmsOf(boostedKick) > rmsOf(dryKick));
  SetMasterFx(MasterParam::EqLow, 64);
  assert(drainFx(8) && fxHit(36) == dryKick);
  // Buses allocate on their first send with a non-zero return level; tracks that never used
  // effects own no bus or chorus memory.
  assert(!g_fx.core->buses.DelayAllocated() && !g_fx.core->buses.ReverbAllocated());
  // Out of memory: the reverb reports itself unavailable and stays silent while the dry sound
  // plays on; the next settings change tries again.
  failAllocationsFrom = 4096;
  SetEngineParameter(0, "fx.reverb", 100);
  NoteOn(0, 36, 100);
  settle(1);
  failAllocationsFrom = 0;
  assert(!FxBusAvailable(fx::kBusReverb) && FxBusAvailable(fx::kBusDelay) && !g_fx.core->buses.ReverbAllocated() && energy() > 0);
  assert(FxReturnLevel(fx::kBusReverb) == 0.0f && !g_fx.routed[0]);
  SetEngineParameter(0, "fx.reverb", 0);
  AllNotesOff(0);
  assert(drainFx(4) && FxBusAvailable(fx::kBusReverb) == false && fxHit(36) == dryKick);
  assert(!g_fx.bus[1] && !g_fx.bus[2] && !g_fx.core->inserts[1].HasChorus() && !g_fx.core->inserts[2].HasChorus());
  SetMasterFx(MasterParam::DelayLevel, 0);
  SetEngineParameter(0, "fx.delay", 100);
  settle(1);
  assert(!g_fx.core->buses.DelayAllocated() && !g_fx.core->buses.ReverbAllocated());
  SetMasterFx(MasterParam::DelayLevel, 100);
  settle(1);
  assert(g_fx.core->buses.DelayAllocated() && !g_fx.core->buses.ReverbAllocated() && FxBusAvailable(fx::kBusDelay) && g_fx.routed[0]);
  SetEngineParameter(0, "fx.delay", 0);
  assert(drainFx(4) && fxHit(36) == dryKick);

  // Reverb: a closed hat grows a tail that decays to digital silence, the meter follows it, the
  // effects path then ends and the hat renders bit-identically again.
  std::array<std::uint64_t, 6> dryHatEnergy{}, wetHatEnergy{};
  NoteOn(0, 42, 100);
  for (auto& e : dryHatEnergy) { RenderFrames(pcm.data(), pcm.size()); e = energy(); }
  const auto dryHat = fxHit(42);
  SetMasterFx(MasterParam::ReverbSize, 20);
  SetEngineParameter(0, "fx.reverb", 127);
  NoteOn(0, 42, 100);
  for (auto& e : wetHatEnergy) { RenderFrames(pcm.data(), pcm.size()); e = energy(); assert(finiteFx()); }
  assert(g_fx.core->buses.ReverbAllocated() && FxBusAvailable(fx::kBusReverb) && FxReturnLevel(fx::kBusReverb) > 0.001f);
  assert(dryHatEnergy[2] == 0 && wetHatEnergy[2] > 1000 && wetHatEnergy[3] > 0 && wetHatEnergy[5] < wetHatEnergy[2]);
  SetEngineParameter(0, "fx.reverb", 0);
  assert(drainFx(100) && energy() == 0);
  settle(60);
  assert(FxReturnLevel(fx::kBusReverb) == 0.0f && fxHit(42) == dryHat);
  resetMasterFx();

  // A track whose bus cannot be allocated keeps its sends: its voices stay in AMY's mix and the
  // render hook feeds the reverb from there.
  resetMixTracks();
  EnsureEngine(2, SynthEngineKind::Drums);
  SetEnginePreset(2, "drums.808");
  settle(1);
  failAllocationsFrom = 1024;
  SetEngineParameter(2, "fx.reverb", 127);
  settle(1);
  failAllocationsFrom = 0;
  assert(!g_fx.bus[2] && g_fx.routed[2] && !g_fx.insertRouted[2]);
  NoteOn(2, 38, 120);
  settle(2);
  assert(FxReturnLevel(fx::kBusReverb) > 0.001f && finiteFx());
  SetEngineParameter(2, "fx.reverb", 0);
  AllNotesOff(2);
  assert(drainFx(150));
  resetMasterFx();

  // Delay: a sampler click echoes one synced note value later, and follows tempo changes.
  resetMixTracks();
  auto click = std::make_shared<SampleBuffer>();
  click->frameCount = 64;
  click->frames = std::make_unique<std::int16_t[]>(click->frameCount);
  std::fill_n(click->frames.get(), click->frameCount, 16000);
  EnsureEngine(1, SynthEngineKind::Sampler);
  SetSamplerFrames(1, click, kSampleRate);
  SetEngineParameter(1, "env.attack", 0);
  SetMasterFx(MasterParam::DelayTime, 5); // 1/8
  SetMasterFx(MasterParam::DelayFeedback, 0);
  SetMasterFx(MasterParam::DelayTone, 127);
  SetMasterFx(MasterParam::DelayLevel, 127);
  SetEngineParameter(1, "fx.delay", 127);
  const auto echoDelay = [&](float bpm) {
    SetTempo(bpm);
    settle(30); // Let the read head finish its tape-style glide to the new time.
    std::vector<std::int16_t> audio;
    NoteOn(1, 60, 127);
    for (int buffer = 0; buffer < 10; ++buffer) { RenderFrames(pcm.data(), pcm.size()); audio.insert(audio.end(), pcm.begin(), pcm.end()); assert(finiteFx()); }
    // Energy centroids of the click and of its echo.
    const auto centroid = [&](std::size_t from, std::size_t to) {
      double moment = 0.0, total = 0.0;
      for (std::size_t i = from; i < (std::min)(to, audio.size()); ++i) { const double v = double(audio[i]) * audio[i]; moment += v * double(i); total += v; }
      return total > 0.0 ? moment / total : 0.0;
    };
    const auto onset = static_cast<std::size_t>(std::find_if(audio.begin(), audio.end(), [](auto v) { return v != 0; }) - audio.begin());
    const double dry = centroid(onset, onset + 1500);
    const double echo = centroid(onset + 2000, audio.size());
    return (echo - dry) / double(kSampleRate);
  };
  assert(near(echoDelay(120.0f), 0.25, 0.001));
  assert(near(echoDelay(90.0f), 1.0 / 3.0, 0.001));
  SetEngineParameter(1, "fx.delay", 0);
  assert(drainFx(150));
  SetTempo(120.0f);
  resetMasterFx();

  // Inserts on a sampler tone: the DJ filter attenuates on both sides of its centre, crush, drive
  // and chorus change the signal, nothing goes non-finite, and the sampler's own drive is no
  // longer fed by fx.drive.
  resetMixTracks();
  setupTrack(SynthEngineKind::Sampler, nullptr, tone);
  const auto toneThrough = [&](const char* id, int value) {
    SetEngineParameter(0, id, value);
    NoteOn(0, 60, 100);
    settle(3);
    RenderFrames(pcm.data(), pcm.size());
    assert(finiteFx());
    const auto audio = pcm;
    NoteOff(0, 60);
    neutralTrackFx(0);
    assert(drainFx(20));
    return audio;
  };
  const auto plainTone = toneThrough("fx.filter", 64);
  assert(!g_fx.routed[0]);
  assert(rmsOf(toneThrough("fx.filter", 5)) < 0.2 * rmsOf(plainTone));    // Low-pass near 120 Hz.
  assert(rmsOf(toneThrough("fx.filter", 120)) < 0.1 * rmsOf(plainTone));  // High-pass near 4 kHz.
  const auto crushed = toneThrough("fx.crush", 110);
  assert(difference(crushed, plainTone) > 0.05 && rmsOf(crushed) > 0.3 * rmsOf(plainTone));
  const auto driven = toneThrough("fx.drive", 127);
  assert(difference(driven, plainTone) > 0.05 && rmsOf(driven) > 0.5 * rmsOf(plainTone));
  assert(g_engines[0].sampler.drive == 0);
  const auto chorused = toneThrough("fx.chorus", 2);
  assert(difference(chorused, plainTone) > 0.05 && g_fx.core->inserts[0].HasChorus() && !g_fx.core->inserts[2].HasChorus());

  // A routed Juno track: muting still silences it, unmuting brings it back, and presets and
  // engine switches reset the track effects (the Juno chorus follows the patch).
  resetMixTracks();
  setupTrack(SynthEngineKind::Juno, "juno.a11", {});
  assert(g_fx.tracks[0].inserts.chorus == factory::JunoValue(factory::kJuno[0], "fx.chorus") && g_fx.tracks[0].inserts.chorus == 1);
  SetEngineParameter(0, "fx.drive", 90);
  SetEngineParameter(0, "fx.filter", 30);
  NoteOn(0, 60, 100);
  settle(3);
  assert(g_fx.routed[0] && g_fx.bus[0] && energy() > 0 && finiteFx());
  SetTrackMuted(0, true);
  settle(1);
  RenderFrames(pcm.data(), pcm.size());
  assert(energy() == 0);
  SetTrackMuted(0, false);
  settle(2);
  assert(energy() > 0);
  NoteOff(0, 60);
  SetEnginePreset(0, "juno.a51");
  settle(1);
  assert(g_fx.tracks[0].inserts.drive == 0 && g_fx.tracks[0].inserts.filter == 64 && g_fx.tracks[0].inserts.chorus == 0);
  SetEngineParameter(0, "fx.reverb", 50);
  SetEngineParameter(0, "fx.chorus", 3);
  EnsureEngine(0, SynthEngineKind::Dx7);
  settle(1);
  assert(g_fx.tracks[0].reverbSend == 0 && g_fx.tracks[0].inserts.chorus == 0);
  SetEngineParameter(0, "fx.crush", 40);
  SetEnginePreset(0, "dx7.rom1a.01");
  settle(1);
  assert(g_fx.tracks[0].inserts.crush == 0 && drainFx(20));

  // New engine parameters. DX7: LFO wave, pitch-mod sensitivity, operator mode and the full
  // operator envelope.
  SetEngineParameter(0, "global.lfo_wave", 2);
  SetEngineParameter(0, "global.lfo_pitch", 50);
  SetEngineParameter(0, "global.lfo_pms", 3);
  SetEngineParameter(0, "op1.level4", 20);
  SetEngineParameter(0, "op1.mode", 1);
  SetEngineParameter(0, "op1.coarse", 1);
  settle(1);
  assert(oscillator(1, 1).wave == SAW_UP);
  assert(near(oscillator(1, 0).freq_coefs[amy_embed::COEF_MOD], 0.6 * std::pow(1.7, 2.0) * 50.0 / 1188.0, 1e-5));
  assert(g_engines[0].dx7[105 + 7] == 20 && near(oscillator(1, 7).eg0_values[0], Dx7Level(20), 1e-4));
  assert(std::isnan(oscillator(1, 7).ratio) && oscillator(1, 7).freq_coefs[amy_embed::COEF_CONST] > 9.0f);
  SetEngineParameter(0, "op1.mode", 0);
  settle(1);
  {
    const auto& op = g_engines[0].dx7;
    const double expectedRatio = 1.0 + (op[105 + 19] + (op[105 + 20] - 7.0) / 8.0) / 100.0;
    assert(op[105 + 17] == 0 && near(oscillator(1, 7).ratio, expectedRatio, 1e-3));
  }
  // Juno: DCO LFO depth, pulse switch, VCF polarity and the VCA mode, which swaps the envelopes.
  EnsureEngine(0, SynthEngineKind::Juno);
  SetEnginePreset(0, "juno.a51");
  SetEngineParameter(0, "dco.lfo", 127);
  SetEngineParameter(0, "dco.pulse", 0);
  SetEngineParameter(0, "vcf.env", 127);
  SetEngineParameter(0, "vcf.polarity", 1);
  SetEngineParameter(0, "vca.mode", 0);
  settle(1);
  assert(near(oscillator(1, 2).freq_coefs[amy_embed::COEF_MOD], 0.03, 1e-6) && oscillator(1, 2).amp_coefs[amy_embed::COEF_CONST] == 0.0f);
  auto control = oscillator(1, 0);
  const std::uint32_t attackMs = 6 + 8 * g_engines[0].juno[11];
  const auto zero = [](float coefficient) { return coefficient == 0.0f || std::isnan(coefficient); }; // AMY reports zero coefficients as unset.
  assert(control.filter_freq_coefs[amy_embed::COEF_EG0] == -11.0f && zero(control.filter_freq_coefs[amy_embed::COEF_EG1]) && control.eg0_times[0] == attackMs);
  SetEngineParameter(0, "vca.mode", 1);
  settle(1);
  control = oscillator(1, 0);
  assert(zero(control.filter_freq_coefs[amy_embed::COEF_EG0]) && control.filter_freq_coefs[amy_embed::COEF_EG1] == -11.0f);
  // Envelope 0 is now the VCA gate (held at 1, released at once); the ADSR drives the VCF from envelope 1.
  assert(control.eg0_values[1] == 1.0f && control.eg0_times[2] == 0 && control.eg0_values[2] == 0.0f && control.eg1_times[0] == attackMs);
  assert((g_engines[0].juno[17] & 6) == 6 && (g_engines[0].juno[16] & 8) == 0);

  // Everything at its extreme on three tracks at once stays finite and bounded.
  resetMixTracks();
  EnsureEngine(0, SynthEngineKind::Juno);
  SetEnginePreset(0, "juno.a11");
  EnsureEngine(1, SynthEngineKind::Drums);
  SetEnginePreset(1, "drums.808");
  EnsureEngine(2, SynthEngineKind::Sampler);
  SetSamplerFrames(2, tone, kSampleRate);
  SetEngineParameter(2, "sample.loop", 1);
  for (std::size_t track = 0; track < 3; ++track)
  {
    for (const char* id : {"fx.drive", "fx.crush", "fx.resonance", "fx.delay", "fx.reverb"}) SetEngineParameter(track, id, 127);
    SetEngineParameter(track, "fx.filter", track == 1 ? 127 : 0);
    SetEngineParameter(track, "fx.chorus", 3);
  }
  for (const auto param : {MasterParam::DelayFeedback, MasterParam::DelayLevel, MasterParam::ReverbSize, MasterParam::ReverbLevel,
                           MasterParam::Compressor, MasterParam::EqLow, MasterParam::EqHigh, MasterParam::Tape})
    SetMasterFx(param, 127);
  SetMasterFx(MasterParam::DelayTone, 0);
  SetMasterFx(MasterParam::ReverbDamp, 0);
  SetMasterFx(MasterParam::DelayTime, 0);
  for (int buffer = 0; buffer < 12; ++buffer)
  {
    NoteOn(0, std::uint8_t(48 + buffer), 127);
    NoteOn(1, buffer % 2 ? 36 : 38, 127);
    NoteOn(2, std::uint8_t(60 + buffer % 5), 127);
    RenderFrames(pcm.data(), pcm.size());
    assert(finiteFx() && energy() > 0 && OutputPeak() <= 1.0f && FxReturnLevel(fx::kBusDelay) <= 1.0f && FxReturnLevel(fx::kBusReverb) <= 1.0f);
  }
  for (std::size_t track = 0; track < 3; ++track) { AllNotesOff(track); neutralTrackFx(track); }
  resetMasterFx();
  settle(8);
  assert(finiteFx() && FxReturnLevel(fx::kBusDelay) > 0.0f); // Tails ring on after the sends close.

  std::cout << "Synth regression passed: sample boundaries, factory decode, parameter routing, preset persistence/recovery, rendered VCA mute, cross-track master EQ, Drums PCM/levels/tails/stop/persistence, mixed engine switching, MIDI bounds, track gain/mute/meters, effects (bypass identity, allocation, delay timing, reverb tail, inserts, routed mute, preset reset, extremes), new DX7/Juno parameters.\n";
}

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#include "Devices/DeviceAudio.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/Engines/Juno/JunoSynthEngine.h"
// Include the runtime once to check both rendered PCM and mapping state against AMY.
#include "Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp"

namespace amy_embed { extern "C" {
extern std::uint16_t* voice_to_base_osc;
extern std::uint8_t* osc_to_voice;
} }

namespace { std::filesystem::path storageRoot; }
namespace Device::Storage {
std::string PresetDirectoryPath(std::string_view instrument) { return (storageRoot / std::string(instrument)).string(); }
bool EnsureDirectoryLayout()
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
  Device::Storage::EnsureDirectoryLayout();
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
  assert(drums.ListParameters({0, 32}).totalCount == 9);
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
  std::cout << "Synth regression passed: sample boundaries, factory decode, parameter routing, preset persistence/recovery, rendered VCA mute, cross-track master EQ, Drums PCM/levels/tails/stop/persistence, mixed engine switching, MIDI bounds.\n";
}

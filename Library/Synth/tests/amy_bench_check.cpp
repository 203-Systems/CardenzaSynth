// The test build's AMY sources and drum kits (Library/Synth/Audio/Amy/AmyBench.h) on the host: each
// one must sound, and its level against the factory presets' target is printed (the table's levelDb).
// AMY_BENCH_WAV_DIR=<dir> also writes each one as a WAV file there, to listen to.
// CARDENZA_GAMMA9001_BIN names the kits' sample file (tools/audio/gamma9001_blob.py).
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "Devices/DeviceAudio.h"
#include "Library/Synth/Core/SynthStorage.h"
#include "Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp"

namespace { std::filesystem::path storageRoot; }
namespace synth::storage {
std::string PresetDirectory(std::string_view instrument) { return (storageRoot / std::string(instrument)).string(); }
bool EnsureLayout() { return true; }
}
namespace device {
void SetSynthAudioRenderProvider(SynthAudioRenderProvider) {}
SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t index) { return {index, true, false, "bench"}; }
}

namespace {

using namespace synth;
using namespace synth::audio;

constexpr std::size_t kBlock = 256;

void WriteWav(const std::filesystem::path& path, const std::vector<std::int16_t>& audio)
{
  std::FILE* file = std::fopen(path.string().c_str(), "wb");
  if (file == nullptr) return;
  const auto u32 = [&](std::uint32_t value) { std::fwrite(&value, 4, 1, file); };
  const auto u16 = [&](std::uint16_t value) { std::fwrite(&value, 2, 1, file); };
  const auto bytes = static_cast<std::uint32_t>(audio.size() * 2);
  std::fwrite("RIFF", 1, 4, file); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, file);
  u32(16); u16(1); u16(1); u32(kSampleRate); u32(kSampleRate * 2); u16(2); u16(16);
  std::fwrite("data", 1, 4, file); u32(bytes);
  std::fwrite(audio.data(), 2, audio.size(), file);
  std::fclose(file);
}

void Render(std::size_t blocks, std::vector<std::int16_t>* audio = nullptr)
{
  std::array<std::int16_t, kBlock> block{};
  for (std::size_t index = 0; index < blocks; ++index)
  {
    RenderFrames(block.data(), block.size());
    if (audio != nullptr) audio->insert(audio->end(), block.begin(), block.end());
  }
}

double Decibels(double value) { return 20.0 * std::log10((std::max)(value, 1.0) / 32767.0); }

// Loudest 0.3 s and highest sample of what was rendered (what PresetLevels.inc evens out).
void Measure(const std::vector<std::int16_t>& audio, double& peak, double& loudness)
{
  constexpr std::size_t kWindow = 50 * kBlock;
  double highest = 0.0, loudest = 0.0;
  for (std::size_t start = 0; start + kWindow <= audio.size(); start += kWindow)
  {
    double sum = 0.0;
    for (std::size_t index = start; index < start + kWindow; ++index)
    {
      highest = (std::max)(highest, std::abs(static_cast<double>(audio[index])));
      sum += static_cast<double>(audio[index]) * audio[index];
    }
    loudest = (std::max)(loudest, sum / kWindow);
  }
  peak = Decibels(highest);
  loudness = Decibels(std::sqrt(loudest));
}

// A phrase to listen to: a low note, a middle chord, a high note.
void Phrase(std::size_t track, std::vector<std::int16_t>& audio)
{
  const auto play = [&](std::initializer_list<int> notes, std::size_t held, std::size_t after) {
    for (int note : notes) NoteOn(track, static_cast<std::uint8_t>(note), 105);
    Render(held, &audio);
    for (int note : notes) NoteOff(track, static_cast<std::uint8_t>(note));
    Render(after, &audio);
  };
  play({36}, 150, 60);
  play({48, 55, 60, 64}, 260, 100);
  play({72}, 120, 60);
  play({60, 67}, 200, 160);
}

}  // namespace

int main()
{
  storageRoot = std::filesystem::temp_directory_path() / "cardenza-amy-bench";
  const char* wavDirectory = std::getenv("AMY_BENCH_WAV_DIR");
  if (wavDirectory != nullptr) std::filesystem::create_directories(wavDirectory);
  constexpr double kTargetLoudness = -18.0, kHighestPeak = -6.0, kSilent = -70.0;
  int failures = 0;

  // What a Juno voice takes here, to compare the sources' render times with (a host is not the board:
  // the ratio is a hint, the stress test's amy: scenarios measure it).
  const auto renderMicros = [](std::size_t track, int voices) {
    for (int voice = 0; voice < voices; ++voice) NoteOn(track, static_cast<std::uint8_t>(48 + 3 * voice), 100);
    Render(20);
    const auto start = std::chrono::steady_clock::now();
    Render(400);
    const double micros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / 400.0;
    for (int voice = 0; voice < voices; ++voice) NoteOff(track, static_cast<std::uint8_t>(48 + 3 * voice));
    Render(200);
    return micros / voices;
  };
  EnsureEngine(1, SynthEngineKind::Juno);
  SetEnginePreset(1, "juno.a68");
  Render(4);
  const double junoMicros = renderMicros(1, 6);
  std::printf("%-16s %6.1f us a voice here\n", "juno.a68", junoMicros);

  for (std::size_t index = 0; index < bench::kSources.size(); ++index)
  {
    const bench::Source& source = bench::kSources[index];
    SetEnginePreset(1, bench::SourcePresetId(index).data());
    Render(4);
    assert(g_engines[1].patchNumber == static_cast<int>(index));
    const int oscs = amy_embed::instrument_get_oscs_per_voice(g_engines[1].synthId);
    // The level that brings a held middle C to the factory presets' target, as PresetLevels.inc is made.
    double level = 0.0, peak = 0.0, loudness = 0.0;
    for (int pass = 0; pass < 4; ++pass)
    {
      g_engines[1].presetGain = static_cast<float>(std::pow(10.0, level / 20.0));
      Render(6);
      std::vector<std::int16_t> audio;
      NoteOn(1, 60, 110);
      Render(200, &audio);
      NoteOff(1, 60);
      Render(120);
      Measure(audio, peak, loudness);
      if (loudness < kSilent) break;
      const double change = (std::min)(kTargetLoudness - loudness, kHighestPeak - peak);
      if (std::abs(change) < 1.0) break;
      level = std::clamp(level + change, -30.0, 30.0);
    }
    const bool silent = loudness < kSilent;
    if (silent) ++failures;
    g_engines[1].presetGain = static_cast<float>(std::pow(10.0, source.levelDb / 20.0));
    Render(6);
    const double micros = renderMicros(1, source.voices);
    std::printf("%-16s %s oscs %2d  level %+3d dB (table %+3d)  loudness %6.1f peak %6.1f  %6.1f us a voice here (%.2f Juno)\n", source.name,
                silent ? "SILENT" : "ok    ", oscs, static_cast<int>(std::lround(level)), source.levelDb, loudness, peak, micros, micros / junoMicros);
    if (wavDirectory != nullptr)
    {
      std::vector<std::int16_t> audio;
      Phrase(1, audio);
      WriteWav(std::filesystem::path(wavDirectory) / (std::string(source.name) + ".wav"), audio);
    }
  }

  EnsureEngine(0, SynthEngineKind::Drums);
  for (const bench::DrumKit& kit : bench::kDrumKits)
  {
    SetEnginePreset(0, kit.id);
    Render(4);
    std::vector<std::int16_t> all;
    std::printf("%-16s", kit.name);
    for (std::size_t pad = 0; pad < kDrumNotes.size(); ++pad)
    {
      std::vector<std::int16_t> audio;
      NoteOn(0, kDrumNotes[pad], 110);
      Render(60, &audio);
      NoteOff(0, kDrumNotes[pad]);
      Render(20, &audio);
      double peak = 0.0, loudness = 0.0;
      Measure(audio, peak, loudness);
      // A drum is shorter than the loudness window: its peak says whether it sounded.
      const bool silent = peak < -50.0;
      if (silent) ++failures;
      std::printf(" %s %5.1f%s", kDrumLabels[pad], peak, silent ? " SILENT" : "");
      all.insert(all.end(), audio.begin(), audio.end());
    }
    std::printf("\n");
    if (wavDirectory != nullptr) WriteWav(std::filesystem::path(wavDirectory) / (std::string(kit.name) + ".wav"), all);
  }
  std::fflush(stdout);
  if (failures != 0) std::fprintf(stderr, "amy_bench_check: %d silent\n", failures);
  _Exit(failures == 0 ? 0 : 1);
}

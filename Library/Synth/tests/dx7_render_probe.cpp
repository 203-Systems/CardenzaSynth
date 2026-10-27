// Offline real-backend workload for comparing CPU cost and PCM before/after DSP changes.
// Timings describe this host, not the ESP32's real-time margin. Optional argv[1] dumps raw mono PCM.
#include <array>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "Devices/DeviceAudio.h"
#include "Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp"

namespace synth::storage {
void NotifyWriteFailure() {}
bool EnsureLayout() { return false; }
std::string PresetDirectory(std::string_view) { return {}; }
}

namespace device {
void SetSynthAudioRenderProvider(SynthAudioRenderProvider) {}
SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t index) { return {index, true, false, "dx7-probe"}; }
}

int main(int argc, char** argv)
{
  using namespace synth;
  using namespace synth::audio;
  using Clock = std::chrono::steady_clock;
  std::ofstream pcm;
  if (argc > 1)
  {
    pcm.open(argv[1], std::ios::binary);
    if (!pcm) return 1;
  }
  std::array<std::int16_t, kAmyFramesPerBlock> block{};
  constexpr std::array<std::uint8_t, 4> notes{{48, 55, 60, 67}};
  std::cout << "preset,voices,mean_us,p95_us,max_us,peak,hash\n";
  for (const auto& preset : Dx7FactoryPresets())
  {
    for (int voices = 1; voices <= 4; ++voices)
    {
      // All patches at three voices; also measure scaling for representative patches.
      if (voices != 3 && preset.patchNumber != 128 && preset.patchNumber != 138 &&
          preset.patchNumber != 150 && preset.patchNumber != 219) continue;
      EnsureEngine(0, SynthEngineKind::None);
      RenderFrames(block.data(), block.size());
      EnsureEngine(0, SynthEngineKind::Dx7);
      SetEnginePreset(0, preset.id);
      RenderFrames(block.data(), block.size());
      for (int note = 0; note < voices; ++note) NoteOn(0, notes[note], 110);
      std::array<double, 512> timings{};
      std::uint64_t hash = 14695981039346656037ULL;
      int peak = 0;
      double sum = 0;
      for (std::size_t index = 0; index < timings.size(); ++index)
      {
        if (index == 192) AllNotesOff(0);
        // Repeated chords include release tails and voice reuse under the four-voice limit.
        if (index >= 384 && index % 16 == 0)
        {
          AllNotesOff(0);
          for (int note = 0; note < voices; ++note)
            NoteOn(0, notes[note] + ((index / 16) % 3), 100);
        }
        const auto start = Clock::now();
        if (RenderFrames(block.data(), block.size()) != block.size()) return 2;
        timings[index] = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
        sum += timings[index];
        for (auto sample : block)
        {
          peak = std::max(peak, std::abs(static_cast<int>(sample)));
          const auto bits = static_cast<std::uint16_t>(sample);
          hash = (hash ^ (bits & 255)) * 1099511628211ULL;
          hash = (hash ^ (bits >> 8)) * 1099511628211ULL;
        }
        if (pcm.is_open()) pcm.write(reinterpret_cast<const char*>(block.data()), sizeof(block));
      }
      std::sort(timings.begin(), timings.end());
      std::cout << preset.id << ',' << voices << ',' << std::fixed << std::setprecision(2)
                << sum / timings.size() << ',' << timings[486] << ',' << timings.back() << ','
                << peak << ',' << std::hex << hash << std::dec << '\n';
    }
  }
  return pcm.is_open() && !pcm ? 3 : 0;
}

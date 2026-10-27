#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Devices/DeviceAudio.h"
#include "Library/Synth/Core/Engines/Dx7/Dx7SynthEngine.h"
#include "Library/Synth/Core/SynthStorage.h"
// Match synth_regression: exercise the real AMY backend and its render hook in one translation unit.
#include "Library/Synth/Audio/Amy/SynthAudioRuntimeAmy.cpp"

namespace {
const std::filesystem::path storageRoot = std::filesystem::temp_directory_path() /
    ("cardenza-hotpath-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

void Require(bool passed, const char* message)
{
  if (!passed) throw std::runtime_error(message);
}
}

namespace synth::storage {
void NotifyWriteFailure() {}
std::string PresetDirectory(std::string_view instrument) { return (storageRoot / std::string(instrument)).string(); }
bool EnsureLayout() { return true; }  // No user banks or filesystem writes are needed by this test.
}

namespace device {
void SetSynthAudioRenderProvider(SynthAudioRenderProvider) {}
SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t index) { return {index, true, false, "hotpath-test"}; }
}

namespace {
using namespace synth;
using namespace synth::audio;

void TestWarmUpLeavesCommandsQueued()
{
  EnsureEngine(0, SynthEngineKind::Sampler);
  const auto pending = g_pendingEvents.size();
  WarmUp();
  Require(g_amyInitialized, "warm-up did not initialize AMY");
  Require(g_pendingEvents.size() == pending && !g_engines[0].configured,
          "warm-up consumed engine commands outside the render thread");
  Require(amy_embed::amy_global.total_blocks == 0 && g_amyBlockCursor == kAmyFramesPerBlock,
          "warm-up advanced the audio clock");
}

void TestWarmUpPreservesLiveSamplerAudio()
{
  auto sample = std::make_shared<SampleBuffer>();
  sample->frameCount = 2048;
  sample->frames = std::make_unique<std::int16_t[]>(sample->frameCount);
  for (std::size_t i = 0; i < sample->frameCount; ++i)
    sample->frames[i] = static_cast<std::int16_t>(15000.0 * std::sin(0.037 * static_cast<double>(i)));

  const auto render = [&](bool warmUp) {
    EnsureEngine(0, SynthEngineKind::None);
    std::array<std::int16_t, 64> silence{};
    RenderFrames(silence.data(), silence.size());
    EnsureEngine(0, SynthEngineKind::Sampler);
    SetSamplerFrames(0, sample, kSampleRate);
    NoteOn(0, 60, 110);
    std::vector<std::int16_t> output(777);
    RenderFrames(output.data(), 73);
    if (warmUp)
    {
      // These calls model several engine attachments while another track is playing.
      for (int call = 0; call < 4; ++call) WarmUp();
    }
    RenderFrames(output.data() + 73, output.size() - 73);
    return output;
  };

  const auto expected = render(false);
  const auto actual = render(true);
  Require(std::any_of(expected.begin(), expected.end(), [](auto value) { return value != 0; }),
          "live sampler fixture was silent");
  Require(actual == expected, "warm-up dropped or changed live sampler audio");
}

void TestLiveEngineAttachmentPreservesAmyBlock()
{
  Dx7SynthEngine playing;
  Require(playing.Init({0, 0x4200, "hotpath-playing"}), "playing DX7 failed to initialize");
  NoteOn(0, 60, 100);
  std::array<std::int16_t, 73> prefix{};
  RenderFrames(prefix.data(), prefix.size());
  const auto clock = amy_embed::amy_global.total_blocks;
  const auto cursor = g_amyBlockCursor;
  const auto block = g_amyBlock;

  Dx7SynthEngine attached;
  Require(attached.Init({1, 0x4201, "hotpath-attached"}), "second DX7 failed to initialize");
  Require(!g_engines[1].configured, "engine attachment applied queued commands on the caller thread");
  Require(amy_embed::amy_global.total_blocks == clock && g_amyBlockCursor == cursor && g_amyBlock == block,
          "engine attachment consumed a partially played AMY block");

  Dx7SynthEngine replacement;
  Require(replacement.RestoreState(attached.CaptureState()) && replacement.CanReplace(attached),
          "replacement DX7 failed to stage");
  replacement.ReplaceBinding(attached);
  Require(amy_embed::amy_global.total_blocks == clock && g_amyBlockCursor == cursor && g_amyBlock == block,
          "engine replacement consumed a partially played AMY block");

  std::array<std::int16_t, kAmyFramesPerBlock - 73> tail{};
  RenderFrames(tail.data(), tail.size());
  Require(g_engines[1].configured, "render thread did not apply the attached engine");
  Require(amy_embed::amy_global.total_blocks == clock && g_amyBlockCursor == kAmyFramesPerBlock,
          "the next render skipped the remaining AMY samples");
}

void TestGainHookMatchesOriginalArithmetic()
{
  constexpr std::uint16_t osc = 0;
  g_amyOscTrack[osc] = 1;
  g_fxHook[0] = {};
  std::array<std::int32_t, 256> original{};
  const std::array<std::int32_t, 10> edges{{
      std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max(),
      -65537, -65536, -1, 0, 1, 65535, 65536, 65537}};
  for (std::size_t index = 0; index < original.size(); ++index)
    original[index] = edges[index % edges.size()];

  const auto check = [&](std::int32_t start, std::int32_t end, std::uint16_t length) {
    auto actual = original;
    auto expected = original;
    for (std::uint16_t index = 0; index < length; ++index)
    {
      const std::int64_t gain = start + ((end - start) * static_cast<std::int32_t>(index)) / length;
      expected[index] = static_cast<std::int32_t>((static_cast<std::int64_t>(expected[index]) * gain) >> 16);
    }
    g_trackMix[0].amyBlockStartQ16 = start;
    g_trackMix[0].amyBlockEndQ16 = end;
    Require(ApplyTrackGainToAmyVoice(osc, actual.data(), length) == 0, "gain hook unexpectedly consumed the voice");
    Require(actual == expected, "gain hook changed fixed-point output or wrote past the block");
  };

  for (const auto length : {1, 7, 64, 127, 128, 255, 256})
  {
    for (const auto gain : {1, 257, 8192, 32767, 32768, 49152, 65535, 65536})
      check(gain, gain, static_cast<std::uint16_t>(length));
    check(0, 16384, static_cast<std::uint16_t>(length));
    check(65536, 49152, static_cast<std::uint16_t>(length));
    check(12345, 23456, static_cast<std::uint16_t>(length));
    check(23456, 12345, static_cast<std::uint16_t>(length));
  }
  auto muted = original;
  g_trackMix[0].amyBlockStartQ16 = g_trackMix[0].amyBlockEndQ16 = 0;
  Require(ApplyTrackGainToAmyVoice(osc, muted.data(), muted.size()) == 1 && muted == original,
          "muting must skip mixing without changing voice samples");
}
}

int main()
{
  try
  {
    TestWarmUpLeavesCommandsQueued();
    TestWarmUpPreservesLiveSamplerAudio();
    TestLiveEngineAttachmentPreservesAmyBlock();
    TestGainHookMatchesOriginalArithmetic();
    std::cout << "audio hotpath: queued initialization, uninterrupted live audio and gain equivalence passed\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

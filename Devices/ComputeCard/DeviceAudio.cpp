#include "Devices/DeviceAudio.h"

#include <array>
#include <cstdint>
#include <mutex>

#include "audio/audio.h"
#include "Library/Synth/SynthAudioRuntime.h"

#if defined(ESP_PLATFORM)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace device {

namespace {

#if defined(ESP_PLATFORM)

constexpr std::size_t kRenderFrameCount = 256;
constexpr BaseType_t kAudioTaskPinnedCore = 1;
constexpr UBaseType_t kAudioTaskPriority = 6;
std::mutex g_audioPumpMutex;
TaskHandle_t g_audioPumpTask = nullptr;

void ResetAudioPumpState()
{
  std::lock_guard<std::mutex> lock(g_audioPumpMutex);
  g_audioPumpTask = nullptr;
}

void AudioRenderTask(void*)
{
  if (!ComputeCardHAL::Audio::EnableOutput())
  {
    ResetAudioPumpState();
    vTaskDelete(nullptr);
    return;
  }

  if (!ComputeCardHAL::Audio::NotifyOutputClockStarted())
  {
    ResetAudioPumpState();
    vTaskDelete(nullptr);
    return;
  }

  std::array<std::int16_t, kRenderFrameCount> buffer{};
  for (;;)
  {
    synth::audio::RenderFrames(buffer.data(), buffer.size());
    const float gain = static_cast<float>(ComputeCardHAL::Audio::GetVolume()) / 100.0f;
    if (gain < 0.999f)
    {
      for (std::int16_t& sample : buffer)
      {
        sample = static_cast<std::int16_t>(static_cast<float>(sample) * gain);
      }
    }

    if (!ComputeCardHAL::Audio::WriteOutputFrames(buffer.data(), buffer.size()))
    {
      ResetAudioPumpState();
      vTaskDelete(nullptr);
      return;
    }
  }
}

bool EnsureAudioPumpStarted()
{
  std::lock_guard<std::mutex> lock(g_audioPumpMutex);

  if (g_audioPumpTask != nullptr)
  {
    return true;
  }
  if (!ComputeCardHAL::Audio::Init() || !ComputeCardHAL::Audio::IsOutputReady())
  {
    return false;
  }

  TaskHandle_t taskHandle = nullptr;
  BaseType_t created = pdFAIL;
#if portNUM_PROCESSORS > 1
  created = xTaskCreatePinnedToCore(AudioRenderTask, "synth_audio", 4096, nullptr, kAudioTaskPriority, &taskHandle, kAudioTaskPinnedCore);
#else
  created = xTaskCreate(AudioRenderTask, "synth_audio", 4096, nullptr, kAudioTaskPriority, &taskHandle);
#endif
  if (created != pdPASS)
  {
    return false;
  }

  g_audioPumpTask = taskHandle;
  return true;
}

#else

bool EnsureAudioPumpStarted()
{
  return ComputeCardHAL::Audio::IsOutputReady();
}

#endif

}  // namespace

SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex) {
  SynthAudioPipelineBinding binding;
  binding.engineIndex = engineIndex;
  binding.backendName = "computecard-i2s";

  const bool status = ComputeCardHAL::Audio::Init();
  binding.hasNativeOutput = status && EnsureAudioPumpStarted();
  binding.connected = binding.hasNativeOutput;
  return binding;
}

}  // namespace device
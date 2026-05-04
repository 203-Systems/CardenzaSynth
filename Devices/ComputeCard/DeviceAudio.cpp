#include "Devices/DeviceAudio.h"

#include <array>
#include <cstdint>
#include <mutex>

#include "Devices/ComputeCard/main/hal/computecard/audio/audio.h"
#include "Library/Synth/SynthAudioRuntime.h"

#if __has_include(<driver/i2s_std.h>) && __has_include(<freertos/FreeRTOS.h>) && __has_include(<freertos/task.h>)
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace device {

namespace {

#if __has_include(<driver/i2s_std.h>) && __has_include(<freertos/FreeRTOS.h>) && __has_include(<freertos/task.h>)

constexpr std::size_t kRenderFrameCount = 256;
std::mutex g_audioPumpMutex;
TaskHandle_t g_audioPumpTask = nullptr;

void ResetAudioPumpState()
{
  std::lock_guard<std::mutex> lock(g_audioPumpMutex);
  g_audioPumpTask = nullptr;
}

void AudioRenderTask(void*)
{
  i2s_chan_handle_t channel = ComputeCardHAL::Audio::GetAudioChannel();
  if (channel == nullptr)
  {
    ResetAudioPumpState();
    vTaskDelete(nullptr);
    return;
  }

  if (i2s_channel_enable(channel) != ESP_OK)
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

    std::size_t bytesWritten = 0;
    (void)i2s_channel_write(channel,
                            buffer.data(),
                            buffer.size() * sizeof(std::int16_t),
                            &bytesWritten,
                            portMAX_DELAY);
  }
}

bool EnsureAudioPumpStarted()
{
  std::lock_guard<std::mutex> lock(g_audioPumpMutex);

  if (g_audioPumpTask != nullptr)
  {
    return true;
  }
  if (ComputeCardHAL::Audio::Init() != ESP_OK || ComputeCardHAL::Audio::GetAudioChannel() == nullptr)
  {
    return false;
  }

  TaskHandle_t taskHandle = nullptr;
  if (xTaskCreate(AudioRenderTask, "synth_audio", 4096, nullptr, 5, &taskHandle) != pdPASS)
  {
    return false;
  }

  g_audioPumpTask = taskHandle;
  return true;
}

#else

bool EnsureAudioPumpStarted()
{
  return ComputeCardHAL::Audio::GetAudioChannel() != nullptr;
}

#endif

}  // namespace

SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex) {
  SynthAudioPipelineBinding binding;
  binding.engineIndex = engineIndex;
  binding.backendName = "computecard-i2s";

  const esp_err_t status = ComputeCardHAL::Audio::Init();
  binding.hasNativeOutput = status == ESP_OK && EnsureAudioPumpStarted();
  binding.connected = binding.hasNativeOutput;
  return binding;
}

}  // namespace device
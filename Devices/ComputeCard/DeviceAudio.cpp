#include "Devices/DeviceAudio.h"

#include <array>
#include <cstdint>
#include <mutex>

#include "audio/audio.h"
#include "Library/Synth/SynthAudioRuntime.h"

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace device {

namespace {

#if defined(ESP_PLATFORM)

constexpr std::size_t kRenderFrameCount = 256;
// Audio stays on core 1 so it does not contend with runtime/UI and keyboard work on core 0.
constexpr BaseType_t kAudioTaskPinnedCore = 1;
constexpr UBaseType_t kAudioTaskPriority = 6;
// AMY's ESP render/fill tasks reserve 12-16 KiB; match that headroom here because
// polyphonic render paths can consume substantially more stack than single-note cases.
constexpr uint32_t kAudioTaskStackSize = 16 * 1024;
constexpr char kLogTag[] = "ComputeCardAudio";
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
    ESP_LOGE(kLogTag, "failed to enable I2S output");
    ResetAudioPumpState();
    vTaskDelete(nullptr);
    return;
  }

  if (!ComputeCardHAL::Audio::NotifyOutputClockStarted())
  {
    ESP_LOGE(kLogTag, "codec/output clock notification failed");
    ResetAudioPumpState();
    vTaskDelete(nullptr);
    return;
  }

  std::array<std::int16_t, kRenderFrameCount> buffer{};
  std::uint32_t renderedBlocks = 0;
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
      ESP_LOGE(kLogTag, "I2S write failed after %" PRIu32 " rendered blocks", renderedBlocks);
      ResetAudioPumpState();
      vTaskDelete(nullptr);
      return;
    }

    ++renderedBlocks;
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
    ESP_LOGW(kLogTag, "audio output not ready");
    return false;
  }

  TaskHandle_t taskHandle = nullptr;
  BaseType_t created = pdFAIL;
#if portNUM_PROCESSORS > 1
  created = xTaskCreatePinnedToCore(AudioRenderTask, "synth_audio", kAudioTaskStackSize, nullptr, kAudioTaskPriority, &taskHandle, kAudioTaskPinnedCore);
#else
  created = xTaskCreate(AudioRenderTask, "synth_audio", kAudioTaskStackSize, nullptr, kAudioTaskPriority, &taskHandle);
#endif
  if (created != pdPASS)
  {
    ESP_LOGE(kLogTag, "failed to create synth_audio task");
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

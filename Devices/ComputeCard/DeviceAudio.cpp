#include "Devices/Device.h"
#include "Devices/DeviceAudio.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <mutex>

#include "audio/audio.h"
#include "computecard/mic/mic.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

device::SynthAudioRenderProvider g_audioProvider{};
std::mutex g_audioProviderMutex;
std::mutex g_audioOutputMutex;
std::mutex g_audioInputMutex;
bool g_audioOutputEnabled = false;
bool g_audioInputEnabled = false;

device::SynthAudioRenderProvider CurrentAudioProvider()
{
  std::lock_guard<std::mutex> lock(g_audioProviderMutex);
  return g_audioProvider;
}

std::size_t RenderSynthFrames(std::int16_t* destination, std::size_t frameCount)
{
  const device::SynthAudioRenderProvider provider = CurrentAudioProvider();
  if (provider.renderFrames == nullptr)
  {
    std::fill(destination, destination + frameCount, 0);
    return frameCount;
  }

  return provider.renderFrames(destination, frameCount);
}

constexpr std::size_t kRenderFrameCount = 256;
constexpr BaseType_t kAudioTaskPinnedCore = 1;
constexpr UBaseType_t kAudioTaskPriority = 6;
constexpr uint32_t kAudioTaskStackSize = 16 * 1024;
constexpr char kLogTag[] = "ComputeCardAudio";
std::mutex g_audioPumpMutex;
TaskHandle_t g_audioPumpTask = nullptr;

bool EnsureOutputChannelEnabled()
{
  const i2s_chan_handle_t outputChannel = ComputeCardHAL::Audio::GetTxChannel();
  if (outputChannel == nullptr)
  {
    return false;
  }

  std::lock_guard<std::mutex> lock(g_audioOutputMutex);
  if (g_audioOutputEnabled)
  {
    return true;
  }

  if (i2s_channel_enable(outputChannel) != ESP_OK)
  {
    return false;
  }

  g_audioOutputEnabled = true;
  (void)ComputeCardHAL::Audio::SetVolume(ComputeCardHAL::Audio::GetVolume());
  return true;
}

bool WriteOutputFrames(const std::int16_t* samples, std::size_t sampleCount)
{
  if (samples == nullptr || sampleCount == 0)
  {
    return false;
  }

  if (!EnsureOutputChannelEnabled())
  {
    return false;
  }

  std::size_t bytesWritten = 0;
  return i2s_channel_write(ComputeCardHAL::Audio::GetTxChannel(),
                           samples,
                           sampleCount * sizeof(std::int16_t),
                           &bytesWritten,
                           portMAX_DELAY) == ESP_OK;
}

void ResetAudioPumpState()
{
  std::lock_guard<std::mutex> lock(g_audioPumpMutex);
  g_audioPumpTask = nullptr;
}

void AudioRenderTask(void*)
{
  if (!EnsureOutputChannelEnabled())
  {
    ESP_LOGE(kLogTag, "failed to enable I2S output");
    ResetAudioPumpState();
    vTaskDelete(nullptr);
    return;
  }

  std::array<std::int16_t, kRenderFrameCount> buffer{};
  std::uint32_t renderedBlocks = 0;
  for (;;)
  {
    RenderSynthFrames(buffer.data(), buffer.size());
    const float gain = static_cast<float>(ComputeCardHAL::Audio::GetVolume()) / 100.0f;
    if (gain < 0.999f)
    {
      for (std::int16_t& sample : buffer)
      {
        sample = static_cast<std::int16_t>(static_cast<float>(sample) * gain);
      }
    }

    if (!WriteOutputFrames(buffer.data(), buffer.size()))
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
  if (!ComputeCardHAL::Audio::Init() || ComputeCardHAL::Audio::GetTxChannel() == nullptr)
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

}  // namespace

namespace Device::Audio {

InputStreamInfo InputInfo()
{
  return InputStreamInfo{kDefaultInputSampleRate, kInputChannels, kInputBitsPerSample};
}

InputStatus StartInput()
{
  const i2s_chan_handle_t inputChannel = ComputeCardHAL::Mic::InputChannel();
  if (inputChannel == nullptr)
  {
    return InputStatus::NotReady;
  }

  std::lock_guard<std::mutex> lock(g_audioInputMutex);
  if (g_audioInputEnabled)
  {
    return InputStatus::Ready;
  }

  if (i2s_channel_enable(inputChannel) != ESP_OK)
  {
    return InputStatus::NotReady;
  }

  g_audioInputEnabled = true;
  return InputStatus::Ready;
}

void StopInput()
{
  std::lock_guard<std::mutex> lock(g_audioInputMutex);
  if (!g_audioInputEnabled)
  {
    return;
  }

  if (const i2s_chan_handle_t inputChannel = ComputeCardHAL::Mic::InputChannel(); inputChannel != nullptr)
  {
    (void)i2s_channel_disable(inputChannel);
  }
  g_audioInputEnabled = false;
}

InputStatus ReadInputFrames(std::int16_t* destination,
                            std::size_t frameCount,
                            std::size_t& framesRead,
                            std::uint32_t timeoutMs)
{
  framesRead = 0;
  if (destination == nullptr && frameCount != 0)
  {
    return InputStatus::InvalidArgument;
  }

  if (frameCount == 0)
  {
    return InputStatus::Ready;
  }

  {
    std::lock_guard<std::mutex> lock(g_audioInputMutex);
    if (!g_audioInputEnabled)
    {
      return InputStatus::NotReady;
    }
  }

  const i2s_chan_handle_t inputChannel = ComputeCardHAL::Mic::InputChannel();
  if (inputChannel == nullptr)
  {
    return InputStatus::NotReady;
  }

  std::size_t bytesRead = 0;
  if (i2s_channel_read(inputChannel,
                       destination,
                       frameCount * sizeof(std::int16_t),
                       &bytesRead,
                       timeoutMs) != ESP_OK)
  {
    return InputStatus::NotReady;
  }

  framesRead = bytesRead / sizeof(std::int16_t);
  return InputStatus::Ready;
}

}  // namespace Device::Audio

namespace device {

void SetSynthAudioRenderProvider(SynthAudioRenderProvider provider)
{
  std::lock_guard<std::mutex> lock(g_audioProviderMutex);
  g_audioProvider = provider;
}

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

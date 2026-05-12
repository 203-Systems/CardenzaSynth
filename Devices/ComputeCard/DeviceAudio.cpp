#include "Devices/Device.h"
#include "Devices/DeviceAudio.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <mutex>

#include "audio/audio.h"
#include "computecard/mic/mic.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

device::SynthAudioRenderProvider g_audioProvider{};
std::atomic<device::SynthAudioRenderCallback> g_audioRenderCallback{nullptr};
std::mutex g_audioProviderMutex;
std::mutex g_audioOutputMutex;
std::mutex g_audioInputMutex;
bool g_audioOutputEnabled = false;
bool g_audioInputEnabled = false;

std::size_t RenderSynthFrames(std::int16_t* destination, std::size_t frameCount)
{
  const device::SynthAudioRenderCallback renderFrames = g_audioRenderCallback.load(std::memory_order_acquire);
  if (renderFrames == nullptr)
  {
    std::fill(destination, destination + frameCount, 0);
    return frameCount;
  }

  return renderFrames(destination, frameCount);
}

constexpr std::size_t kRenderFrameCount = 256;
constexpr BaseType_t kAudioTaskPinnedCore = 1;
constexpr UBaseType_t kAudioTaskPriority = 6;
constexpr uint32_t kAudioTaskStackSize = 16 * 1024;
constexpr int64_t kRenderBudgetUs =
  static_cast<int64_t>((1000000ULL * kRenderFrameCount) / device::kDefaultSynthAudioSampleRate);
constexpr uint32_t kTimingLogBlockInterval = 1024;
constexpr std::int32_t kVolumeScaleUnity = 32768;
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

void ApplyVolume(std::int16_t* samples, std::size_t sampleCount, std::uint8_t volume)
{
  if (volume >= 100)
  {
    return;
  }

  const std::int32_t gain = (static_cast<std::int32_t>(volume) * kVolumeScaleUnity + 50) / 100;
  for (std::size_t index = 0; index < sampleCount; ++index)
  {
    samples[index] = static_cast<std::int16_t>((static_cast<std::int32_t>(samples[index]) * gain) / kVolumeScaleUnity);
  }
}

bool WriteOutputFrames(i2s_chan_handle_t outputChannel, const std::int16_t* samples, std::size_t sampleCount)
{
  if (outputChannel == nullptr || samples == nullptr || sampleCount == 0)
  {
    return false;
  }

  std::size_t bytesWritten = 0;
  return i2s_channel_write(outputChannel,
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

  const i2s_chan_handle_t outputChannel = ComputeCardHAL::Audio::GetTxChannel();
  if (outputChannel == nullptr)
  {
    ESP_LOGE(kLogTag, "I2S output channel missing");
    ResetAudioPumpState();
    vTaskDelete(nullptr);
    return;
  }

  std::array<std::int16_t, kRenderFrameCount> buffer{};
  std::uint32_t renderedBlocks = 0;
  int64_t maxRenderUs = 0;
  int64_t maxWriteUs = 0;
  std::uint32_t overBudgetRenderCount = 0;
  for (;;)
  {
    const int64_t renderStartUs = esp_timer_get_time();
    RenderSynthFrames(buffer.data(), buffer.size());
    const int64_t renderEndUs = esp_timer_get_time();
    ApplyVolume(buffer.data(), buffer.size(), ComputeCardHAL::Audio::GetVolume());

    const int64_t writeStartUs = esp_timer_get_time();

    if (!WriteOutputFrames(outputChannel, buffer.data(), buffer.size()))
    {
      ESP_LOGE(kLogTag, "I2S write failed after %" PRIu32 " rendered blocks", renderedBlocks);
      ResetAudioPumpState();
      vTaskDelete(nullptr);
      return;
    }

    const int64_t writeEndUs = esp_timer_get_time();
    const int64_t renderUs = renderEndUs - renderStartUs;
    const int64_t writeUs = writeEndUs - writeStartUs;
    maxRenderUs = (std::max)(maxRenderUs, renderUs);
    maxWriteUs = (std::max)(maxWriteUs, writeUs);
    if (renderUs > kRenderBudgetUs)
    {
      ++overBudgetRenderCount;
      if (overBudgetRenderCount == 1 || (overBudgetRenderCount % 16) == 0)
      {
        ESP_LOGW(kLogTag,
                 "audio render over budget: render=%" PRId64 "us budget=%" PRId64 "us write=%" PRId64 "us block=%" PRIu32 " overruns=%" PRIu32,
                 renderUs,
                 kRenderBudgetUs,
                 writeUs,
                 renderedBlocks,
                 overBudgetRenderCount);
      }
    }

    ++renderedBlocks;
    if ((renderedBlocks % kTimingLogBlockInterval) == 0)
    {
      ESP_LOGI(kLogTag,
               "audio timing: blocks=%" PRIu32 " max_render=%" PRId64 "us max_write=%" PRId64 "us budget=%" PRId64 "us overruns=%" PRIu32,
               renderedBlocks,
               maxRenderUs,
               maxWriteUs,
               kRenderBudgetUs,
               overBudgetRenderCount);
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
  g_audioRenderCallback.store(provider.renderFrames, std::memory_order_release);
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

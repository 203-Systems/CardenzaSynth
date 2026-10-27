#include "Devices/Device.h"
#include "Devices/DeviceAudio.h"
#include "Devices/AudioOutputTransfer.h"
#include "Devices/Cardenza/AudioDiagnostics.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <mutex>

#include "audio/audio.h"
#include "mic/mic.h"

#include <esp_log.h>
#include <esp_attr.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace {

device::SynthAudioRenderProvider g_audioProvider{};
std::atomic<device::SynthAudioRenderCallback> g_audioRenderCallback{nullptr};
std::mutex g_audioProviderMutex;
std::mutex g_audioOutputMutex;
std::mutex g_audioInputMutex;
std::mutex g_audioInputDataMutex;
std::atomic<bool> g_audioInputEnabled{false};
std::atomic<bool> g_audioInputOverflow{false};
constexpr std::size_t kInputRingFrames = 16384;
std::array<std::int16_t, kInputRingFrames> g_audioInputRing{};
std::size_t g_audioInputHead = 0;
std::size_t g_audioInputCount = 0;
TaskHandle_t g_audioInputTask = nullptr;

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
constexpr std::size_t kAudioBufferCount = 4;
constexpr std::size_t kStartupBufferCount = 3;
constexpr std::size_t kWriteChunkFrames = 128;
constexpr UBaseType_t kRenderTaskPriority = 10;
constexpr UBaseType_t kWriterTaskPriority = 12;
// ESP-IDF stack sizes are bytes. Keep the existing AMY stack allowance.
constexpr uint32_t kRenderTaskStackBytes = 16 * 1024;
constexpr uint32_t kWriterTaskStackBytes = 4096;
constexpr uint32_t kWriteTimeoutMs = 20;
constexpr int64_t kRenderBudgetUs =
  static_cast<int64_t>((1000000ULL * kRenderFrameCount) / device::kDefaultSynthAudioSampleRate);
constexpr char kLogTag[] = "CardenzaAudio";
static_assert(configTICK_RATE_HZ == 1000, "Audio queue waits require a 1 ms RTOS tick");
static_assert(std::atomic<uint32_t>::is_always_lock_free, "Audio counters must be lock-free");

std::mutex g_audioPumpMutex;
TaskHandle_t g_audioPumpTask = nullptr;
TaskHandle_t g_audioWriterTask = nullptr;
std::array<std::array<std::int16_t, kRenderFrameCount>, kAudioBufferCount> g_audioBuffers{};
StaticQueue_t g_freeQueueStorage{};
StaticQueue_t g_filledQueueStorage{};
std::array<uint8_t, kAudioBufferCount> g_freeQueueItems{};
std::array<uint8_t, kAudioBufferCount> g_filledQueueItems{};
QueueHandle_t g_freeQueue = nullptr;
QueueHandle_t g_filledQueue = nullptr;
std::atomic<bool> g_audioPumpStarted{false};
uint32_t g_dmaBytes = 0;

struct AudioCounters {
  std::atomic<uint32_t> renderedBlocks{0};
  std::atomic<uint32_t> maxRenderUs{0};
  std::atomic<uint32_t> maxWriteUs{0};
  std::atomic<uint32_t> renderDeadlineMisses{0};
  std::atomic<uint32_t> producerWaits{0};
  std::atomic<uint32_t> shortRenders{0};
  std::atomic<uint32_t> shortWrites{0};
  std::atomic<uint32_t> writeFailures{0};
  std::atomic<uint32_t> txQueueOverflows{0};
  std::atomic<uint32_t> lastI2sError{ESP_OK};
  std::atomic<uint32_t> running{0};
} g_audioCounters;

void RecordMaximum(std::atomic<uint32_t>& counter, int64_t elapsedUs)
{
  const auto elapsed = static_cast<uint32_t>((std::min)(elapsedUs, int64_t{UINT32_MAX}));
  // Each maximum has exactly one writer; diagnostics only loads it.
  if (elapsed > counter.load(std::memory_order_relaxed))
    counter.store(elapsed, std::memory_order_relaxed);
}

bool IRAM_ATTR OutputQueueOverflow(i2s_chan_handle_t, i2s_event_data_t*, void*)
{
  // This is a TX service-delay signal, not an exact count of audible underruns.
  g_audioCounters.txQueueOverflows.fetch_add(1, std::memory_order_relaxed);
  return false;
}

bool PrepareOutputChannel(i2s_chan_handle_t channel)
{
  std::lock_guard<std::mutex> lock(g_audioOutputMutex);
  // The codec needs clocks during initialization. Run zeroed DMA while setting it
  // up, then stop before preloading real PCM. I2C must never delay the writer.
  if (i2s_channel_enable(channel) != ESP_OK)
    return false;
  const bool volumeReady = CardenzaHAL::Audio::SetVolume(CardenzaHAL::Audio::GetVolume());
  const esp_err_t disabled = i2s_channel_disable(channel);
  if (!volumeReady || disabled != ESP_OK)
    return false;

  i2s_chan_info_t info{};
  if (i2s_channel_get_info(channel, &info) != ESP_OK || info.total_dma_buf_size == 0)
    return false;
  g_dmaBytes = info.total_dma_buf_size;
  i2s_event_callbacks_t callbacks{};
  callbacks.on_send_q_ovf = OutputQueueOverflow;
  return i2s_channel_register_event_callback(channel, &callbacks, nullptr) == ESP_OK;
}

void AudioRenderTask(void*)
{
  // Both task handles and queues must exist before either worker can run.
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  std::size_t primedBlocks = 0;
  for (;;)
  {
    uint8_t index = 0;
    xQueueReceive(g_freeQueue, &index, portMAX_DELAY);
    auto& buffer = g_audioBuffers[index];
    const int64_t startUs = esp_timer_get_time();
    const std::size_t rendered = RenderSynthFrames(buffer.data(), buffer.size());
    if (rendered < buffer.size())
    {
      std::fill(buffer.begin() + rendered, buffer.end(), 0);
      g_audioCounters.shortRenders.fetch_add(1, std::memory_order_relaxed);
    }
    const int64_t renderUs = esp_timer_get_time() - startUs;
    RecordMaximum(g_audioCounters.maxRenderUs, renderUs);
    if (renderUs > kRenderBudgetUs)
      g_audioCounters.renderDeadlineMisses.fetch_add(1, std::memory_order_relaxed);
    g_audioCounters.renderedBlocks.fetch_add(1, std::memory_order_relaxed);
    xQueueSend(g_filledQueue, &index, portMAX_DELAY);
    if (primedBlocks < kStartupBufferCount && ++primedBlocks == kStartupBufferCount)
      xTaskNotifyGive(g_audioWriterTask);
  }
}

void WriteOutputFrames(i2s_chan_handle_t channel, const std::int16_t* samples, std::size_t frames)
{
  std::size_t offset = 0;
  while (offset < frames)
  {
    const int64_t startUs = esp_timer_get_time();
    const int64_t deadlineUs = startUs + kWriteTimeoutMs * 1000;
    // Small chunks limit each native call to at most one DMA segment boundary.
    // The native timeout is in milliseconds, not FreeRTOS ticks.
    const std::size_t chunkFrames = (std::min)(frames - offset, kWriteChunkFrames);
    const auto result = device::audio::WriteAllPcm(samples + offset, chunkFrames,
      [&](const void* data, std::size_t bytes) -> device::audio::WriteAttempt {
        const int64_t remainingUs = deadlineUs - esp_timer_get_time();
        if (remainingUs <= 0)
        {
          g_audioCounters.lastI2sError.store(ESP_ERR_TIMEOUT, std::memory_order_relaxed);
          return {0, false};
        }
        std::size_t written = 0;
        const esp_err_t status = i2s_channel_write(channel, data, bytes, &written,
          static_cast<uint32_t>((remainingUs + 999) / 1000));
        if (status != ESP_OK)
          g_audioCounters.lastI2sError.store(static_cast<uint32_t>(status), std::memory_order_relaxed);
        return {written, status == ESP_OK || status == ESP_ERR_TIMEOUT};
      });
    RecordMaximum(g_audioCounters.maxWriteUs, esp_timer_get_time() - startUs);
    g_audioCounters.shortWrites.fetch_add(result.shortWrites, std::memory_order_relaxed);
    offset += result.bytesWritten / sizeof(std::int16_t);
    if (!result.complete)
    {
      g_audioCounters.writeFailures.fetch_add(1, std::memory_order_relaxed);
      // Keep ownership of the unwritten tail. A transient timeout must not skip
      // PCM or return a buffer that the producer could overwrite. Yield even on
      // an immediate driver error, so UI and recovery remain responsive.
      vTaskDelay(1);
    }
  }
}

void AudioWriterTask(void* parameter)
{
  const auto channel = static_cast<i2s_chan_handle_t>(parameter);
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

  uint8_t index = 0;
  std::size_t consumedFrames = 0;
  // Fill native DMA before enabling it. A software block can straddle the end
  // of DMA, so retain its suffix for the first normal write.
  for (;;)
  {
    xQueueReceive(g_filledQueue, &index, portMAX_DELAY);
    const auto& buffer = g_audioBuffers[index];
    std::size_t loadedBytes = 0;
    const esp_err_t status = i2s_channel_preload_data(channel, buffer.data(),
      buffer.size() * sizeof(std::int16_t), &loadedBytes);
    if (loadedBytes > buffer.size() * sizeof(std::int16_t) || loadedBytes % sizeof(std::int16_t) != 0)
      loadedBytes = 0;
    consumedFrames = loadedBytes / sizeof(std::int16_t);
    if (status != ESP_OK)
    {
      g_audioCounters.lastI2sError.store(static_cast<uint32_t>(status), std::memory_order_relaxed);
      g_audioCounters.writeFailures.fetch_add(1, std::memory_order_relaxed);
    }
    if (status != ESP_OK || consumedFrames < buffer.size())
      break;
    xQueueSend(g_freeQueue, &index, portMAX_DELAY);
  }

  esp_err_t status;
  while ((status = i2s_channel_enable(channel)) != ESP_OK)
  {
    g_audioCounters.lastI2sError.store(static_cast<uint32_t>(status), std::memory_order_relaxed);
    g_audioCounters.writeFailures.fetch_add(1, std::memory_order_relaxed);
    vTaskDelay(pdMS_TO_TICKS(kWriteTimeoutMs));
  }
  g_audioCounters.running.store(1, std::memory_order_relaxed);
  WriteOutputFrames(channel, g_audioBuffers[index].data() + consumedFrames, kRenderFrameCount - consumedFrames);
  xQueueSend(g_freeQueue, &index, portMAX_DELAY);

  for (;;)
  {
    if (xQueueReceive(g_filledQueue, &index, pdMS_TO_TICKS(2)) != pdTRUE)
    {
      g_audioCounters.producerWaits.fetch_add(1, std::memory_order_relaxed);
      // Empty software buffers do not mean native DMA has run dry. Wait for the
      // next real block instead of inserting silence ahead of arriving PCM.
      // HAL auto_clear supplies zeroes only if a DMA segment actually drains.
      xQueueReceive(g_filledQueue, &index, portMAX_DELAY);
    }
    WriteOutputFrames(channel, g_audioBuffers[index].data(), kRenderFrameCount);
    xQueueSend(g_freeQueue, &index, portMAX_DELAY);
  }
}

bool EnsureAudioPumpStarted()
{
  std::lock_guard<std::mutex> lock(g_audioPumpMutex);
  if (g_audioPumpTask != nullptr)
    return true;
  // Mono: one sample per frame, the HAL sends it to both codec slots.
  if (!CardenzaHAL::Audio::Init(I2S_SLOT_MODE_MONO))
    return false;
  const auto channel = CardenzaHAL::Audio::GetTxChannel();
  if (channel == nullptr || !PrepareOutputChannel(channel))
  {
    ESP_LOGE(kLogTag, "audio output preparation failed");
    return false;
  }

  g_freeQueue = xQueueCreateStatic(kAudioBufferCount, sizeof(uint8_t), g_freeQueueItems.data(), &g_freeQueueStorage);
  g_filledQueue = xQueueCreateStatic(kAudioBufferCount, sizeof(uint8_t), g_filledQueueItems.data(), &g_filledQueueStorage);
  for (uint8_t index = 0; index < kAudioBufferCount; ++index)
    xQueueSend(g_freeQueue, &index, 0);

  BaseType_t created = pdFAIL;
#if portNUM_PROCESSORS > 1
  created = xTaskCreatePinnedToCore(AudioWriterTask, "synth_i2s", kWriterTaskStackBytes, channel,
    kWriterTaskPriority, &g_audioWriterTask, 0);
#else
  created = xTaskCreate(AudioWriterTask, "synth_i2s", kWriterTaskStackBytes, channel,
    kWriterTaskPriority, &g_audioWriterTask);
#endif
  if (created != pdPASS)
  {
    ESP_LOGE(kLogTag, "failed to create synth_i2s task");
    return false;
  }
#if portNUM_PROCESSORS > 1
  created = xTaskCreatePinnedToCore(AudioRenderTask, "synth_render", kRenderTaskStackBytes, nullptr,
    kRenderTaskPriority, &g_audioPumpTask, 1);
#else
  created = xTaskCreate(AudioRenderTask, "synth_render", kRenderTaskStackBytes, nullptr,
    kRenderTaskPriority, &g_audioPumpTask);
#endif
  if (created != pdPASS)
  {
    // The writer is still blocked on its first notification.
    vTaskDelete(g_audioWriterTask);
    g_audioWriterTask = nullptr;
    ESP_LOGE(kLogTag, "failed to create synth_render task");
    return false;
  }
  g_audioPumpStarted.store(true, std::memory_order_release);
  xTaskNotifyGive(g_audioPumpTask);
  return true;
}

bool IRAM_ATTR InputQueueOverflow(i2s_chan_handle_t, i2s_event_data_t*, void*)
{
  g_audioInputOverflow.store(true, std::memory_order_relaxed);
  return false;
}

void AudioInputTask(void*)
{
  std::array<std::int16_t, 256> frames{};
  for (;;)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    while (g_audioInputEnabled.load(std::memory_order_acquire))
    {
      // Serialize native reads with channel enable/disable; UI drains a separate ring.
      std::lock_guard<std::mutex> inputLock(g_audioInputMutex);
      if (!g_audioInputEnabled.load(std::memory_order_relaxed))
      {
        break;
      }
      std::size_t bytesRead = 0;
      const esp_err_t result = i2s_channel_read(CardenzaHAL::Mic::InputChannel(),
          frames.data(), frames.size() * sizeof(std::int16_t), &bytesRead, 20);
      if (result != ESP_OK && result != ESP_ERR_TIMEOUT)
      {
        g_audioInputOverflow.store(true, std::memory_order_relaxed);
      }
      const std::size_t count = bytesRead / sizeof(std::int16_t);
      std::lock_guard<std::mutex> dataLock(g_audioInputDataMutex);
      if (count > kInputRingFrames - g_audioInputCount)
      {
        g_audioInputOverflow.store(true, std::memory_order_relaxed);
        continue;
      }
      for (std::size_t i = 0; i < count; ++i)
      {
        g_audioInputRing[(g_audioInputHead + g_audioInputCount + i) % kInputRingFrames] = frames[i];
      }
      g_audioInputCount += count;
    }
  }
}

}  // namespace

namespace Device::Audio {

void SetOutputVolume(std::uint8_t percent)
{
  // Hardware volume is applied once in the codec; track gain belongs to the DSP.
  std::lock_guard<std::mutex> lock(g_audioOutputMutex);
  (void)CardenzaHAL::Audio::SetVolume((std::min)(percent, std::uint8_t{100}));
}

std::uint8_t OutputVolume()
{
  return CardenzaHAL::Audio::GetVolume();
}

InputStreamInfo InputInfo()
{
  return InputStreamInfo{kDefaultInputSampleRate, kInputChannels, kInputBitsPerSample};
}

InputStatus StartInput()
{
  const i2s_chan_handle_t inputChannel = CardenzaHAL::Mic::InputChannel();
  if (inputChannel == nullptr)
  {
    return InputStatus::NotReady;
  }

  std::lock_guard<std::mutex> lock(g_audioInputMutex);
  if (g_audioInputEnabled)
  {
    return InputStatus::Ready;
  }

  if (g_audioInputTask == nullptr)
  {
    i2s_event_callbacks_t callbacks{};
    callbacks.on_recv_q_ovf = InputQueueOverflow;
    if (i2s_channel_register_event_callback(inputChannel, &callbacks, nullptr) != ESP_OK ||
        xTaskCreate(AudioInputTask, "sample_input", 4096, nullptr, 7, &g_audioInputTask) != pdPASS)
    {
      return InputStatus::NotReady;
    }
  }
  {
    std::lock_guard<std::mutex> dataLock(g_audioInputDataMutex);
    g_audioInputHead = 0;
    g_audioInputCount = 0;
    g_audioInputOverflow.store(false, std::memory_order_relaxed);
  }
  if (i2s_channel_enable(inputChannel) != ESP_OK)
  {
    return InputStatus::NotReady;
  }

  g_audioInputEnabled.store(true, std::memory_order_release);
  xTaskNotifyGive(g_audioInputTask);
  return InputStatus::Ready;
}

void StopInput()
{
  // Clear before waiting on the read mutex, so the higher-priority reader exits.
  if (!g_audioInputEnabled.exchange(false, std::memory_order_acq_rel))
  {
    return;
  }
  std::lock_guard<std::mutex> lock(g_audioInputMutex);

  if (const i2s_chan_handle_t inputChannel = CardenzaHAL::Mic::InputChannel(); inputChannel != nullptr)
  {
    (void)i2s_channel_disable(inputChannel);
  }
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

  const int64_t deadline = esp_timer_get_time() + static_cast<int64_t>(timeoutMs) * 1000;
  for (;;)
  {
    {
      std::lock_guard<std::mutex> dataLock(g_audioInputDataMutex);
      if (g_audioInputOverflow.exchange(false, std::memory_order_relaxed))
      {
        ESP_LOGW(kLogTag, "microphone input overflow; recording must stop");
        return InputStatus::Overflow;
      }
      framesRead = std::min(frameCount, g_audioInputCount);
      for (std::size_t i = 0; i < framesRead; ++i)
      {
        destination[i] = g_audioInputRing[(g_audioInputHead + i) % kInputRingFrames];
      }
      g_audioInputHead = (g_audioInputHead + framesRead) % kInputRingFrames;
      g_audioInputCount -= framesRead;
    }
    if (framesRead != 0 || timeoutMs == 0 || esp_timer_get_time() >= deadline)
    {
      return InputStatus::Ready;
    }
    if (!g_audioInputEnabled.load(std::memory_order_acquire))
    {
      return InputStatus::NotReady;
    }
    vTaskDelay(1);
  }
}

}  // namespace Device::Audio

namespace device {

void PollAudioDiagnostics()
{
  static int64_t lastLogUs = 0;
  const int64_t nowUs = esp_timer_get_time();
  if (!g_audioPumpStarted.load(std::memory_order_acquire) || nowUs - lastLogUs < 5000000)
    return;
  lastLogUs = nowUs;
  ESP_LOGI(kLogTag,
    "audio: blocks=%" PRIu32 " render_max=%" PRIu32 "us budget=%" PRId64
    "us misses=%" PRIu32 " write_max=%" PRIu32 "us producer_waits=%" PRIu32
    " short_render=%" PRIu32 " short_write=%" PRIu32 " write_fail=%" PRIu32
    " tx_q_overflow=%" PRIu32 " last_error=0x%" PRIx32 " running=%" PRIu32
    " ready=%u dma_bytes=%" PRIu32,
    g_audioCounters.renderedBlocks.load(std::memory_order_relaxed),
    g_audioCounters.maxRenderUs.load(std::memory_order_relaxed), kRenderBudgetUs,
    g_audioCounters.renderDeadlineMisses.load(std::memory_order_relaxed),
    g_audioCounters.maxWriteUs.load(std::memory_order_relaxed),
    g_audioCounters.producerWaits.load(std::memory_order_relaxed),
    g_audioCounters.shortRenders.load(std::memory_order_relaxed),
    g_audioCounters.shortWrites.load(std::memory_order_relaxed),
    g_audioCounters.writeFailures.load(std::memory_order_relaxed),
    g_audioCounters.txQueueOverflows.load(std::memory_order_relaxed),
    g_audioCounters.lastI2sError.load(std::memory_order_relaxed),
    g_audioCounters.running.load(std::memory_order_relaxed),
    static_cast<unsigned>(uxQueueMessagesWaiting(g_filledQueue)), g_dmaBytes);
}

void SetSynthAudioRenderProvider(SynthAudioRenderProvider provider)
{
  std::lock_guard<std::mutex> lock(g_audioProviderMutex);
  g_audioProvider = provider;
  g_audioRenderCallback.store(provider.renderFrames, std::memory_order_release);
}

SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex) {
  SynthAudioPipelineBinding binding;
  binding.engineIndex = engineIndex;
  binding.backendName = "cardenza-i2s";

  const bool status = CardenzaHAL::Audio::Init(I2S_SLOT_MODE_MONO);
  binding.hasNativeOutput = status && EnsureAudioPumpStarted();
  binding.connected = binding.hasNativeOutput;
  return binding;
}

}  // namespace device

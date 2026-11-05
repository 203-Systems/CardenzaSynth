#include "Devices/Device.h"
#include "Devices/DeviceAudio.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>

#include "audio/audio.h"
#include "usb/usb.h"
#include "Devices/Cardenza/DeviceUsbStats.h"
#include "mic/mic.h"

#include <esp_log.h>
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

device::SynthAudioRenderProvider g_audioProvider{};
std::atomic<device::SynthAudioRenderCallback> g_audioRenderCallback{nullptr};
std::mutex g_audioProviderMutex;
std::mutex g_audioOutputMutex;
std::mutex g_audioInputMutex;
std::mutex g_audioInputDataMutex;
bool g_audioOutputEnabled = false;
// GPIO43 is the speaker's WS and the microphone's PDM clock, so they take turns: while the input
// runs the TX channel is off, the codec in standby and rendered blocks go nowhere.
bool g_audioOutputPaused = false;
std::atomic<bool> g_audioInputEnabled{false};
std::atomic<bool> g_audioInputOverflow{false};
// Exists only while the input runs. The UI drains it every frame and writes the take to the card,
// which can stall: up to 372 ms of audio, less when the memory is not there.
constexpr std::array<std::size_t, 3> kInputRingSizes{{16384, 8192, 4096}};
std::size_t g_audioInputRingFrames = 0;
std::unique_ptr<std::int16_t[]> g_audioInputRing;
std::size_t g_audioInputHead = 0;
std::size_t g_audioInputCount = 0;
TaskHandle_t g_audioInputTask = nullptr;

// The rendered sound also goes to a computer recording from the USB port, before the device volume.
std::atomic<std::uint32_t> g_framesToComputer{0};
std::atomic<std::uint32_t> g_framesFromComputer{0};
std::atomic<std::uint16_t> g_peakFromComputer{0};
std::atomic<std::uint16_t> g_peakToComputer{0};  // since the last console line

void SendToComputer(const std::int16_t* mono, std::size_t frameCount)
{
  if (!CardenzaHAL::USB::AudioInputActive()) return;
  g_framesToComputer.fetch_add(static_cast<std::uint32_t>(frameCount), std::memory_order_relaxed);
  int peak = g_peakToComputer.load(std::memory_order_relaxed);
  for (std::size_t frame = 0; frame < frameCount; ++frame) peak = (std::max)(peak, std::abs(static_cast<int>(mono[frame])));
  g_peakToComputer.store(static_cast<std::uint16_t>((std::min)(peak, 32767)), std::memory_order_relaxed);
  std::array<std::int16_t, 2 * 256> stereo;
  for (std::size_t done = 0; done < frameCount;)
  {
    const std::size_t count = (std::min)(frameCount - done, stereo.size() / 2);
    for (std::size_t frame = 0; frame < count; ++frame) stereo[2 * frame] = stereo[2 * frame + 1] = mono[done + frame];
    (void)CardenzaHAL::USB::AudioWrite(stereo.data(), count);
    done += count;
  }
}

// Device::Audio::Timing: written by the output task only.
std::atomic<std::uint32_t> g_timingBlocks{0};
// 32 bits: a 64-bit atomic is a library call behind a lock both cores share with interrupts off.
std::atomic<std::uint32_t> g_timingRenderUs{0};
std::atomic<std::uint32_t> g_timingOverruns{0};
std::atomic<std::uint32_t> g_timingUnderruns{0};
std::atomic<std::uint32_t> g_timingLongestUs{0};

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
constexpr uint32_t kAudioTaskStackSize = 12 * 1024;  // Measured: 7 KB.
constexpr int64_t kRenderBudgetUs =
  static_cast<int64_t>((1000000ULL * kRenderFrameCount) / device::kDefaultSynthAudioSampleRate);
constexpr uint32_t kTimingLogBlockInterval = 1024;
// The timing line is printed when an interval had underruns, the USB line when the computer's audio
// starts, stops or restarts; otherwise both once every this many intervals (about 5 minutes). Every
// line ends up on the card: printed every interval, an idle device wrote the log every 5 s.
constexpr uint32_t kQuietLogIntervals = 50;
// What the output's DMA holds (CardenzaHAL audio: six descriptors of 256 frames), and how long a write
// has to wait to tell that it was full.
constexpr int64_t kOutputQueueUs = 6 * kRenderBudgetUs;
constexpr int64_t kWriteWaitedUs = 300;
constexpr char kLogTag[] = "CardenzaAudio";
std::mutex g_audioPumpMutex;
TaskHandle_t g_audioPumpTask = nullptr;

bool EnsureOutputChannelEnabled()
{
  const i2s_chan_handle_t outputChannel = CardenzaHAL::Audio::GetTxChannel();
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
  return true;
}

bool OutputPaused()
{
  std::lock_guard<std::mutex> lock(g_audioOutputMutex);
  return g_audioOutputPaused;
}

// Gives GPIO43 to the microphone: TX off (a write in progress finishes first), codec in standby.
void PauseOutputForInput()
{
  std::lock_guard<std::mutex> lock(g_audioOutputMutex);
  g_audioOutputPaused = true;
  if (g_audioOutputEnabled)
  {
    (void)i2s_channel_disable(CardenzaHAL::Audio::GetTxChannel());
    g_audioOutputEnabled = false;
  }
  (void)CardenzaHAL::Audio::StandbyCodec();
}

// The speaker's pins were not given back after the microphone: RecoverOutput tries that first.
std::atomic<bool> g_outputPinsPending{false};

// Gives GPIO43 back to the speaker and wakes the codec; the render task enables TX again.
void ResumeOutputAfterInput()
{
  std::lock_guard<std::mutex> lock(g_audioOutputMutex);
  const bool pins = CardenzaHAL::Audio::RestoreOutputPins();
  if (!pins || !CardenzaHAL::Audio::Init(I2S_SLOT_MODE_MONO))
  {
    // Silent until the render task gets it back (RecoverOutput, once a second).
    ESP_LOGW(kLogTag, "audio output not restored after input: trying again");
    g_outputPinsPending.store(!pins, std::memory_order_relaxed);
  }
  g_audioOutputPaused = false;
}

// The speaker cannot play (its codec did not start, or did not wake after the microphone): tried again
// once a second from the render task, which keeps everything else in time meanwhile.
void RecoverOutput(int64_t nowUs, int64_t& nextTryUs)
{
  constexpr int64_t kRecoverIntervalUs = 1000000;
  if (nowUs < nextTryUs) return;
  nextTryUs = nowUs + kRecoverIntervalUs;
  std::lock_guard<std::mutex> lock(g_audioOutputMutex);
  if (g_audioOutputPaused) return;
  if (g_outputPinsPending.load(std::memory_order_relaxed))
  {
    if (!CardenzaHAL::Audio::RestoreOutputPins()) return;
    g_outputPinsPending.store(false, std::memory_order_relaxed);
  }
  if (CardenzaHAL::Audio::Init(I2S_SLOT_MODE_MONO) && CardenzaHAL::Audio::GetTxChannel() != nullptr)
    ESP_LOGI(kLogTag, "audio output restored");
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

void AudioRenderTask(void*)
{
  std::array<std::int16_t, kRenderFrameCount> buffer{};
  std::uint32_t renderedBlocks = 0;
  int64_t maxRenderUs = 0;
  int64_t maxWriteUs = 0;
  std::uint32_t overBudgetRenderCount = 0;
  int64_t totalRenderUs = 0;
  std::uint32_t outputFailures = 0;
  // Underruns: the DMA played out everything it had (a click) because rendering fell behind for longer
  // than the queue lasts. Estimated from the clock: a write that had to wait found the queue full; time
  // between writes drains it.
  std::uint32_t underruns = 0;
  int64_t queuedUs = 0;
  int64_t lastWriteEndUs = 0;
  // What the last printed lines said (kQuietLogIntervals).
  std::uint32_t loggedUnderruns = 0;
  std::uint32_t quietIntervals = kQuietLogIntervals;  // the first interval is printed
  unsigned long previousUsbPackets = 0;  // USB audio packets counted at the end of the last interval
  unsigned long loggedUsbRestarts = 0;
  bool loggedUsbStreaming = false;
  // Without a speaker to write to (the microphone has GPIO43, the codec is down, a write failed),
  // nothing paces the render: blocks are rendered at the rate they would be heard, at most two in a
  // row before a tick's sleep, so the engines, streamed samples and the audio to the computer keep real
  // time (they ran at about half speed while recording) and the core's lower tasks still run.
  int64_t heldSinceUs = 0;
  int64_t heldBlocks = 0;
  int heldInARow = 0;
  int64_t nextRecoverUs = 0;
  int64_t writeRetryAtUs = 0;
  for (;;)
  {
    const bool paused = OutputPaused();
    const i2s_chan_handle_t outputChannel = CardenzaHAL::Audio::GetTxChannel();
    const int64_t nowUs = esp_timer_get_time();
    const bool codecDown = outputChannel == nullptr || !CardenzaHAL::Audio::IsCodecReady();
    if (!paused && codecDown) RecoverOutput(nowUs, nextRecoverUs);
    const bool held = paused || codecDown || nowUs < writeRetryAtUs;
    if (held)
    {
      lastWriteEndUs = 0;  // no queue to run dry
      // Starting, or too far behind to catch up: count from now.
      if (heldSinceUs == 0 || (nowUs - heldSinceUs) - heldBlocks * kRenderBudgetUs > kOutputQueueUs)
      {
        heldSinceUs = nowUs;
        heldBlocks = 0;
      }
      if (heldBlocks * kRenderBudgetUs > nowUs - heldSinceUs || heldInARow >= 2)
      {
        heldInARow = 0;
        vTaskDelay(1);
        continue;
      }
      ++heldBlocks;
      ++heldInARow;
    }
    else
    {
      heldSinceUs = 0;
      heldInARow = 0;
    }

    const int64_t renderStartUs = esp_timer_get_time();
    RenderSynthFrames(buffer.data(), buffer.size());
    const int64_t renderEndUs = esp_timer_get_time();
    SendToComputer(buffer.data(), buffer.size());
    const int64_t writeStartUs = esp_timer_get_time();
    if (held || OutputPaused()) continue;

    // This task also drains the engine events, so it never ends: an output that fails stays silent
    // and is tried again a second later.
    if (!EnsureOutputChannelEnabled() || !WriteOutputFrames(outputChannel, buffer.data(), buffer.size()))
    {
      lastWriteEndUs = 0;
      if (OutputPaused()) continue;
      if ((++outputFailures % 64) == 1) ESP_LOGE(kLogTag, "I2S output failed after %" PRIu32 " rendered blocks", renderedBlocks);
      writeRetryAtUs = esp_timer_get_time() + 1000000;
      continue;
    }

    const int64_t writeEndUs = esp_timer_get_time();
    const int64_t renderUs = renderEndUs - renderStartUs;
    const int64_t writeUs = writeEndUs - writeStartUs;
    if (lastWriteEndUs != 0)
    {
      queuedUs -= writeStartUs - lastWriteEndUs;
      if (queuedUs < 0)
      {
        ++underruns;
        queuedUs = 0;
      }
    }
    queuedUs = writeUs > kWriteWaitedUs ? kOutputQueueUs : (std::min)(kOutputQueueUs, queuedUs + kRenderBudgetUs);
    lastWriteEndUs = writeEndUs;
    maxRenderUs = (std::max)(maxRenderUs, renderUs);
    maxWriteUs = (std::max)(maxWriteUs, writeUs);
    totalRenderUs += renderUs;
    if (renderUs > kRenderBudgetUs) ++overBudgetRenderCount;
    g_timingBlocks.store(renderedBlocks + 1, std::memory_order_relaxed);
    g_timingRenderUs.store(g_timingRenderUs.load(std::memory_order_relaxed) + static_cast<std::uint32_t>(renderUs), std::memory_order_relaxed);
    g_timingOverruns.store(overBudgetRenderCount, std::memory_order_relaxed);
    g_timingUnderruns.store(underruns, std::memory_order_relaxed);
    if (static_cast<std::uint32_t>(renderUs) > g_timingLongestUs.load(std::memory_order_relaxed))
      g_timingLongestUs.store(static_cast<std::uint32_t>(renderUs), std::memory_order_relaxed);

    ++renderedBlocks;
    if ((renderedBlocks % kTimingLogBlockInterval) == 0)
    {
      const bool heartbeat = ++quietIntervals >= kQuietLogIntervals;
      if (heartbeat || underruns != loggedUnderruns)
      {
        ESP_LOGI(kLogTag,
                 "audio timing: blocks=%" PRIu32 " avg_render=%" PRId64 "us max_render=%" PRId64 "us max_write=%" PRId64 "us budget=%" PRId64 "us overruns=%" PRIu32 " underruns=%" PRIu32,
                 renderedBlocks,
                 totalRenderUs / kTimingLogBlockInterval,
                 maxRenderUs,
                 maxWriteUs,
                 kRenderBudgetUs,
                 overBudgetRenderCount,
                 underruns);
        loggedUnderruns = underruns;
        quietIntervals = 0;
      }
      const CardenzaHAL::USB::AudioCounters usb =
          CardenzaHAL::USB::Started() ? CardenzaHAL::USB::AudioStats() : CardenzaHAL::USB::AudioCounters{};
      const unsigned long usbPackets = static_cast<unsigned long>(usb.packetsFromHost) + static_cast<unsigned long>(usb.packetsToHost);
      const bool usbStreaming = usbPackets != previousUsbPackets;
      previousUsbPackets = usbPackets;
      if (CardenzaHAL::USB::Started() &&
          (usbStreaming != loggedUsbStreaming || usb.playbackRestarts != loggedUsbRestarts || (heartbeat && usbStreaming)))
      {
        loggedUsbStreaming = usbStreaming;
        loggedUsbRestarts = usb.playbackRestarts;
        ESP_LOGI(kLogTag, "usb audio: from_pc %lu pkts %lu B fifo %lu read %lu restarts %lu | to_pc %lu pkts %lu B empty %lu fifo %lu written %lu peak %u | in=%d out=%d heap=%u",
                 static_cast<unsigned long>(usb.packetsFromHost), static_cast<unsigned long>(usb.bytesFromHost),
                 static_cast<unsigned long>(usb.fifoFromHost), static_cast<unsigned long>(g_framesFromComputer.load()),
                 static_cast<unsigned long>(usb.playbackRestarts), static_cast<unsigned long>(usb.packetsToHost), static_cast<unsigned long>(usb.bytesToHost),
                 static_cast<unsigned long>(usb.emptyPacketsToHost), static_cast<unsigned long>(usb.fifoToHost),
                 static_cast<unsigned long>(g_framesToComputer.load()), static_cast<unsigned>(g_peakToComputer.exchange(0)),
                 CardenzaHAL::USB::AudioInputActive() ? 1 : 0,
                 CardenzaHAL::USB::AudioOutputActive() ? 1 : 0, static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
      }
      maxRenderUs = 0;
      maxWriteUs = 0;
      totalRenderUs = 0;
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
  // Mono: one sample per frame, the HAL sends it to both codec slots. The task starts even when the
  // codec does not: it drains the engines' events (they pile up in memory otherwise) and tries the
  // codec again once a second.
  if (!CardenzaHAL::Audio::Init(I2S_SLOT_MODE_MONO) || CardenzaHAL::Audio::GetTxChannel() == nullptr)
  {
    ESP_LOGW(kLogTag, "audio output not ready: trying again");
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
        // This task outranks the UI and the audio: an error must not become a busy loop.
        g_audioInputOverflow.store(true, std::memory_order_relaxed);
        vTaskDelay(1);
      }
      const std::size_t count = bytesRead / sizeof(std::int16_t);
      std::lock_guard<std::mutex> dataLock(g_audioInputDataMutex);
      if (!g_audioInputRing) continue;
      if (count > g_audioInputRingFrames - g_audioInputCount)
      {
        g_audioInputOverflow.store(true, std::memory_order_relaxed);
        continue;
      }
      for (std::size_t i = 0; i < count; ++i)
      {
        g_audioInputRing[(g_audioInputHead + g_audioInputCount + i) % g_audioInputRingFrames] = frames[i];
      }
      g_audioInputCount += count;
    }
  }
}

std::atomic<std::uint8_t> g_outputVolume{Device::Audio::kDefaultOutputVolume};

}  // namespace

namespace Device::Audio {

void SetOutputVolume(std::uint8_t percent)
{
  // The HAL's percent runs the codec linearly in dB over its whole -95.5..0 dB, so half the slider
  // would be inaudible. Ours takes the top 50 dB, half a dB per percent (a codec register step).
  const std::uint8_t volume = (std::min)(percent, std::uint8_t{100});
  g_outputVolume.store(volume, std::memory_order_relaxed);
  (void)CardenzaHAL::Audio::SetVolume(volume == 0 ? 0 : static_cast<std::uint8_t>(((91 + volume) * 100 + 95) / 191));
}

ComputerAudioStats ComputerStats()
{
  ComputerAudioStats stats;
  stats.framesToComputer = g_framesToComputer.load(std::memory_order_relaxed);
  stats.framesFromComputer = g_framesFromComputer.load(std::memory_order_relaxed);
  stats.peakFromComputer = g_peakFromComputer.exchange(0, std::memory_order_relaxed);
  return stats;
}

std::size_t ReadComputerAudio(std::int16_t* stereoFrames, std::size_t frameCount)
{
  const std::size_t frames = CardenzaHAL::USB::AudioRead(stereoFrames, frameCount);
  if (frames == 0) return 0;
  g_framesFromComputer.fetch_add(static_cast<std::uint32_t>(frames), std::memory_order_relaxed);
  int peak = 0;
  for (std::size_t sample = 0; sample < 2 * frames; ++sample) peak = (std::max)(peak, std::abs(static_cast<int>(stereoFrames[sample])));
  if (peak > g_peakFromComputer.load(std::memory_order_relaxed)) g_peakFromComputer.store(static_cast<std::uint16_t>(peak), std::memory_order_relaxed);
  // The computer's own volume control: mute, and its level in 1/256 dB.
  const float gain = CardenzaHAL::USB::HostMuted() ? 0.0f : std::pow(10.0f, CardenzaHAL::USB::HostVolume() / (256.0f * 20.0f));
  if (gain < 1.0f)
    for (std::size_t sample = 0; sample < 2 * frames; ++sample) stereoFrames[sample] = static_cast<std::int16_t>(stereoFrames[sample] * gain);
  return frames;
}

RenderTiming Timing()
{
  RenderTiming timing;
  timing.blocks = g_timingBlocks.load(std::memory_order_relaxed);
  timing.renderMicros = g_timingRenderUs.load(std::memory_order_relaxed);
  timing.overruns = g_timingOverruns.load(std::memory_order_relaxed);
  timing.underruns = g_timingUnderruns.load(std::memory_order_relaxed);
  timing.longestRenderMicros = g_timingLongestUs.exchange(0, std::memory_order_relaxed);
  return timing;
}

std::uint8_t OutputVolume()
{
  return g_outputVolume.load(std::memory_order_relaxed);
}

InputStreamInfo InputInfo()
{
  return InputStreamInfo{kDefaultInputSampleRate, kInputChannels, kInputBitsPerSample};
}

InputStatus StartInput()
{
  std::lock_guard<std::mutex> lock(g_audioInputMutex);
  if (g_audioInputEnabled)
  {
    return InputStatus::Ready;
  }

  {
    std::lock_guard<std::mutex> dataLock(g_audioInputDataMutex);
    for (const std::size_t frames : kInputRingSizes)
    {
      g_audioInputRing.reset(new (std::nothrow) std::int16_t[frames]);
      g_audioInputRingFrames = frames;
      if (g_audioInputRing) break;
    }
    if (!g_audioInputRing) return InputStatus::NotReady;
    g_audioInputHead = 0;
    g_audioInputCount = 0;
    g_audioInputOverflow.store(false, std::memory_order_relaxed);
  }
  auto fail = [] {
    ResumeOutputAfterInput();
    std::lock_guard<std::mutex> dataLock(g_audioInputDataMutex);
    g_audioInputRing.reset();
    g_audioInputCount = 0;
    return InputStatus::NotReady;
  };
  // The speaker lets go of GPIO43 before the microphone channel takes it.
  PauseOutputForInput();
  const i2s_chan_handle_t inputChannel = CardenzaHAL::Mic::InputChannel();
  if (inputChannel == nullptr || !CardenzaHAL::Mic::RestoreInputPins())
  {
    return fail();
  }

  if (g_audioInputTask == nullptr)
  {
    i2s_event_callbacks_t callbacks{};
    callbacks.on_recv_q_ovf = InputQueueOverflow;
    if (i2s_channel_register_event_callback(inputChannel, &callbacks, nullptr) != ESP_OK ||
        xTaskCreate(AudioInputTask, "sample_input", 4096, nullptr, 7, &g_audioInputTask) != pdPASS)
    {
      g_audioInputTask = nullptr;
      return fail();
    }
  }
  if (i2s_channel_enable(inputChannel) != ESP_OK)
  {
    return fail();
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
  ResumeOutputAfterInput();
  ESP_LOGI(kLogTag, "input stopped: reader stack left %u bytes", static_cast<unsigned>(uxTaskGetStackHighWaterMark(g_audioInputTask)));
  std::lock_guard<std::mutex> dataLock(g_audioInputDataMutex);
  g_audioInputRing.reset();
  g_audioInputCount = 0;
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
      framesRead = g_audioInputRing ? std::min(frameCount, g_audioInputCount) : 0;
      for (std::size_t i = 0; i < framesRead; ++i)
      {
        destination[i] = g_audioInputRing[(g_audioInputHead + i) % g_audioInputRingFrames];
      }
      g_audioInputHead = (g_audioInputHead + framesRead) % g_audioInputRingFrames;
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

  const bool codec = CardenzaHAL::Audio::Init(I2S_SLOT_MODE_MONO);
  // The render task runs whether or not the codec started (it tries again), so the engines always
  // have their output.
  binding.hasNativeOutput = EnsureAudioPumpStarted();
  binding.connected = binding.hasNativeOutput && codec;
  return binding;
}

}  // namespace device

#include "Devices/Device.h"
#include "Devices/DeviceAudio.h"

#include <algorithm>
#include <deque>
#include <memory>
#include <mutex>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <mmsystem.h>

#include <array>
#include <chrono>
#include <thread>
#endif

namespace {

device::SynthAudioRenderProvider g_audioProvider{};
std::mutex g_audioProviderMutex;
bool g_audioInputEnabled = false;
Device::Audio::InputStreamInfo g_inputStreamInfo{};

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

#if defined(_WIN32)

constexpr std::size_t kWaveBufferFrameCount = 512;
constexpr std::size_t kWaveInputMaxChannels = 2;
constexpr std::size_t kWaveBufferCount = 4;
std::mutex g_waveInCaptureMutex;

class WaveOutPump {
public:
  bool Start()
  {
    if (running_)
    {
      return true;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = CurrentAudioProvider().sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    if (waveOutOpen(&device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
      device_ = nullptr;
      return false;
    }

    for (std::size_t index = 0; index < kWaveBufferCount; ++index)
    {
      headers_[index] = {};
      headers_[index].lpData = reinterpret_cast<LPSTR>(buffers_[index].data());
      headers_[index].dwBufferLength = static_cast<DWORD>(buffers_[index].size() * sizeof(std::int16_t));
      if (waveOutPrepareHeader(device_, &headers_[index], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
      {
        Stop();
        return false;
      }

      RefillBuffer(index);
      if (waveOutWrite(device_, &headers_[index], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
      {
        Stop();
        return false;
      }
    }

    running_ = true;
    thread_ = std::thread([this]() {
      Run();
    });
    return true;
  }

  ~WaveOutPump()
  {
    Stop();
  }

  bool IsRunning() const
  {
    return running_;
  }

private:
  void RefillBuffer(std::size_t index)
  {
    RenderSynthFrames(buffers_[index].data(), buffers_[index].size());
    headers_[index].dwFlags &= ~WHDR_DONE;
    headers_[index].dwBufferLength = static_cast<DWORD>(buffers_[index].size() * sizeof(std::int16_t));
  }

  void Run()
  {
    while (running_)
    {
      bool refilledAny = false;
      for (std::size_t index = 0; index < kWaveBufferCount; ++index)
      {
        if ((headers_[index].dwFlags & WHDR_DONE) == 0)
        {
          continue;
        }

        RefillBuffer(index);
        if (waveOutWrite(device_, &headers_[index], sizeof(WAVEHDR)) == MMSYSERR_NOERROR)
        {
          refilledAny = true;
        }
      }

      if (!refilledAny)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }
  }

  void Stop()
  {
    if (!device_)
    {
      return;
    }

    running_ = false;
    if (thread_.joinable())
    {
      thread_.join();
    }

    waveOutReset(device_);
    for (WAVEHDR& header : headers_)
    {
      if (header.lpData != nullptr)
      {
        waveOutUnprepareHeader(device_, &header, sizeof(WAVEHDR));
      }
    }
    waveOutClose(device_);
    device_ = nullptr;
  }

  HWAVEOUT device_ = nullptr;
  std::array<std::array<std::int16_t, kWaveBufferFrameCount>, kWaveBufferCount> buffers_{};
  std::array<WAVEHDR, kWaveBufferCount> headers_{};
  std::thread thread_{};
  bool running_ = false;
};

class WaveInCapture {
public:
  bool Start()
  {
    if (running_)
    {
      return true;
    }

    WAVEFORMATEX format{};
    const std::array<std::pair<DWORD, WORD>, 4> formatCandidates = {{
        {Device::Audio::kDefaultInputSampleRate, 1},
        {48000, 1},
        {48000, 2},
        {Device::Audio::kDefaultInputSampleRate, 2},
    }};
    for (const auto& candidate : formatCandidates)
    {
      format = {};
      format.wFormatTag = WAVE_FORMAT_PCM;
      format.nChannels = candidate.second;
      format.nSamplesPerSec = candidate.first;
      format.wBitsPerSample = Device::Audio::kInputBitsPerSample;
      format.nBlockAlign = static_cast<WORD>(format.nChannels * format.wBitsPerSample / 8);
      format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

      if (waveInOpen(&device_, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR)
      {
        inputChannelCount_ = format.nChannels;
        inputSampleRate_ = format.nSamplesPerSec;
        break;
      }
    }

    if (device_ == nullptr)
    {
      return false;
    }

    g_inputStreamInfo.sampleRate = inputSampleRate_;
    g_inputStreamInfo.channelCount = 1;
    g_inputStreamInfo.bitsPerSample = Device::Audio::kInputBitsPerSample;

    for (std::size_t index = 0; index < kWaveBufferCount; ++index)
    {
      headers_[index] = {};
      headers_[index].lpData = reinterpret_cast<LPSTR>(buffers_[index].data());
      headers_[index].dwBufferLength = static_cast<DWORD>(kWaveBufferFrameCount * inputChannelCount_ * sizeof(std::int16_t));
      if (waveInPrepareHeader(device_, &headers_[index], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
      {
        Stop();
        return false;
      }

      if (waveInAddBuffer(device_, &headers_[index], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
      {
        Stop();
        return false;
      }
    }

    if (waveInStart(device_) != MMSYSERR_NOERROR)
    {
      Stop();
      return false;
    }

    running_ = true;
    return true;
  }

  ~WaveInCapture()
  {
    Stop();
  }

  bool IsRunning() const
  {
    return running_;
  }

  void Stop()
  {
    if (!device_)
    {
      return;
    }

    running_ = false;
    waveInStop(device_);
    waveInReset(device_);

    {
      std::lock_guard<std::mutex> lock(dataMutex_);
      capturedFrames_.clear();
    }

    for (WAVEHDR& header : headers_)
    {
      if (header.lpData != nullptr)
      {
        waveInUnprepareHeader(device_, &header, sizeof(WAVEHDR));
      }
    }

    waveInClose(device_);
    device_ = nullptr;
  }

  Device::Audio::InputStatus ReadFrames(std::int16_t* destination,
                                        std::size_t frameCount,
                                        std::size_t& framesRead,
                                        std::uint32_t timeoutMs)
  {
    framesRead = 0;
    if (!running_)
    {
      return Device::Audio::InputStatus::NotReady;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    for (;;)
    {
      HarvestCompletedBuffers();
      {
        std::lock_guard<std::mutex> lock(dataMutex_);
        const std::size_t availableFrames = capturedFrames_.size();
        if (availableFrames != 0 || timeoutMs == 0)
        {
          framesRead = std::min(frameCount, availableFrames);
          for (std::size_t index = 0; index < framesRead; ++index)
          {
            destination[index] = capturedFrames_.front();
            capturedFrames_.pop_front();
          }
          return Device::Audio::InputStatus::Ready;
        }
      }

      if (std::chrono::steady_clock::now() >= deadline)
      {
        return Device::Audio::InputStatus::Ready;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

private:
  void HarvestCompletedBuffers()
  {
    if (!device_)
    {
      return;
    }

    for (std::size_t index = 0; index < kWaveBufferCount; ++index)
    {
      if ((headers_[index].dwFlags & WHDR_DONE) == 0)
      {
        continue;
      }

      const std::size_t capturedSampleCount = static_cast<std::size_t>(headers_[index].dwBytesRecorded / sizeof(std::int16_t));
      if (capturedSampleCount > 0)
      {
        std::lock_guard<std::mutex> lock(dataMutex_);
        if (inputChannelCount_ <= 1)
        {
          capturedFrames_.insert(capturedFrames_.end(),
                                 buffers_[index].begin(),
                                 buffers_[index].begin() + static_cast<std::ptrdiff_t>(capturedSampleCount));
        }
        else
        {
          const std::size_t capturedFrameCount = capturedSampleCount / inputChannelCount_;
          for (std::size_t frame = 0; frame < capturedFrameCount; ++frame)
          {
            std::int32_t sum = 0;
            for (std::size_t channel = 0; channel < inputChannelCount_; ++channel)
            {
              sum += buffers_[index][frame * inputChannelCount_ + channel];
            }
            capturedFrames_.push_back(static_cast<std::int16_t>(sum / static_cast<std::int32_t>(inputChannelCount_)));
          }
        }
      }

      headers_[index].dwBytesRecorded = 0;
      headers_[index].dwFlags &= ~WHDR_DONE;
      if (running_ && waveInAddBuffer(device_, &headers_[index], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
      {
        running_ = false;
        return;
      }
    }
  }

  HWAVEIN device_ = nullptr;
  std::array<std::array<std::int16_t, kWaveBufferFrameCount * kWaveInputMaxChannels>, kWaveBufferCount> buffers_{};
  std::array<WAVEHDR, kWaveBufferCount> headers_{};
  std::mutex dataMutex_;
  std::deque<std::int16_t> capturedFrames_{};
  std::size_t inputChannelCount_ = 1;
  std::uint32_t inputSampleRate_ = Device::Audio::kDefaultInputSampleRate;
  bool running_ = false;
};

std::unique_ptr<WaveInCapture>& WaveInCaptureInstance()
{
  static std::unique_ptr<WaveInCapture> capture;
  return capture;
}

bool EnsureWaveOutPumpStarted()
{
  static std::mutex mutex;
  static std::unique_ptr<WaveOutPump> pump;
  std::lock_guard<std::mutex> lock(mutex);

  if (pump && pump->IsRunning())
  {
    return true;
  }

  pump = std::make_unique<WaveOutPump>();
  if (!pump->Start())
  {
    pump.reset();
    return false;
  }

  return true;
}

bool EnsureWaveInCaptureStarted()
{
  std::unique_ptr<WaveInCapture>& capture = WaveInCaptureInstance();
  std::lock_guard<std::mutex> lock(g_waveInCaptureMutex);

  if (capture && capture->IsRunning())
  {
    return true;
  }

  capture = std::make_unique<WaveInCapture>();
  if (!capture->Start())
  {
    capture.reset();
    return false;
  }

  return true;
}

void StopWaveInCapture()
{
  std::unique_ptr<WaveInCapture>& capture = WaveInCaptureInstance();
  std::lock_guard<std::mutex> lock(g_waveInCaptureMutex);
  if (!capture)
  {
    return;
  }

  capture->Stop();
  capture.reset();
}

Device::Audio::InputStatus ReadWaveInFrames(std::int16_t* destination,
                                            std::size_t frameCount,
                                            std::size_t& framesRead,
                                            std::uint32_t timeoutMs)
{
  std::unique_ptr<WaveInCapture>& capture = WaveInCaptureInstance();
  std::lock_guard<std::mutex> lock(g_waveInCaptureMutex);
  if (!capture)
  {
    framesRead = 0;
    return Device::Audio::InputStatus::NotReady;
  }

  return capture->ReadFrames(destination, frameCount, framesRead, timeoutMs);
}

#else

bool EnsureWaveOutPumpStarted()
{
  return false;
}

bool EnsureWaveInCaptureStarted()
{
  return false;
}

void StopWaveInCapture()
{
}

Device::Audio::InputStatus ReadWaveInFrames(std::int16_t*,
                                            std::size_t,
                                            std::size_t& framesRead,
                                            std::uint32_t)
{
  framesRead = 0;
  return Device::Audio::InputStatus::NotReady;
}

#endif

}  // namespace

namespace Device::Audio {

InputStreamInfo InputInfo()
{
  return g_inputStreamInfo;
}

InputStatus StartInput()
{
  g_audioInputEnabled = EnsureWaveInCaptureStarted();
  return g_audioInputEnabled ? InputStatus::Ready : InputStatus::NotReady;
}

void StopInput()
{
  g_audioInputEnabled = false;
  StopWaveInCapture();
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

  if (!g_audioInputEnabled)
  {
    return InputStatus::NotReady;
  }

  if (frameCount == 0)
  {
    return InputStatus::Ready;
  }

  return ReadWaveInFrames(destination, frameCount, framesRead, timeoutMs);
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
  binding.backendName = EnsureWaveOutPumpStarted() ? "simulator-waveout" : "simulator-stub";
  binding.connected = binding.backendName == "simulator-waveout";
  binding.hasNativeOutput = binding.connected;
  return binding;
}

}  // namespace device
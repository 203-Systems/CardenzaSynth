#include "Devices/DeviceAudio.h"

#include <memory>
#include <mutex>

#include "Library/Synth/SynthAudioRuntime.h"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#include <mmsystem.h>

#include <array>
#include <chrono>
#include <thread>
#endif

namespace device {

namespace {

#if defined(_WIN32)

constexpr std::size_t kWaveBufferFrameCount = 512;
constexpr std::size_t kWaveBufferCount = 4;

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
    format.nSamplesPerSec = synth::audio::kSampleRate;
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
    synth::audio::RenderFrames(buffers_[index].data(), buffers_[index].size());
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

#else

bool EnsureWaveOutPumpStarted()
{
  return false;
}

#endif

}  // namespace

SynthAudioPipelineBinding ConnectSynthAudioPipeline(std::size_t engineIndex) {
  SynthAudioPipelineBinding binding;
  binding.engineIndex = engineIndex;
  binding.backendName = EnsureWaveOutPumpStarted() ? "simulator-waveout" : "simulator-stub";
  binding.connected = binding.backendName == "simulator-waveout";
  binding.hasNativeOutput = binding.connected;
  return binding;
}

}  // namespace device
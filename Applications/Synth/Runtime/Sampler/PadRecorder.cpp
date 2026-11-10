#include "Applications/Synth/Runtime/Sampler/PadRecorder.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include "Applications/Synth/Runtime/Sampler/SamplerWav.h"
#include "Devices/Device.h"
#include "Library/Synth/Audio/SampleStream.h"

namespace synth {

namespace {

constexpr std::size_t kReadFrames = 512;

}  // namespace

PadRecorder::~PadRecorder()
{
  if (recording_) Device::Audio::StopInput();
}

bool PadRecorder::Begin(std::size_t track, std::size_t pad, std::string& message)
{
  if (buffer_)
  {
    message = "BUSY";
    return false;
  }
  const Device::Audio::InputStreamInfo info = Device::Audio::InputInfo();
  if (info.channelCount != 1 || info.bitsPerSample != 16 || info.sampleRate < sampler::kMinSampleRate || info.sampleRate > sampler::kMaxSampleRate)
  {
    message = "INPUT FORMAT";
    return false;
  }
  auto buffer = sampler::AllocateFrames(sampler::PadSampleFrameLimit());
  if (!buffer)
  {
    message = "NO MEMORY";
    return false;
  }
  if (Device::Audio::StartInput() != Device::Audio::InputStatus::Ready)
  {
    message = "INPUT ERR";
    return false;
  }
  buffer_ = std::move(buffer);
  frames_ = 0;
  sampleRate_ = info.sampleRate;
  track_ = track;
  pad_ = pad;
  recording_ = true;
  level_ = 0.0f;
  waveform_.fill(0.0f);
  binPeak_ = 0;
  binFrames_ = 0;
  return true;
}

void PadRecorder::Tick()
{
  if (!recording_) return;
  const std::size_t capacity = buffer_->frameCount;
  // The waveform shows the whole buffer filling up, a bin at a time.
  const std::size_t binFrames = std::max<std::size_t>(1, capacity / kWaveformBins);
  std::int32_t peak = 0;
  while (frames_ < capacity)
  {
    std::size_t read = 0;
    std::int16_t* destination = buffer_->frames.get() + frames_;
    const auto status = Device::Audio::ReadInputFrames(destination, std::min(kReadFrames, capacity - frames_), read, 0);
    if (status == Device::Audio::InputStatus::Overflow)
    {
      // Frames went missing: what came before them is the take.
      Stop();
      return;
    }
    if (status != Device::Audio::InputStatus::Ready || read == 0) break;
    for (std::size_t index = 0; index < read; ++index)
    {
      const std::int32_t magnitude = std::abs(static_cast<std::int32_t>(destination[index]));
      peak = std::max(peak, magnitude);
      binPeak_ = std::max(binPeak_, magnitude);
      if (++binFrames_ < binFrames) continue;
      const std::size_t bin = std::min<std::size_t>((frames_ + index) / binFrames, kWaveformBins - 1);
      waveform_[bin] = static_cast<float>(binPeak_) / static_cast<float>(std::numeric_limits<std::int16_t>::max());
      binPeak_ = 0;
      binFrames_ = 0;
    }
    frames_ += read;
  }
  level_ = static_cast<float>(peak) / static_cast<float>(std::numeric_limits<std::int16_t>::max());
  if (frames_ >= capacity) Stop();
}

void PadRecorder::Stop()
{
  if (!recording_) return;
  Device::Audio::StopInput();
  recording_ = false;
  level_ = 0.0f;
}

float PadRecorder::Progress() const
{
  if (!buffer_ || buffer_->frameCount == 0) return 0.0f;
  return static_cast<float>(frames_) / static_cast<float>(buffer_->frameCount);
}

bool PadRecorder::Finish(const std::string& path, std::string& message)
{
  if (!buffer_)
  {
    message = "NOT RECORDING";
    return false;
  }
  Stop();
  std::int16_t* frames = buffer_->frames.get();
  const std::size_t count = frames_;
  // Centred (a microphone's offset would click at both ends), then cut to the sound: from a little
  // before the first frame within 26 dB of the loudest to the last within 40 dB, faded out over 5 ms.
  std::int64_t sum = 0;
  for (std::size_t index = 0; index < count; ++index) sum += frames[index];
  const auto mean = count != 0 ? static_cast<std::int32_t>(sum / static_cast<std::int64_t>(count)) : 0;
  std::int32_t peak = 0;
  for (std::size_t index = 0; index < count; ++index)
  {
    const std::int32_t value = std::clamp<std::int32_t>(frames[index] - mean, -32768, 32767);
    frames[index] = static_cast<std::int16_t>(value);
    peak = std::max(peak, std::abs(value));
  }
  // Under -54 dBFS at its loudest there is nothing on it but the room.
  if (peak < 65)
  {
    Cancel();
    message = "TOO QUIET";
    return false;
  }
  const std::int32_t onset = std::max<std::int32_t>(peak / 20, 32);
  const std::int32_t tail = std::max<std::int32_t>(peak / 100, 16);
  std::size_t start = 0;
  while (start < count && std::abs(static_cast<std::int32_t>(frames[start])) < onset) ++start;
  std::size_t end = count;
  while (end > start && std::abs(static_cast<std::int32_t>(frames[end - 1])) < tail) --end;
  const std::size_t preroll = sampleRate_ / 500;  // 2 ms
  const std::size_t release = sampleRate_ / 100;  // 10 ms
  start = start > preroll ? start - preroll : 0;
  end = std::min(count, end + release);
  const std::size_t length = end - start;
  const std::size_t fade = std::min<std::size_t>(length, sampleRate_ / 200);
  for (std::size_t index = 0; index < fade; ++index)
  {
    std::int16_t& sample = frames[end - fade + index];
    sample = static_cast<std::int16_t>(static_cast<std::int32_t>(sample) * static_cast<std::int32_t>(fade - index) / static_cast<std::int32_t>(fade));
  }
  bool written = false;
  {
    audio::SampleFileGuard guard(path);
    written = sampler::WriteMono16Wav(path, frames + start, length, sampleRate_);
  }
  buffer_.reset();
  frames_ = 0;
  if (!written)
  {
    message = "SAVE ERR";
    return false;
  }
  return true;
}

void PadRecorder::Cancel()
{
  Stop();
  buffer_.reset();
  frames_ = 0;
}

}  // namespace synth

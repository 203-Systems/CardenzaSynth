#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "Library/Synth/Audio/SynthAudioRuntime.h"

namespace synth {

// A drum pad's sound from the microphone: up to the length a pad holds (two seconds with PSRAM), in
// RAM. Finish takes the silence off both ends and writes it to the card's Samples as a WAV of its
// own, which the pad then plays (GrooveApp). The microphone has the speaker's clock pin, so the
// speaker is silent while it records (Device::Audio::StartInput).
class PadRecorder {
public:
  static constexpr std::size_t kWaveformBins = 32;

  ~PadRecorder();

  // false with the reason (INPUT ERR, NO MEMORY, BUSY) when it cannot start.
  bool Begin(std::size_t track, std::size_t pad, std::string& message);
  // Reads the microphone; at the end of the buffer it stops by itself (Full()).
  void Tick();
  // Stops and writes what it heard to `path` (a WAV in Samples). false with the reason (TOO QUIET,
  // SAVE ERR) when there is nothing to keep; nothing is written then.
  bool Finish(const std::string& path, std::string& message);
  void Cancel();

  bool Active() const { return buffer_ != nullptr; }
  bool Recording() const { return recording_; }
  // Stopped by itself at the end of the buffer, waiting for Finish.
  bool Full() const { return buffer_ != nullptr && !recording_; }
  std::size_t Track() const { return track_; }
  std::size_t Pad() const { return pad_; }
  float Level() const { return level_; }
  float Progress() const;
  const std::array<float, kWaveformBins>& Waveform() const { return waveform_; }

private:
  void Stop();

  std::shared_ptr<audio::SampleBuffer> buffer_;
  std::size_t frames_ = 0;
  std::uint32_t sampleRate_ = audio::kSampleRate;
  std::size_t track_ = 0;
  std::size_t pad_ = 0;
  bool recording_ = false;
  float level_ = 0.0f;
  std::array<float, kWaveformBins> waveform_{};
  std::int32_t binPeak_ = 0;
  std::size_t binFrames_ = 0;
};

}  // namespace synth

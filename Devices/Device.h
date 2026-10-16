#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace Device {

namespace Display {

inline constexpr int kWidth = 240;
inline constexpr int kHeight = 135;
inline constexpr std::size_t kPixelCount = static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(kHeight);

using Pixel = std::uint16_t;

struct FrameBufferView {
  const Pixel* pixels = nullptr;
  std::size_t pixelCount = kPixelCount;
  int width = kWidth;
  int height = kHeight;
};

struct MutableFrameBufferView {
  Pixel* pixels = nullptr;
  std::size_t pixelCount = kPixelCount;
  int width = kWidth;
  int height = kHeight;

  operator FrameBufferView() const {
    return FrameBufferView{pixels, pixelCount, width, height};
  }
};

FrameBufferView CurrentFrameBuffer();
MutableFrameBufferView MutableFrameBuffer();
void Clear(Pixel color = 0);
void Present();

}  // namespace Display

namespace Keyboard {

inline constexpr int kRows = 4;
inline constexpr int kColumns = 14;
inline constexpr int kKeyCount = kRows * kColumns;

using MatrixKeyState = std::bitset<kKeyCount>;

const MatrixKeyState& State();

}  // namespace Keyboard

namespace Buttons {

bool HomePressed();

}  // namespace Buttons

namespace LED {

inline constexpr int kRows = Keyboard::kRows;
inline constexpr int kColumns = Keyboard::kColumns;
inline constexpr int kLedCount = kRows * kColumns;
inline constexpr std::uint8_t kBrightnessOff = 0;
inline constexpr std::uint8_t kBrightnessMax = 15;

std::uint8_t Brightness(std::size_t row, std::size_t column);
void SetKey(std::size_t row, std::size_t column, std::uint8_t brightness);
void Clear();
void Fill(std::uint8_t brightness);
void SetGlobalBrightness(std::uint8_t brightness);
std::uint8_t GlobalBrightness();
void SetEnabled(bool enabled);
bool Enabled();

}  // namespace LED

namespace Audio {

inline constexpr int kOutputChannels = 2;
inline constexpr bool kHasInput = true;
inline constexpr std::uint32_t kDefaultInputSampleRate = 44100;
inline constexpr std::uint8_t kInputChannels = 1;
inline constexpr std::uint8_t kInputBitsPerSample = 16;

struct SampleBufferView {
  const std::int16_t* samples = nullptr;
  std::size_t frameCount = 0;
};

struct InputStreamInfo {
  std::uint32_t sampleRate = kDefaultInputSampleRate;
  std::uint8_t channelCount = kInputChannels;
  std::uint8_t bitsPerSample = kInputBitsPerSample;
};

enum class InputStatus {
  Ready,
  NotReady,
  InvalidArgument,
  Overflow,
};

InputStreamInfo InputInfo();
InputStatus StartInput();
void StopInput();
InputStatus ReadInputFrames(std::int16_t* destination,
                            std::size_t frameCount,
                            std::size_t& framesRead,
                            std::uint32_t timeoutMs = 0);

}  // namespace Audio

namespace Storage {

struct FileHandleView {
  const char* path = nullptr;
  bool writable = false;
};

std::string RootPath();
std::string SampleDirectoryPath();
std::string PresetDirectoryPath();
std::string PresetDirectoryPath(std::string_view instrument);
std::string ProjectDirectoryPath();
bool EnsureDirectoryLayout();

}  // namespace Storage

std::string Name();
std::string BackendName();
bool Init();
void Start();

}  // namespace Device

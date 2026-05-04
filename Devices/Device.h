#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

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
inline constexpr bool kHasPerKeyLed = true;

struct MatrixKeyEvent {
  std::size_t row = 0;
  std::size_t column = 0;
  bool pressed = false;
};

using MatrixKeyState = std::array<bool, kKeyCount>;

MatrixKeyState SnapshotState();

}  // namespace Keyboard

namespace Audio {

inline constexpr int kOutputChannels = 2;
inline constexpr bool kHasInput = true;

struct SampleBufferView {
  const std::int16_t* samples = nullptr;
  std::size_t frameCount = 0;
};

}  // namespace Audio

namespace Storage {

struct FileHandleView {
  const char* path = nullptr;
  bool writable = false;
};

}  // namespace Storage

std::string Name();
std::string BackendName();
bool Init();
void Start();

}  // namespace Device

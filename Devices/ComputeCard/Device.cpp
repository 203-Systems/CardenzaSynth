#include "Devices/Device.h"

#include <algorithm>
#include <array>

#include "computecardHAL.h"

namespace {

std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
std::array<std::uint8_t, Device::LED::kLedCount> keyboardLedState{};
std::uint8_t keyboardLedGlobalBrightness = Device::LED::kBrightnessMax;
bool keyboardLedsEnabled = false;

constexpr bool IsValidMatrixPosition(std::size_t row, std::size_t column) {
  return row < static_cast<std::size_t>(Device::LED::kRows) &&
         column < static_cast<std::size_t>(Device::LED::kColumns);
}

constexpr std::size_t MatrixIndex(std::size_t row, std::size_t column) {
  return row * static_cast<std::size_t>(Device::LED::kColumns) + column;
}

std::uint8_t ClampLedBrightness(std::uint8_t brightness) {
  return std::min(brightness, Device::LED::kBrightnessMax);
}

}  // namespace

namespace Device {

namespace Display {

FrameBufferView CurrentFrameBuffer() {
  return FrameBufferView{frameBuffer.data(), frameBuffer.size(), kWidth, kHeight};
}

MutableFrameBufferView MutableFrameBuffer() {
  return MutableFrameBufferView{frameBuffer.data(), frameBuffer.size(), kWidth, kHeight};
}

void Clear(Pixel color) {
  frameBuffer.fill(color);
}

void Present() {
#if defined(ESP_PLATFORM)
  (void)ComputeCardHAL::Display::PresentFrame(frameBuffer.data(), kWidth, kHeight);
#endif
}

}  // namespace Display

namespace Keyboard {

const MatrixKeyState& State() {
  keyboardState.reset();
  ComputeCardHAL::Keyboard::updateKeyList();
  for (const auto& key : ComputeCardHAL::Keyboard::keyList())
  {
    if (key.y < 0 || key.y >= kRows || key.x < 0 || key.x >= kColumns)
    {
      continue;
    }

    const std::size_t index = static_cast<std::size_t>(key.y) * static_cast<std::size_t>(kColumns) + static_cast<std::size_t>(key.x);
    keyboardState.set(index);
  }
  return keyboardState;
}

}  // namespace Keyboard

namespace LED {

std::uint8_t Brightness(std::size_t row, std::size_t column) {
  if (!IsValidMatrixPosition(row, column)) {
    return kBrightnessOff;
  }

  return keyboardLedState[MatrixIndex(row, column)];
}

void SetKey(std::size_t row, std::size_t column, std::uint8_t brightness) {
  if (!IsValidMatrixPosition(row, column)) {
    return;
  }

  const std::uint8_t clampedBrightness = ClampLedBrightness(brightness);
  keyboardLedState[MatrixIndex(row, column)] = clampedBrightness;
  ComputeCardHAL::Keyboard::setLED(ComputeCardHAL::Keyboard::Point2D_t{
      static_cast<int>(column), static_cast<int>(row)},
      clampedBrightness);
}

void Clear() {
  keyboardLedState.fill(kBrightnessOff);
  ComputeCardHAL::Keyboard::clearAllLEDs();
}

void Fill(std::uint8_t brightness) {
  const std::uint8_t clampedBrightness = ClampLedBrightness(brightness);
  keyboardLedState.fill(clampedBrightness);
  ComputeCardHAL::Keyboard::setAllLEDs(clampedBrightness);
}

void SetGlobalBrightness(std::uint8_t brightness) {
  keyboardLedGlobalBrightness = ClampLedBrightness(brightness);
  ComputeCardHAL::Keyboard::setLEDGlobalBrightness(keyboardLedGlobalBrightness);
}

std::uint8_t GlobalBrightness() {
  return keyboardLedGlobalBrightness;
}

void SetEnabled(bool enabled) {
  keyboardLedsEnabled = enabled;
  (void)ComputeCardHAL::Keyboard::enableLED(enabled);
}

bool Enabled() {
  return keyboardLedsEnabled;
}

}  // namespace LED

std::string Name() {
  return "ComputeCard";
}

std::string BackendName() {
  return "hardware-hal";
}

bool Init() {
  return ComputeCardHAL::Init();
}

void Start() {}

}  // namespace Device

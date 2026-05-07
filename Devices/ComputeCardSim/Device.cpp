#include "Devices/Device.h"

#include <algorithm>
#include <array>

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

void Present() {}

}  // namespace Display

namespace Keyboard {

const MatrixKeyState& State() {
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

  keyboardLedState[MatrixIndex(row, column)] = ClampLedBrightness(brightness);
}

void Clear() {
  keyboardLedState.fill(kBrightnessOff);
}

void Fill(std::uint8_t brightness) {
  keyboardLedState.fill(ClampLedBrightness(brightness));
}

void SetGlobalBrightness(std::uint8_t brightness) {
  keyboardLedGlobalBrightness = ClampLedBrightness(brightness);
}

std::uint8_t GlobalBrightness() {
  return keyboardLedGlobalBrightness;
}

void SetEnabled(bool enabled) {
  keyboardLedsEnabled = enabled;
}

bool Enabled() {
  return keyboardLedsEnabled;
}

}  // namespace LED

std::string Name() {
  return "ComputeCardSim";
}

std::string BackendName() {
  return "sim-host+webui";
}

bool Init() {
  return true;
}

void Start() {}

}  // namespace Device

#include "Devices/Device.h"

#include <array>

#include "Devices/ComputeCard/main/hal/computecard/computecardHAL.h"
#include "Devices/ComputeCard/main/hal/computecard/display/display.h"
#include "Devices/ComputeCard/main/hal/computecard/keyboard/keyboard.h"

namespace {

std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};

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
#if __has_include(<esp_lcd_panel_ops.h>)
  (void)ComputeCardHAL::Display::PresentFrame(frameBuffer.data(), kWidth, kHeight);
#endif
}

}  // namespace Display

namespace Keyboard {

MatrixKeyState SnapshotState() {
  MatrixKeyState state{};
  ComputeCardHAL::Keyboard::updateKeyList();
  for (const auto& key : ComputeCardHAL::Keyboard::keyList())
  {
    if (key.y < 0 || key.y >= kRows || key.x < 0 || key.x >= kColumns)
    {
      continue;
    }

    const std::size_t index = static_cast<std::size_t>(key.y) * static_cast<std::size_t>(kColumns) + static_cast<std::size_t>(key.x);
    state[index] = true;
  }
  return state;
}

}  // namespace Keyboard

std::string Name() {
  return "ComputeCard";
}

std::string BackendName() {
  return "hardware-hal";
}

bool Init() {
  return ComputeCardHAL::Init() == ESP_OK;
}

void Start() {}

}  // namespace Device

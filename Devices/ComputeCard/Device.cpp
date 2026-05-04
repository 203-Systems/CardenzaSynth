#include "Devices/Device.h"

#include <array>

#include "Devices/ComputeCard/main/hal/computecard/display/display.h"

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
  (void)ComputeCardHAL::Display::Init();
  if (auto* panel = ComputeCardHAL::Display::GetPanel(); panel != nullptr)
  {
    (void)esp_lcd_panel_draw_bitmap(panel, 0, 0, kWidth, kHeight, frameBuffer.data());
  }
#endif
}

}  // namespace Display

std::string Name() {
  return "ComputeCard";
}

std::string BackendName() {
  return "hardware-hal";
}

}  // namespace Device

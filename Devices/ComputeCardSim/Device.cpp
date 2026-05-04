#include "Devices/Device.h"

#include <array>

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

void Present() {}

}  // namespace Display

std::string Name() {
  return "ComputeCardSim";
}

std::string BackendName() {
  return "sim-host+webui";
}

}  // namespace Device

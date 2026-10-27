#include "Devices/Device.h"
#include "Devices/DisplayDamage.h"
#include "Devices/LedFrameBuffer.h"
#include "Devices/LedLight.h"

#include <array>

#include <esp_attr.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>

#include "cardenzaHAL.h"

namespace {

// Only the CPU draws here. The HAL stages transfers through its internal DMA band.
EXT_RAM_BSS_ATTR std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
Device::LED::detail::LedFrameBuffer keyboardLedFrames;
using DisplayDamage = Device::Display::detail::DamageTracker<Device::Display::kWidth, Device::Display::kHeight>;
DisplayDamage displayDamage;
static_assert(Device::Display::kWidth == CardenzaHAL::Display::WIDTH &&
              Device::Display::kHeight == CardenzaHAL::Display::HEIGHT);

// The HAL mounts the card here. The folders below it belong to the OS storage protocol.
constexpr char kCardRoot[] = "/sdcard";
// A missing card is probed again at most this often (a failed SDMMC mount takes a while).
constexpr std::int64_t kMountRetryUs = 5 * 1000 * 1000;
std::int64_t lastMountAttemptUs = 0;
bool mountAttempted = false;

constexpr bool IsValidMatrixPosition(std::size_t row, std::size_t column) {
  return row < static_cast<std::size_t>(Device::LED::kRows) &&
         column < static_cast<std::size_t>(Device::LED::kColumns);
}

constexpr std::size_t MatrixIndex(std::size_t row, std::size_t column) {
  return row * static_cast<std::size_t>(Device::LED::kColumns) + column;
}

// What the HAL holds now: changed levels are sent as one frame and one scan-table publication. Key
// levels are perceived brightness, gamma-mapped to the HAL's linear duty (Devices/LedLight.h); the
// master level goes as it is, the HAL corrects it.
Device::LED::detail::LedFrame halLeds{};
bool halLedsKnown = false;

void PublishLedFrame(bool changed) {
  if (!changed) return;
  const auto& frame = keyboardLedFrames.Current();
  if (!halLedsKnown || frame.levels != halLeds.levels) {
    CardenzaHAL::Keyboard::LedFrame levels{};
    static_assert(levels.size() == Device::LED::kLedCount);
    for (std::size_t index = 0; index < levels.size(); ++index)
      levels[index] = Device::LED::KeyLevel(frame.levels[index]);
    CardenzaHAL::Keyboard::setLEDFrame(levels);
  }
  if (!halLedsKnown || frame.master != halLeds.master) CardenzaHAL::Keyboard::setLEDGlobalBrightness(frame.master);
  if (!halLedsKnown || frame.enabled != halLeds.enabled) (void)CardenzaHAL::Keyboard::enableLED(frame.enabled);
  halLeds = frame;
  halLedsKnown = true;
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
  const auto plan = displayDamage.Prepare(frameBuffer.data());
  std::array<CardenzaHAL::Display::Region, std::tuple_size_v<decltype(plan.rectangles)>> regions{};
  for (std::size_t index = 0; index < plan.count; ++index) {
    const auto& rect = plan.rectangles[index];
    regions[index] = {rect.x, rect.y, rect.width, rect.height};
  }
  if (!CardenzaHAL::Display::Present(frameBuffer.data(), regions.data(), plan.count)) {
    displayDamage.Invalidate();
    return;
  }
  displayDamage.Commit(frameBuffer.data(), plan);
}

void SetBacklight(std::uint8_t level) {
  // The HAL caches the level and applies it when the backlight PWM starts.
  (void)CardenzaHAL::Display::SetBrightness(level);
}

std::uint8_t Backlight() {
  return CardenzaHAL::Display::GetBrightness();
}

}  // namespace Display

namespace Keyboard {

const MatrixKeyState& State() {
  keyboardState.reset();
  CardenzaHAL::Keyboard::updateKeyList();
  for (const auto& key : CardenzaHAL::Keyboard::keyList())
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

namespace Buttons {

bool HomePressed() {
  return CardenzaHAL::System::GetFuncButton();
}

}  // namespace Buttons

namespace LED {

void BeginFrame() { keyboardLedFrames.BeginFrame(); }
void Present() { PublishLedFrame(keyboardLedFrames.Present()); }

std::uint8_t Brightness(std::size_t row, std::size_t column) {
  if (!IsValidMatrixPosition(row, column)) {
    return kBrightnessOff;
  }

  return keyboardLedFrames.Current().levels[MatrixIndex(row, column)];
}

void SetKey(std::size_t row, std::size_t column, std::uint8_t brightness) {
  if (!IsValidMatrixPosition(row, column)) {
    return;
  }

  PublishLedFrame(keyboardLedFrames.SetKey(row, column, brightness));
}

void Clear() {
  PublishLedFrame(keyboardLedFrames.Fill(kBrightnessOff));
}

void Fill(std::uint8_t brightness) {
  PublishLedFrame(keyboardLedFrames.Fill(brightness));
}

void SetGlobalBrightness(std::uint8_t brightness) {
  PublishLedFrame(keyboardLedFrames.SetMaster(brightness));
}

std::uint8_t GlobalBrightness() {
  return keyboardLedFrames.Current().master;
}

void SetEnabled(bool enabled) {
  PublishLedFrame(keyboardLedFrames.SetEnabled(enabled));
}

bool Enabled() {
  return keyboardLedFrames.Current().enabled;
}

}  // namespace LED

namespace Storage {

std::string RootPath() {
  return kCardRoot;
}

bool Mount() {
  if (CardenzaHAL::Storage::IsMounted()) {
    return true;
  }
  const std::int64_t now = esp_timer_get_time();
  if (mountAttempted && now - lastMountAttemptUs < kMountRetryUs) {
    return false;
  }
  mountAttempted = true;
  lastMountAttemptUs = now;
  return CardenzaHAL::Storage::Init();
}

bool IsMounted() {
  return CardenzaHAL::Storage::IsMounted();
}

void NotifyWriteFailure() {
  if (CardenzaHAL::Storage::IsMounted() && !CardenzaHAL::Storage::CheckAfterIoError()) {
    mountAttempted = true;
    lastMountAttemptUs = esp_timer_get_time();
  }
}

std::uint64_t CapacityBytes() {
  return IsMounted() ? CardenzaHAL::Storage::GetCapacity() : 0;
}

std::uint64_t FreeBytes() {
  std::uint64_t total = 0;
  std::uint64_t free = 0;
  return IsMounted() && esp_vfs_fat_info(kCardRoot, &total, &free) == ESP_OK ? free : 0;
}

}  // namespace Storage

std::string Name() {
  return "Cardenza";
}

std::string BackendName() {
  return "hardware-hal";
}

bool Init() {
  const bool initialized = CardenzaHAL::Init();
  if (!initialized) {
    return false;
  }

  // CardenzaHAL::Init has just tried to mount the card; Storage::Mount retries later.
  mountAttempted = true;
  lastMountAttemptUs = esp_timer_get_time();
  return true;
}

void Start() {}

}  // namespace Device

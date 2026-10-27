#include "Devices/Device.h"
#include "Devices/DisplayDamage.h"
#include "Devices/LedFrameBuffer.h"
#include "Devices/LedLight.h"

#include <array>

#include <esp_attr.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_commands.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "cardenzaHAL.h"

namespace {

// Only the CPU draws here. Display DMA reads the internal displayTransfer below.
EXT_RAM_BSS_ATTR std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
Device::LED::detail::LedFrameBuffer keyboardLedFrames;
SemaphoreHandle_t framePresentDone = nullptr;
bool displayCallbacksInstalled = false;
using DisplayDamage = Device::Display::detail::DamageTracker<Device::Display::kWidth, Device::Display::kHeight>;
DisplayDamage displayDamage;
constexpr int kDisplayBandRows = 8;
// The DMA buffer remains owned by the transfer even if a callback is delayed.
// UI rendering can continue into frameBuffer without touching in-flight pixels.
alignas(4) std::array<Device::Display::Pixel, Device::Display::kWidth * kDisplayBandRows> displayTransfer{};
bool displayTransferPending = false;

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

bool IRAM_ATTR OnFramePresentDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* userContext) {
  if (userContext == nullptr) {
    return false;
  }

  BaseType_t highTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(userContext), &highTaskWoken);
  return highTaskWoken == pdTRUE;
}

bool EnsureDisplayPresentSync(esp_lcd_panel_io_handle_t panelIo) {
  if (displayCallbacksInstalled) {
    return true;
  }

  if (framePresentDone == nullptr) {
    framePresentDone = xSemaphoreCreateBinary();
    if (framePresentDone == nullptr) {
      return false;
    }
  }

  esp_lcd_panel_io_callbacks_t callbacks = {};
  callbacks.on_color_trans_done = OnFramePresentDone;
  // A synchronous parameter transaction drains any HAL startup color transfers
  // before their callback is replaced. Do not mistake those for our own DMA.
  if (esp_lcd_panel_io_tx_param(panelIo, LCD_CMD_NOP, nullptr, 0) != ESP_OK) return false;
  if (esp_lcd_panel_io_register_event_callbacks(panelIo, &callbacks, framePresentDone) != ESP_OK) {
    return false;
  }

  displayCallbacksInstalled = true;
  return true;
}

bool WaitForDisplayTransfer(TickType_t timeout)
{
  if (!displayTransferPending) return true;
  if (xSemaphoreTake(framePresentDone, timeout) != pdTRUE)
  {
    displayDamage.Invalidate();
    return false;
  }
  displayTransferPending = false;
  return true;
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
  if (!CardenzaHAL::Display::Init()) {
    displayDamage.Invalidate();
    return;
  }

  const esp_lcd_panel_handle_t panel = CardenzaHAL::Display::GetPanel();
  const esp_lcd_panel_io_handle_t panelIo = CardenzaHAL::Display::GetPanelIO();
  if (panel == nullptr || panelIo == nullptr || !EnsureDisplayPresentSync(panelIo)) {
    displayDamage.Invalidate();
    return;
  }

  // A previously timed-out DMA still owns displayTransfer. Poll it without
  // stalling every UI frame; once it completes, resynchronize the full screen.
  if (!WaitForDisplayTransfer(0)) return;
  const auto plan = displayDamage.Prepare(frameBuffer.data());
  for (std::size_t index = 0; index < plan.count; ++index) {
    const auto& rect = plan.rectangles[index];
    for (int y = rect.y; y < rect.y + rect.height; y += kDisplayBandRows) {
      const detail::DamageRect band{rect.x, y, rect.width,
        std::min(kDisplayBandRows, rect.y + rect.height - y)};
      DisplayDamage::CopyBand(frameBuffer.data(), band, displayTransfer.data());
      while (xSemaphoreTake(framePresentDone, 0) == pdTRUE) {}
      if (esp_lcd_panel_draw_bitmap(panel, band.x, band.y, band.x + band.width,
                                   band.y + band.height, displayTransfer.data()) != ESP_OK) {
        displayDamage.Invalidate();
        return;
      }
      displayTransferPending = true;
      if (!WaitForDisplayTransfer(pdMS_TO_TICKS(20))) return;
    }
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

#include "Devices/Device.h"
#include "Devices/LedFrameBuffer.h"
#include "Devices/LedLight.h"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include <esp_attr.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nvs.h>
#include <nvs_flash.h>

#include "cardenzaHAL.h"

namespace {

std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
Device::LED::detail::LedFrameBuffer keyboardLedFrames;
SemaphoreHandle_t framePresentDone = nullptr;
bool displayCallbacksInstalled = false;

// The HAL mounts the card here. The folders below it belong to the OS storage protocol.
constexpr char kCardRoot[] = "/sdcard";
// A missing card is probed again at most this often (a failed SDMMC mount takes a while).
constexpr std::int64_t kMountRetryUs = 5 * 1000 * 1000;
std::int64_t lastMountAttemptUs = 0;
bool mountAttempted = false;
// Internal flash records (settings kept while the card is missing).
constexpr char kNvsNamespace[] = "cardenza";
bool nvsReady = false;

constexpr bool IsValidMatrixPosition(std::size_t row, std::size_t column) {
  return row < static_cast<std::size_t>(Device::LED::kRows) &&
         column < static_cast<std::size_t>(Device::LED::kColumns);
}

constexpr std::size_t MatrixIndex(std::size_t row, std::size_t column) {
  return row * static_cast<std::size_t>(Device::LED::kColumns) + column;
}

// What the HAL holds now: only keys that changed are sent (each setLED rebuilds its scan masks). Key
// levels are perceived brightness, gamma-mapped to the HAL's linear duty (Devices/LedLight.h); the
// master level goes as it is, the HAL corrects it.
Device::LED::detail::LedFrame halLeds{};
bool halLedsKnown = false;

void PublishLedFrame(bool changed) {
  if (!changed) return;
  const auto& frame = keyboardLedFrames.Current();
  for (std::size_t index = 0; index < frame.levels.size(); ++index) {
    if (halLedsKnown && frame.levels[index] == halLeds.levels[index]) continue;
    const CardenzaHAL::Keyboard::Point2D_t key{static_cast<int>(index % Device::LED::kColumns),
                                               static_cast<int>(index / Device::LED::kColumns)};
    CardenzaHAL::Keyboard::setLED(key, Device::LED::KeyLevel(frame.levels[index]));
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
  if (esp_lcd_panel_io_register_event_callbacks(panelIo, &callbacks, framePresentDone) != ESP_OK) {
    return false;
  }

  displayCallbacksInstalled = true;
  return true;
}

bool InitNvs() {
  esp_err_t status = nvs_flash_init();
  if (status == ESP_ERR_NVS_NO_FREE_PAGES || status == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // Truncated or written by a newer NVS layout: start over (it only holds settings copies).
    if (nvs_flash_erase() != ESP_OK) {
      return false;
    }
    status = nvs_flash_init();
  }
  return status == ESP_OK;
}

bool ValidInternalKey(std::string_view key) {
  return !key.empty() && key.size() <= Device::Storage::kInternalKeyMax &&
         std::all_of(key.begin(), key.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
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
    return;
  }

  const esp_lcd_panel_handle_t panel = CardenzaHAL::Display::GetPanel();
  const esp_lcd_panel_io_handle_t panelIo = CardenzaHAL::Display::GetPanelIO();
  if (panel == nullptr || panelIo == nullptr || !EnsureDisplayPresentSync(panelIo)) {
    return;
  }

  while (xSemaphoreTake(framePresentDone, 0) == pdTRUE) {
  }

  if (esp_lcd_panel_draw_bitmap(panel, 0, 0, kWidth, kHeight, frameBuffer.data()) != ESP_OK) {
    return;
  }

  (void)xSemaphoreTake(framePresentDone, pdMS_TO_TICKS(1000));
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

std::uint64_t CapacityBytes() {
  return IsMounted() ? CardenzaHAL::Storage::GetCapacity() : 0;
}

std::uint64_t FreeBytes() {
  std::uint64_t total = 0;
  std::uint64_t free = 0;
  return IsMounted() && esp_vfs_fat_info(kCardRoot, &total, &free) == ESP_OK ? free : 0;
}

bool ReadInternal(std::string_view key, std::string& value) {
  if (!nvsReady || !ValidInternalKey(key)) {
    return false;
  }
  nvs_handle_t handle = 0;
  if (nvs_open(kNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
    return false;
  }
  const std::string name(key);
  std::size_t length = 0;
  bool ok = nvs_get_blob(handle, name.c_str(), nullptr, &length) == ESP_OK && length <= kInternalValueMax;
  std::string data(ok ? length : 0, '\0');
  if (ok && length != 0) {
    ok = nvs_get_blob(handle, name.c_str(), data.data(), &length) == ESP_OK && length == data.size();
  }
  nvs_close(handle);
  if (ok) {
    value = std::move(data);
  }
  return ok;
}

bool WriteInternal(std::string_view key, std::string_view value) {
  if (!nvsReady || !ValidInternalKey(key) || value.size() > kInternalValueMax) {
    return false;
  }
  nvs_handle_t handle = 0;
  if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    return false;
  }
  const std::string name(key);
  const bool ok = nvs_set_blob(handle, name.c_str(), value.data(), value.size()) == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return ok;
}

bool EraseInternal(std::string_view key) {
  if (!nvsReady || !ValidInternalKey(key)) {
    return false;
  }
  nvs_handle_t handle = 0;
  if (nvs_open(kNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
    return false;
  }
  const std::string name(key);
  const esp_err_t status = nvs_erase_key(handle, name.c_str());
  const bool ok = (status == ESP_OK || status == ESP_ERR_NVS_NOT_FOUND) && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return ok;
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
  nvsReady = InitNvs();
  return true;
}

void Start() {}

}  // namespace Device

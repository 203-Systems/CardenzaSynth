#include "Devices/Device.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <string_view>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFDIR) != 0)
#endif
#endif

#include <esp_attr.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "computecardHAL.h"
#include "computecard/board/pins.h"

namespace {

std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
std::array<std::uint8_t, Device::LED::kLedCount> keyboardLedState{};
std::uint8_t keyboardLedGlobalBrightness = Device::LED::kBrightnessMax;
bool keyboardLedsEnabled = false;
bool homeButtonInitialized = false;
SemaphoreHandle_t framePresentDone = nullptr;
bool displayCallbacksInstalled = false;

constexpr std::string_view kStorageRoot = "/sdcard/ComputeCardOS";
constexpr std::string_view kSampleDirectory = "Sample";
constexpr std::string_view kPresetDirectory = "Preset";
constexpr std::string_view kProjectDirectory = "Project";

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

bool EnsureHomeButtonInitialized() {
  if (homeButtonInitialized) {
    return true;
  }

  gpio_config_t config{};
  config.pin_bit_mask = 1ULL << ComputeCardHAL::Pins::kHomeButton;
  config.mode = GPIO_MODE_INPUT;
  config.pull_up_en = GPIO_PULLUP_ENABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  if (gpio_config(&config) != ESP_OK) {
    return false;
  }

  homeButtonInitialized = true;
  return true;
}

std::string JoinPath(std::string_view left, std::string_view right) {
  std::string path(left);
  if (!path.empty() && path.back() != '/') {
    path.push_back('/');
  }
  path.append(right);
  return path;
}

bool DirectoryExists(const std::string& path) {
  struct stat info {};
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool EnsureDirectoryExists(const std::string& path) {
  if (DirectoryExists(path)) {
    return true;
  }

  if (
#if defined(_WIN32)
      _mkdir(path.c_str())
#else
      mkdir(path.c_str(), 0775)
#endif
          == 0) {
    return true;
  }

  if (errno != EEXIST) {
    return false;
  }

  return DirectoryExists(path);
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
  if (!ComputeCardHAL::Display::Init()) {
    return;
  }

  const esp_lcd_panel_handle_t panel = ComputeCardHAL::Display::GetPanel();
  const esp_lcd_panel_io_handle_t panelIo = ComputeCardHAL::Display::GetPanelIO();
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

namespace Buttons {

bool HomePressed() {
  return EnsureHomeButtonInitialized() &&
         gpio_get_level(static_cast<gpio_num_t>(ComputeCardHAL::Pins::kHomeButton)) == 0;
}

}  // namespace Buttons

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

namespace Storage {

std::string RootPath() {
  return std::string(kStorageRoot);
}

std::string SampleDirectoryPath() {
  return JoinPath(RootPath(), kSampleDirectory);
}

std::string PresetDirectoryPath() {
  return JoinPath(RootPath(), kPresetDirectory);
}

std::string PresetDirectoryPath(std::string_view instrument) {
  return JoinPath(PresetDirectoryPath(), instrument);
}

std::string ProjectDirectoryPath() {
  return JoinPath(RootPath(), kProjectDirectory);
}

bool EnsureDirectoryLayout() {
  if (!ComputeCardHAL::Storage::IsMounted() && !ComputeCardHAL::Storage::Init()) {
    return false;
  }

  const std::array<std::string, 7> directories = {
      RootPath(),
      SampleDirectoryPath(),
      PresetDirectoryPath(),
      PresetDirectoryPath("DX7"),
      PresetDirectoryPath("Juno"),
      PresetDirectoryPath("Sampler"),
      ProjectDirectoryPath(),
  };

  for (const std::string& path : directories) {
    if (!EnsureDirectoryExists(path)) {
      return false;
    }
  }

  return true;
}

}  // namespace Storage

std::string Name() {
  return "ComputeCard";
}

std::string BackendName() {
  return "hardware-hal";
}

bool Init() {
  const bool initialized = ComputeCardHAL::Init();
  if (!initialized) {
    return false;
  }

  (void)EnsureHomeButtonInitialized();

  if (ComputeCardHAL::Storage::IsMounted()) {
    (void)Storage::EnsureDirectoryLayout();
  }

  return true;
}

void Start() {}

}  // namespace Device

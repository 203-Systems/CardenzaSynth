#include "Devices/Device.h"
#include "Devices/LedFrameBuffer.h"

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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "cardenzaHAL.h"

namespace {

std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
Device::LED::detail::LedFrameBuffer keyboardLedFrames;
SemaphoreHandle_t framePresentDone = nullptr;
bool displayCallbacksInstalled = false;

constexpr std::string_view kStorageRoot = "/sdcard/CardenzaSynth";
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

void PublishLedFrame(bool changed) {
  if (!changed) return;
  const auto& frame = keyboardLedFrames.Current();
  CardenzaHAL::Keyboard::setLEDFrame(frame.levels, frame.master, frame.enabled);
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
  if (!CardenzaHAL::Storage::IsMounted() && !CardenzaHAL::Storage::Init()) {
    return false;
  }

  const std::array<std::string, 8> directories = {
      RootPath(),
      SampleDirectoryPath(),
      PresetDirectoryPath(),
      PresetDirectoryPath("DX7"),
      PresetDirectoryPath("Juno"),
      PresetDirectoryPath("Sampler"),
      PresetDirectoryPath("Drums"),
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

  if (CardenzaHAL::Storage::IsMounted()) {
    (void)Storage::EnsureDirectoryLayout();
  }

  return true;
}

void Start() {}

}  // namespace Device

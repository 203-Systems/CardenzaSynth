#include "Devices/Device.h"
#include "Devices/LedFrameBuffer.h"
#include "Devices/CardenzaSim/SimulatorInput.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace {

namespace fs = std::filesystem;

std::array<Device::Display::Pixel, Device::Display::kPixelCount> frameBuffer{};
Device::Keyboard::MatrixKeyState keyboardState{};
bool homeButtonPressed = false;
Device::LED::detail::LedFrameBuffer keyboardLedFrames;
bool audioInputEnabled = false;
// The simulator has no backlight; keep the level so settings round-trip like on hardware.
std::atomic<std::uint8_t> backlightLevel{Device::Display::kDefaultBacklight};

constexpr bool IsValidMatrixPosition(std::size_t row, std::size_t column) {
  return row < static_cast<std::size_t>(Device::LED::kRows) &&
         column < static_cast<std::size_t>(Device::LED::kColumns);
}

constexpr std::size_t MatrixIndex(std::size_t row, std::size_t column) {
  return row * static_cast<std::size_t>(Device::LED::kColumns) + column;
}

fs::path SimulatorExecutableDirectory() {
#if defined(_WIN32)
  std::array<char, MAX_PATH> buffer{};
  const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length != 0 && length < buffer.size()) {
    return fs::path(std::string(buffer.data(), length)).parent_path();
  }
#endif

  std::error_code error;
  const fs::path currentDirectory = fs::current_path(error);
  return error ? fs::path{"."} : currentDirectory;
}

const fs::path& StorageRootDirectory() {
  static const fs::path root = SimulatorExecutableDirectory() / "sdcard" / "CardenzaSynth";
  return root;
}

std::string PathString(const fs::path& path) {
  return path.generic_string();
}

}  // namespace

namespace CardenzaSim::Input {

void SetMatrixKey(std::size_t row, std::size_t column, bool pressed) {
  if (!IsValidMatrixPosition(row, column)) {
    return;
  }

  keyboardState.set(MatrixIndex(row, column), pressed);
}

void SetHomeButton(bool pressed) {
  homeButtonPressed = pressed;
}

void ClearMatrixKeys() {
  keyboardState.reset();
}

}  // namespace CardenzaSim::Input

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

void SetBacklight(std::uint8_t level) {
  backlightLevel.store(level, std::memory_order_relaxed);
}

std::uint8_t Backlight() {
  return backlightLevel.load(std::memory_order_relaxed);
}

}  // namespace Display

namespace Keyboard {

const MatrixKeyState& State() {
  return keyboardState;
}

}  // namespace Keyboard

namespace Buttons {

bool HomePressed() {
  return homeButtonPressed;
}

}  // namespace Buttons

namespace LED {

void BeginFrame() { keyboardLedFrames.BeginFrame(); }
void Present() { (void)keyboardLedFrames.Present(); }

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

  (void)keyboardLedFrames.SetKey(row, column, brightness);
}

void Clear() {
  (void)keyboardLedFrames.Fill(kBrightnessOff);
}

void Fill(std::uint8_t brightness) {
  (void)keyboardLedFrames.Fill(brightness);
}

void SetGlobalBrightness(std::uint8_t brightness) {
  (void)keyboardLedFrames.SetMaster(brightness);
}

std::uint8_t GlobalBrightness() {
  return keyboardLedFrames.Current().master;
}

void SetEnabled(bool enabled) {
  (void)keyboardLedFrames.SetEnabled(enabled);
}

bool Enabled() {
  return keyboardLedFrames.Current().enabled;
}

}  // namespace LED

namespace Storage {

std::string RootPath() {
  return PathString(StorageRootDirectory());
}

std::string SampleDirectoryPath() {
  return PathString(StorageRootDirectory() / "Sample");
}

std::string PresetDirectoryPath() {
  return PathString(StorageRootDirectory() / "Preset");
}

std::string PresetDirectoryPath(std::string_view instrument) {
  return PathString(StorageRootDirectory() / "Preset" / std::string(instrument));
}

std::string ProjectDirectoryPath() {
  return PathString(StorageRootDirectory() / "Project");
}

bool EnsureDirectoryLayout() {
  std::error_code error;
  const std::array<fs::path, 8> directories = {
      StorageRootDirectory(),
      StorageRootDirectory() / "Sample",
      StorageRootDirectory() / "Preset",
      StorageRootDirectory() / "Preset" / "DX7",
      StorageRootDirectory() / "Preset" / "Juno",
      StorageRootDirectory() / "Preset" / "Sampler",
      StorageRootDirectory() / "Preset" / "Drums",
      StorageRootDirectory() / "Project",
  };

  for (const fs::path& path : directories) {
    fs::create_directories(path, error);
    if (error) {
      return false;
    }
  }

  return true;
}

}  // namespace Storage

std::string Name() {
  return "CardenzaSim";
}

std::string BackendName() {
  return "sim-host";
}

bool Init() {
  (void)Storage::EnsureDirectoryLayout();
  return true;
}

void Start() {}

}  // namespace Device

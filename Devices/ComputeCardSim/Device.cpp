#include "Devices/Device.h"
#include "Devices/ComputeCardSim/SimulatorInput.h"

#include <algorithm>
#include <array>
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
std::array<std::uint8_t, Device::LED::kLedCount> keyboardLedState{};
std::uint8_t keyboardLedGlobalBrightness = Device::LED::kBrightnessMax;
bool keyboardLedsEnabled = false;
bool audioInputEnabled = false;

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
  static const fs::path root = SimulatorExecutableDirectory() / "sdcard" / "ComputeCardOS";
  return root;
}

std::string PathString(const fs::path& path) {
  return path.generic_string();
}

}  // namespace

namespace ComputeCardSim::Input {

void SetMatrixKey(std::size_t row, std::size_t column, bool pressed) {
  if (!IsValidMatrixPosition(row, column)) {
    return;
  }

  keyboardState.set(MatrixIndex(row, column), pressed);
}

void ClearMatrixKeys() {
  keyboardState.reset();
}

}  // namespace ComputeCardSim::Input

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
  const std::array<fs::path, 7> directories = {
      StorageRootDirectory(),
      StorageRootDirectory() / "Sample",
      StorageRootDirectory() / "Preset",
      StorageRootDirectory() / "Preset" / "DX7",
      StorageRootDirectory() / "Preset" / "Juno",
      StorageRootDirectory() / "Preset" / "Sampler",
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
  return "ComputeCardSim";
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

// Device::Storage for host builds: the "card" is the sdcard/ folder in the working directory
// (beside the executable on Windows). Self-contained so other host devices can compile it unchanged.
#include "Devices/Device.h"
#include "Devices/CardenzaSim/SimulatorInput.h"

#include <array>
#include <atomic>
#include <filesystem>
#include <string>
#include <system_error>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace {

namespace fs = std::filesystem;

std::atomic<bool> cardInserted{true};

fs::path BaseDirectory() {
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

// Fixed on first use, so a host that changes the working directory before boot picks its folder.
const fs::path& Base() {
  static const fs::path base = BaseDirectory();
  return base;
}

fs::path CardDirectory() {
  return Base() / "sdcard";
}

}  // namespace

namespace CardenzaSim::Input {

void SetCardInserted(bool inserted) {
  cardInserted.store(inserted);
}

}  // namespace CardenzaSim::Input

namespace Device::Storage {

void NotifyWriteFailure() {} // Host filesystem failures do not unmount its volume.

std::string RootPath() {
  return CardDirectory().generic_string();
}

bool Mount() {
  if (!cardInserted.load()) {
    return false;
  }
  std::error_code error;
  fs::create_directories(CardDirectory(), error);
  return fs::is_directory(CardDirectory(), error);
}

bool IsMounted() {
  std::error_code error;
  return cardInserted.load() && fs::is_directory(CardDirectory(), error);
}

std::uint64_t CapacityBytes() {
  std::error_code error;
  const fs::space_info info = fs::space(CardDirectory(), error);
  return IsMounted() && !error ? static_cast<std::uint64_t>(info.capacity) : 0;
}

std::uint64_t FreeBytes() {
  std::error_code error;
  const fs::space_info info = fs::space(CardDirectory(), error);
  return IsMounted() && !error ? static_cast<std::uint64_t>(info.available) : 0;
}

}  // namespace Device::Storage

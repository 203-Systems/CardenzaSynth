// Device::Storage for host builds: the "card" is the sdcard/ folder and internal flash is the
// internal/ folder, both in the working directory (beside the executable on Windows). Self-contained
// so other host devices can compile it unchanged.
#include "Devices/Device.h"
#include "Devices/CardenzaSim/SimulatorInput.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

bool ValidInternalKey(std::string_view key) {
  return !key.empty() && key.size() <= Device::Storage::kInternalKeyMax &&
         std::all_of(key.begin(), key.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

fs::path InternalPath(std::string_view key) {
  return Base() / "internal" / (std::string(key) + ".rec");
}

}  // namespace

namespace CardenzaSim::Input {

void SetCardInserted(bool inserted) {
  cardInserted.store(inserted);
}

}  // namespace CardenzaSim::Input

namespace Device::Storage {

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

void CheckAfterIoError() {}

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

bool ReadInternal(std::string_view key, std::string& value) {
  if (!ValidInternalKey(key)) {
    return false;
  }
  std::ifstream file(InternalPath(key), std::ios::binary);
  if (!file) {
    return false;
  }
  std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (file.bad() || contents.size() > kInternalValueMax) {
    return false;
  }
  value = std::move(contents);
  return true;
}

std::vector<std::string> ListInternal() {
  std::vector<std::string> keys;
  std::error_code error;
  for (const fs::directory_entry& entry : fs::directory_iterator(Base() / "internal", error)) {
    if (entry.path().extension() == ".rec") {
      keys.push_back(entry.path().stem().string());
    }
  }
  return keys;
}

bool EraseInternal(std::string_view key) {
  if (!ValidInternalKey(key)) {
    return false;
  }
  std::error_code error;
  fs::remove(InternalPath(key), error);
  return !error;
}

}  // namespace Device::Storage

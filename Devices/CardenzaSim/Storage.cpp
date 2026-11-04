// Device::Storage for host builds: the "card" is the sdcard/ folder in the working directory, so each
// test run gets its own card by starting in a folder of its own (the desktop simulator starts beside its
// executable). Like the device, it keeps nothing anywhere else. Self-contained so other host devices
// can compile it unchanged.
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

namespace {

namespace fs = std::filesystem;

std::atomic<bool> cardInserted{true};

fs::path BaseDirectory() {
  // The working directory everywhere: on Windows this was the executable's folder, which every test
  // shared (they start in a scratch folder of their own), so their cards and records ran into each
  // other across tests and runs.
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

}  // namespace Device::Storage

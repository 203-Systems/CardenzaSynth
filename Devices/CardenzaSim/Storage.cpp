// Device::Storage for host builds: the "card" is the sdcard/ folder in the working directory, so each
// test run gets its own card by starting in a folder of its own (the desktop simulator starts beside its
// executable). Like the device, it keeps nothing anywhere else. Self-contained so other host devices
// can compile it unchanged, with CardenzaHAL's pure storage/app_dir_logic.cpp beside it: the firmware's
// folder is moved by the same code as on the device.
#include "Devices/Device.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
#include "Devices/Cardenza/CardenzaHAL/storage/app_dir_logic.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
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

// The card for CardenzaHAL's migration: each call is one operation, as FAT's on the device.
class HostCard final : public CardenzaHAL::Storage::AppDirLogic::FileSystem {
public:
  Kind Stat(const std::string& path) override {
    std::error_code error;
    const fs::file_status status = fs::status(fs::u8path(path), error);
    if (error || !fs::exists(status)) return Kind::None;
    return fs::is_directory(status) ? Kind::Directory : Kind::File;
  }
  bool List(const std::string& path, std::vector<std::string>& names) override {
    std::error_code error;
    fs::directory_iterator it(fs::u8path(path), error);
    if (error) return false;
    for (fs::directory_iterator end; it != end; it.increment(error)) {
      if (error) return false;
      names.push_back(it->path().filename().u8string());
    }
    return true;
  }
  bool Rename(const std::string& from, const std::string& to) override {
    // Never over an existing name, as on FAT (POSIX rename would replace an empty folder or a file).
    std::error_code error;
    if (fs::exists(fs::u8path(to), error)) return false;
    fs::rename(fs::u8path(from), fs::u8path(to), error);
    return !error;
  }
  bool MakeDirectory(const std::string& path) override {
    std::error_code error;
    return fs::create_directory(fs::u8path(path), error) && !error;
  }
  bool RemoveDirectory(const std::string& path) override {
    std::error_code error;
    return fs::is_directory(fs::u8path(path), error) && fs::is_empty(fs::u8path(path), error) && fs::remove(fs::u8path(path), error) && !error;
  }
};

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

std::string AppRoot() {
  return (CardDirectory() / "Apps" / "CardenzaSynth").generic_string();
}

void MoveToAppRoot(const std::string& oldPath) {
  if (!IsMounted()) return;
  namespace Logic = CardenzaHAL::Storage::AppDirLogic;
  HostCard card;
  const std::string to = AppRoot();
  const auto report = Logic::Migrate(card, RootPath(), oldPath, to);
  const std::string line = Logic::Describe(report, oldPath, to);
  if (!line.empty()) std::printf("CardenzaStorage: %s\n", line.c_str());
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

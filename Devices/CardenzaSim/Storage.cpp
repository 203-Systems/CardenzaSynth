// Device::Storage for host builds: the "card" is the sdcard/ folder in the working directory, so each
// test run gets its own card by starting in a folder of its own (the desktop simulator starts beside its
// executable). Like the device, it keeps nothing anywhere else. Self-contained so other host devices
// can compile it unchanged.
#include "Devices/Device.h"
#include "Devices/CardenzaSim/SimulatorInput.h"
// CardenzaHAL's card layout and folder migration, pure (no ESP-IDF; storage/app_dir_logic.cpp is built in).
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

std::string AppPath() {
  // CardenzaHAL's layout, as on the device: Apps/<AppName>, the firmware's AppName is CardenzaSynth.
  return (CardDirectory() / "Apps" / "CardenzaSynth").generic_string();
}

bool MoveIntoAppPath(const std::string& oldPath) {
  if (!IsMounted()) return false;
  // The HAL's own migration (pure), over std::filesystem instead of ESP-IDF's VFS.
  struct HostFileSystem final : CardenzaHAL::Storage::AppDirLogic::FileSystem {
    Kind Stat(const std::string& path) override {
      std::error_code error;
      const fs::file_status status = fs::status(path, error);
      if (error || !fs::exists(status)) return Kind::None;
      return fs::is_directory(status) ? Kind::Directory : Kind::File;
    }
    bool List(const std::string& path, std::vector<std::string>& names) override {
      std::error_code error;
      for (fs::directory_iterator entry(path, error), end; !error && entry != end; entry.increment(error))
        names.push_back(entry->path().filename().generic_string());
      return !error;
    }
    bool Rename(const std::string& from, const std::string& to) override {
      std::error_code error;
      if (fs::exists(to, error)) return false;
      fs::rename(from, to, error);
      return !error;
    }
    bool MakeDirectory(const std::string& path) override {
      std::error_code error;
      return fs::create_directory(path, error) && !error;
    }
    bool RemoveDirectory(const std::string& path) override {
      std::error_code error;
      return fs::is_empty(path, error) && !error && fs::remove(path, error) && !error;
    }
  } card;
  const std::string to = AppPath();
  const auto report = CardenzaHAL::Storage::AppDirLogic::Migrate(card, RootPath(), oldPath, to);
  const std::string line = CardenzaHAL::Storage::AppDirLogic::Describe(report, oldPath, to);
  if (!line.empty()) std::printf("CardenzaStorage: %s\n", line.c_str());
  return report.outcome != CardenzaHAL::Storage::MigrateReport::Outcome::Failed;
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

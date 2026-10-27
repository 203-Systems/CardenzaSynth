// SD-only settings persistence. Uses an isolated scratch card and device stubs, so the failure
// cases do not touch the simulator's normal card or depend on hardware/flash.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "Devices/Device.h"
#include "OS/Storage/Settings.h"
#include "OS/Storage/Storage.h"

namespace fs = std::filesystem;

namespace {

fs::path cardRoot;
bool cardInserted = true;

void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

void Write(const fs::path& path, const std::string& text)
{
  fs::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  file.close();
  Require(!file.fail(), "fixture write failed");
}

std::string Read(const fs::path& path)
{
  std::ifstream file(path, std::ios::binary);
  Require(file.good(), "fixture read failed");
  return {std::istreambuf_iterator<char>(file), {}};
}

struct ScratchCard {
  fs::path root = fs::temp_directory_path() /
      ("cardenza-settings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  ScratchCard()
  {
    Require(fs::create_directory(root), "scratch directory already exists");
    cardRoot = root / "sdcard";
    fs::create_directory(cardRoot);
  }

  ~ScratchCard()
  {
    std::error_code ignored;
    fs::remove_all(root, ignored);
  }
};

std::string Path(const char* name)
{
  return (cardRoot / "CardenzaSynth" / "Settings" / name).generic_string();
}

void MissingCardAndLateInsertion()
{
  const std::string path = Path("late.cfg");
  Write(path, "sys.volume=80\nsys.screen=140\nfuture.option=keep\nobsolete=yes\n");
  cardInserted = false;
  os::storage::Settings settings("test", path);
  settings.Load();
  Require(settings.GetInt("sys.volume", 17) == 17, "missing card must use the caller's default");
  Require(settings.SetInt("sys.volume", 25), "RAM edit failed");
  Require(settings.Remove("obsolete"), "removal must be recorded before the card is available");
  Require(!settings.Flush() && settings.Dirty(), "missing card must not count as saved");
  Require(Read(path).find("sys.volume=80\n") != std::string::npos, "absent card was changed");
  Require(!settings.RefreshFromCard() && settings.GetInt("sys.volume", 0) == 25,
          "failed refresh must keep RAM edits");
  cardInserted = true;
  Require(settings.RefreshFromCard() && settings.Dirty(), "refresh must preserve pending edits");
  Require(settings.GetInt("sys.volume", 0) == 25 && settings.GetInt("sys.screen", 0) == 140 &&
          settings.GetString("future.option") == "keep" && !settings.Has("obsolete"),
          "late insertion did not rebase individual edits");
  Require(settings.Flush() && !settings.Dirty(), "card save failed");
  os::storage::Settings reloaded("test", path);
  reloaded.Load();
  Require(reloaded.GetInt("sys.volume", 0) == 25 && reloaded.GetInt("sys.screen", 0) == 140 &&
          reloaded.GetString("future.option") == "keep" && !reloaded.Has("obsolete"),
          "saved card lost untouched keys or restored a removed key");
  Require(!fs::exists(cardRoot.parent_path() / "internal"), "unexpected internal persistence");
}

void RebaseAtFlushAndCardSwap()
{
  const std::string path = Path("swap.cfg");
  Write(path, "volume=10\nold.card.only=a\nunchanged=20\n");
  os::storage::Settings settings("test", path);
  settings.Load();
  Require(settings.SetInt("volume", 40), "edit failed");
  // A different card (or a computer edit) appeared after Load; only volume belongs to our edit.
  Write(path, "volume=90\nnew.card.only=b\nunchanged=80\n");
  Require(settings.Flush(), "rebase-on-flush failed");
  Require(settings.GetInt("volume", 0) == 40 && settings.GetInt("unchanged", 0) == 80 &&
          settings.GetString("new.card.only") == "b" && !settings.Has("old.card.only"),
          "flush copied stale, untouched settings from the old card");
  Require(settings.Remove("new.card.only") && settings.SetString("new.card.only", "restored") &&
          settings.Flush(), "set after a removal failed");
  Require(Read(path).find("new.card.only=restored\n") != std::string::npos, "a superseded tombstone survived");
}

void ReadFailureAndWriteFailureKeepEdits()
{
  const std::string path = Path("failure.cfg");
  Write(path, "existing=keep\n");
  os::storage::Settings settings("test", path);
  settings.Load();
  Require(settings.SetInt("volume", 50), "edit failed");
  // A directory at the target is an invalid file, not an empty settings store.
  fs::remove(path);
  fs::create_directory(path);
  Require(!settings.RefreshFromCard() && !settings.Flush() && settings.Dirty(),
          "non-regular target must fail without clearing edits");
  Require(settings.GetString("existing") == "keep", "failed read replaced the RAM snapshot");
  fs::remove(path);
  const std::string oversized(os::storage::kMaxSettingsFileBytes + 1, 'x');
  Write(path, oversized);
  Require(!settings.Flush() && settings.Dirty() && Read(path) == oversized,
          "oversized existing file must not be overwritten");
  // A conflicting parent produces ENOTDIR on POSIX (or an equivalent failure on Windows).
  const std::string parent = Path("blocked");
  Write(parent, "not a folder");
  os::storage::Settings blocked("test", parent + "/settings.cfg");
  Require(blocked.SetInt("volume", 9) && !blocked.Flush() && blocked.Dirty() && Read(parent) == "not a folder",
          "conflicting parent must not be treated as an empty writable store");
  // Make atomic creation fail independently of read success by obstructing its temporary path.
  Write(path, "existing=keep\n");
  fs::create_directory(path + ".tmp");
  Require(!settings.Flush() && settings.Dirty() && Read(path) == "existing=keep\n",
          "failed write must preserve the old file and pending edits");
  fs::remove(path + ".tmp");
  Require(settings.Flush() && !settings.Dirty(), "retry did not persist pending edits");
}

void SizeLimitAndRecovery()
{
  const std::string path = Path("limit.cfg");
  Write(path, "existing=keep\n");
  os::storage::Settings settings("test", path);
  settings.Load();
  const std::string value(200, 'a');
  for (int index = 0; index < 90; ++index)
    Require(settings.SetString("key" + std::to_string(index), value), "large RAM edit failed");
  Require(!settings.Flush() && settings.Dirty() && Read(path) == "existing=keep\n",
          "oversized output must not replace the saved settings");
  for (int index = 0; index < 30; ++index) Require(settings.Remove("key" + std::to_string(index)), "remove failed");
  Require(settings.Flush() && fs::file_size(path) <= os::storage::kMaxSettingsFileBytes,
          "smaller edit set did not recover from the size limit");

  const std::string recovery = Path("recovery.cfg");
  Write(recovery + ".bak", "future.key=retained\nvolume=61\n");
  os::storage::Settings recovered("test", recovery);
  recovered.Load();
  Require(recovered.GetInt("volume", 0) == 61 && recovered.SetInt("volume", 62) && recovered.Flush(),
          "interrupted replacement was not recovered");
  Require(Read(recovery).find("future.key=retained\n") != std::string::npos && !fs::exists(recovery + ".bak"),
          "recovery lost an untouched key or left a stale recovery file");
}

void TolerantFormatAndRegistryRefresh()
{
  const std::string path = Path("format.cfg");
  Write(path, "# manual edit\nvolume=10\ninvalid line\nfuture.key=kept\nvolume=11\n");
  os::storage::Settings settings("test", path);
  settings.Load();
  Require(settings.GetInt("volume", 0) == 11 && settings.SetInt("volume", 12) && settings.Flush(),
          "tolerant text compatibility changed");
  Require(Read(path).find("future.key=kept\n") != std::string::npos, "unknown valid key was dropped");

  cardInserted = false;
  auto& system = os::storage::SystemSettings();
  Require(system.SetInt("sys.volume", 29), "registry edit failed");
  Write(os::storage::SystemSettingsPath(), "sys.screen=150\nfuture.system=keep\n");
  cardInserted = true;
  Require(os::storage::RefreshAllSettingsFromCard() && os::storage::AnySettingsDirty(),
          "registry refresh did not preserve pending edits");
  Require(system.GetInt("sys.volume", 0) == 29 && system.GetInt("sys.screen", 0) == 150 &&
          system.GetString("future.system") == "keep" && os::storage::FlushAllSettings(),
          "registry refresh/flush failed to merge disk keys");
}

}  // namespace

namespace Device::Storage {

std::string RootPath() { return cardRoot.generic_string(); }
bool Mount() { return cardInserted && fs::is_directory(cardRoot); }
bool IsMounted() { return Mount(); }
std::uint64_t CapacityBytes() { return Mount() ? fs::space(cardRoot).capacity : 0; }
std::uint64_t FreeBytes() { return Mount() ? fs::space(cardRoot).available : 0; }

}  // namespace Device::Storage

int main()
{
  try
  {
    ScratchCard scratch;
    MissingCardAndLateInsertion();
    RebaseAtFlushAndCardSwap();
    ReadFailureAndWriteFailureKeepEdits();
    SizeLimitAndRecovery();
    TolerantFormatAndRegistryRefresh();
    std::cout << "SD-only settings tests passed\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

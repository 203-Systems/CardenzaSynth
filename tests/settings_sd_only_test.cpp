// SD-only settings persistence. Uses an isolated scratch card and device stubs, so the failure
// cases do not touch the simulator's normal card or depend on hardware/flash. Last, a run that starts
// without a card: it asks the device for nothing and touches no file on the card.
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
unsigned writeFailureChecks = 0;
unsigned mountCalls = 0;

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

void FailedSaveNotifications()
{
  const std::string path = Path("write-notification.cfg");
  const auto before = writeFailureChecks;
  Require(os::storage::WriteFileAtomic(path, "value=1\n") && writeFailureChecks == before,
          "a successful write added a card-status check");
  fs::create_directory(path + ".tmp");
  Require(!os::storage::WriteFileAtomic(path, "value=2\n") && writeFailureChecks == before + 1,
          "failed atomic write did not notify the device");
  fs::remove(path + ".tmp");
  os::storage::Settings settings("test", path);
  settings.Load();
  settings.SetInt("value", 3);
  fs::remove(path);
  fs::create_directory(path); // prerequisite read fails before any write can open
  Require(!settings.Flush() && settings.Dirty() && writeFailureChecks == before + 2,
          "a failed pre-save read bypassed card recovery");
  fs::remove(path);
  Require(settings.Flush() && !settings.Dirty(), "save did not recover after an I/O failure");
}

// The system decides at its start whether the run has a card. Without one nothing asks the device for
// the card, every file on it is refused, and a card put in later changes nothing.
void RunWithoutCard()
{
  const std::string path = Path("nocard.cfg");
  Write(path, "sys.volume=80\n");
  cardInserted = false;
  Require(!os::storage::DecideCardAtStart() && os::storage::CardMissingThisRun(), "a start without a card was not noticed");
  cardInserted = true;  // put in after the start
  const unsigned mounts = mountCalls;
  const unsigned checks = writeFailureChecks;
  Require(!os::storage::CardPresent() && !os::storage::CardMounted() && os::storage::CardFreeBytes() == 0 &&
              !os::storage::EnsureSystemLayout(),
          "a run without a card used the card put in later");
  Require(!os::storage::Exists(path) && !os::storage::IsFile(path) && os::storage::FileSize(path) < 0 &&
              !os::storage::IsDirectory(os::storage::Root()) && os::storage::ListDirectory(os::storage::SystemDirectory()).empty(),
          "a run without a card read the card");
  std::string text;
  Require(!os::storage::ReadFileWithBackup(path, text, 1024) && !os::storage::WriteFileAtomic(path, "x=1\n") &&
              !os::storage::CreateDirectories(os::storage::JoinPath(os::storage::Root(), "New")) && !os::storage::RemoveFile(path),
          "a run without a card wrote to the card");
  os::storage::Settings settings("test", path);
  settings.Load();
  Require(settings.GetInt("sys.volume", 17) == 17 && settings.SetInt("sys.volume", 25) && !settings.RefreshFromCard() &&
              !settings.Flush() && settings.Dirty() && settings.GetInt("sys.volume", 0) == 25,
          "settings without a card are not kept in RAM");
  os::storage::ReportCardIoError();
  Require(mountCalls == mounts && writeFailureChecks == checks, "a run without a card asked the device about the card");
  Require(Read(path) == "sys.volume=80\n" && !fs::exists(cardRoot / "CardenzaSynth" / "New") && !fs::exists(path + ".tmp"),
          "the card changed in a run without one");
  // The next start finds it.
  Require(os::storage::DecideCardAtStart() && !os::storage::CardMissingThisRun() && os::storage::Exists(path),
          "a start with the card did not find it");
}

}  // namespace

namespace Device::Storage {

void CheckAfterIoError() { ++writeFailureChecks; }

std::string RootPath() { return cardRoot.generic_string(); }
bool Mount()
{
  ++mountCalls;
  return cardInserted && fs::is_directory(cardRoot);
}
bool IsMounted() { return cardInserted && fs::is_directory(cardRoot); }
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
    FailedSaveNotifications();
    RunWithoutCard();
    std::cout << "SD-only settings tests passed\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

#include "Applications/Synth/Runtime/SoundFont/SoundFonts.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#include "Devices/Device.h"
#include "Library/Memory/Memory.h"
#include "Library/Synth/Audio/SoundFont/Sf2File.h"
#include "Library/Synth/Audio/SoundFont/Sf2Program.h"
#include "Library/Synth/Core/SynthStorage.h"
#include "Library/Synth/Core/SynthTypes.h"
#include "OS/Storage/Storage.h"

namespace synth {

namespace {

namespace sf2 = audio::sf2;

bool EndsWithExtension(const std::string& name)
{
  const std::string extension = kSoundFontExtension;
  if (name.size() <= extension.size()) return false;
  for (std::size_t index = 0; index < extension.size(); ++index)
    if (std::tolower(static_cast<unsigned char>(name[name.size() - extension.size() + index])) != extension[index]) return false;
  return true;
}

// The file on the card. It is open only while something is read from it (the card has five file
// slots for everything), and every read goes through a block in internal RAM: a patch's samples are
// read into PSRAM, which the card's DMA cannot reach.
class CardFile final : public sf2::ByteSource {
public:
  explicit CardFile(std::string path) : path_(std::move(path)) {}
  ~CardFile() override { Close(); }
  CardFile(const CardFile&) = delete;
  CardFile& operator=(const CardFile&) = delete;

  bool Open()
  {
    Close();
    failed_ = false;
    const long long size = os::storage::FileSize(path_);
    if (size <= 0) return false;
    size_ = static_cast<std::uint64_t>(size);
    // malloc is internal RAM on the device (Library/Memory/Memory.h).
    block_ = static_cast<std::uint8_t*>(std::malloc(kBlockBytes));
    file_ = block_ != nullptr ? std::fopen(path_.c_str(), "rb") : nullptr;
    if (file_ == nullptr)
    {
      Close();
      return false;
    }
    // Read through its descriptor: newlib reads an unbuffered FILE a byte at a time (AGENT.md).
    std::setvbuf(file_, nullptr, _IONBF, 0);
    return true;
  }

  void Close()
  {
    if (file_ != nullptr) std::fclose(file_);
    file_ = nullptr;
    std::free(block_);
    block_ = nullptr;
  }

  // A read of the card failed since it was opened (not: the file is too short).
  bool Failed() const { return failed_; }

  std::uint64_t Size() const override { return size_; }

  bool Read(std::uint64_t offset, void* destination, std::size_t count) override
  {
    if (file_ == nullptr || offset > size_ || count > size_ - offset || offset + count > static_cast<std::uint64_t>(LONG_MAX)) return false;
    auto* out = static_cast<std::uint8_t*>(destination);
    while (count > 0)
    {
      const std::size_t part = (std::min)(count, kBlockBytes);
      if (!ReadBlock(static_cast<long>(offset), part))
      {
        failed_ = true;
        return false;
      }
      std::memcpy(out, block_, part);
      out += part;
      offset += part;
      count -= part;
    }
    return true;
  }

private:
  static constexpr std::size_t kBlockBytes = 4096;

  bool ReadBlock(long offset, std::size_t bytes)
  {
#if defined(_WIN32)
    const int descriptor = _fileno(file_);
    if (_lseek(descriptor, offset, SEEK_SET) != offset) return false;
#else
    const int descriptor = fileno(file_);
    if (::lseek(descriptor, offset, SEEK_SET) != offset) return false;
#endif
    auto* cursor = reinterpret_cast<char*>(block_);
    while (bytes > 0)
    {
#if defined(_WIN32)
      const int got = _read(descriptor, cursor, static_cast<unsigned>(bytes));
#else
      const ssize_t got = ::read(descriptor, cursor, bytes);
#endif
      if (got <= 0) return false;
      cursor += got;
      bytes -= static_cast<std::size_t>(got);
    }
    return true;
  }

  std::string path_;
  std::FILE* file_ = nullptr;
  std::uint8_t* block_ = nullptr;
  std::uint64_t size_ = 0;
  bool failed_ = false;
};

const char* Explain(sf2::Status status)
{
  switch (status)
  {
    case sf2::Status::Ok: return "";
    case sf2::Status::ReadError: return "SD CARD ERROR";
    case sf2::Status::NotSoundFont: return "NOT A SOUNDFONT";
    case sf2::Status::Unsupported: return "FONT NOT SUPPORTED";
    case sf2::Status::Corrupt: return "BAD FONT FILE";
    case sf2::Status::TooLarge: return "PATCH TOO COMPLEX";
    case sf2::Status::NotFound: return "PATCH NOT IN FONT";
    case sf2::Status::TooBig: return "PATCH TOO BIG";
    case sf2::Status::NoMemory: return "NO MEMORY";
    case sf2::Status::Empty: return "EMPTY PATCH";
  }
  return "BAD FONT FILE";
}

class CardFont;

// Patches read on the device's background task (Device::Background), so the UI never waits for the
// card: one request a track, a newer one replacing one not started yet. What was read waits here for
// its track to take it (TakeLoaded); a result nobody wants any more is dropped by the task that read
// it. The ticket names a request: a track that asked again, or let go, takes nothing older.
struct LoadSlot {
  std::shared_ptr<CardFont> font;
  std::size_t index = 0;
  std::size_t budget = 0;
  std::uint32_t ticket = 0;
  bool waiting = false;  // asked for, not started
  bool done = false;
  std::shared_ptr<const sf2::Program> program;
  std::string message;
};
struct Loads {
  std::mutex mutex;
  // The UI can restore a project while the worker is reading another font. Only one builder
  // may measure and spend the shared PSRAM at once. Enqueuing and cancelling never take this lock.
  std::mutex buildMutex;
  std::array<LoadSlot, kSynthTrackCount> slots;
  std::uint32_t tickets = 0;
  bool deferred = false;  // tests
};
Loads g_loads;

void ReadRequestedPatches();

bool PatchFitsInPsram(std::uint64_t bytes)
{
  const std::size_t free = memory::SlowFree();
  return free >= soundfont::kSlowReserveBytes && bytes <= free - soundfont::kSlowReserveBytes;
}

class CardFont final : public soundfont::Font, public std::enable_shared_from_this<CardFont> {
public:
  CardFont(std::string fileName, std::string path) : fileName_(std::move(fileName)), source_(std::move(path)), reader_(source_, window_, sizeof(window_)) {}

  // Reads the layout and the list of patches.
  sf2::Status Open()
  {
    if (!source_.Open()) return sf2::Status::ReadError;
    sf2::Status status = file_.Open(reader_);
    if (status == sf2::Status::Ok)
    {
      count_ = file_.PresetCount();
      patches_.reset(new (memory::slow) soundfont::Patch[count_]);
      if (!patches_) status = sf2::Status::NoMemory;
    }
    for (std::size_t index = 0; status == sf2::Status::Ok && index < count_; ++index)
    {
      patches_[index].bank = file_.Preset(index).bank;
      patches_[index].program = file_.Preset(index).program;
      if (!file_.PresetName(index, patches_[index].name)) status = sf2::Status::ReadError;
    }
    if (status != sf2::Status::Ok) count_ = 0;
    const bool failed = source_.Failed();
    source_.Close();
    if (failed) os::storage::ReportCardIoError();
    return status;
  }

  std::size_t PatchCount() const override { return count_; }
  const soundfont::Patch& PatchAt(std::size_t index) const override { return patches_[index]; }

  std::shared_ptr<const sf2::Program> Load(std::size_t index, std::size_t budgetBytes, std::string& message) override
  {
    std::lock_guard<std::mutex> lock(g_loads.buildMutex);
    if (index >= count_)
    {
      message = Explain(sf2::Status::NotFound);
      return {};
    }
    const soundfont::Patch& patch = patches_[index];
    const auto begin = std::chrono::steady_clock::now();
    if (!os::storage::CardPresent() || !source_.Open())
    {
      message = os::storage::CardPresent() ? "FONT MISSING" : "NO SD CARD";
      std::printf("Groove: font %s could not be opened for %u:%u\n", fileName_.c_str(), static_cast<unsigned>(patch.bank), static_cast<unsigned>(patch.program));
      return {};
    }
    sf2::Program::Result result = sf2::Program::Build(file_, index, budgetBytes, memory::HasSlow() ? PatchFitsInPsram : nullptr);
    const bool failed = source_.Failed();
    source_.Close();
    if (failed) os::storage::ReportCardIoError();
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
    if (!result.program)
    {
      message = Explain(result.status);
      // How much it would take, so the user can tell a patch a little too big from a piano of 90 MB.
      if (result.status == sf2::Status::TooBig) message += " " + std::to_string((result.neededBytes + 1023) / 1024) + "K";
      std::printf("Groove: font %s patch %u:%u %s not loaded: %s (needs %u KB, may take %u KB)\n", fileName_.c_str(), static_cast<unsigned>(patch.bank),
                  static_cast<unsigned>(patch.program), patch.name, message.c_str(), static_cast<unsigned>(result.neededBytes / 1024),
                  static_cast<unsigned>(budgetBytes / 1024));
      return {};
    }
    std::printf("Groove: font %s patch %u:%u %s loaded: %u regions, %u samples, %u KB in %u ms\n", fileName_.c_str(), static_cast<unsigned>(patch.bank),
                static_cast<unsigned>(patch.program), patch.name, static_cast<unsigned>(result.program->RegionCount()),
                static_cast<unsigned>(result.program->SlotCount()), static_cast<unsigned>(result.program->Bytes() / 1024), static_cast<unsigned>(took));
    message.clear();
    return std::shared_ptr<const sf2::Program>(std::move(result.program));
  }

  std::uint32_t LoadLater(std::size_t owner, std::size_t index, std::size_t budgetBytes) override
  {
    if (owner >= kSynthTrackCount || index >= count_) return 0;
    std::shared_ptr<const sf2::Program> dropped;
    std::uint32_t ticket = 0;
    bool deferred = false;
    {
      std::lock_guard<std::mutex> lock(g_loads.mutex);
      LoadSlot& slot = g_loads.slots[owner];
      dropped = std::move(slot.program);
      slot = LoadSlot{};
      slot.font = shared_from_this();
      slot.index = index;
      slot.budget = budgetBytes;
      if (++g_loads.tickets == 0) ++g_loads.tickets;  // 0 is no ticket
      slot.ticket = ticket = g_loads.tickets;
      slot.waiting = true;
      deferred = g_loads.deferred;
    }
    if (deferred || Device::Background::Run(&ReadRequestedPatches)) return ticket;
    // No task for it: the caller reads the patch itself.
    CancelLoad(ticket);
    return 0;
  }

  bool TakeLoaded(std::size_t owner, std::uint32_t ticket, std::shared_ptr<const sf2::Program>& program, std::string& message) override
  {
    if (owner >= kSynthTrackCount || ticket == 0) return false;
    std::shared_ptr<CardFont> font;  // let go of outside the lock
    std::lock_guard<std::mutex> lock(g_loads.mutex);
    LoadSlot& slot = g_loads.slots[owner];
    if (slot.ticket != ticket || !slot.done) return false;
    program = std::move(slot.program);
    message = std::move(slot.message);
    font = std::move(slot.font);
    slot = LoadSlot{};
    return true;
  }

  void CancelLoad(std::uint32_t ticket) override
  {
    if (ticket == 0) return;
    LoadSlot dropped;  // release the font and program after unlocking
    std::lock_guard<std::mutex> lock(g_loads.mutex);
    for (LoadSlot& slot : g_loads.slots)
      if (slot.ticket == ticket && slot.font.get() == this)
      {
        std::swap(dropped, slot);
        return;
      }
  }

private:
  std::string fileName_;
  CardFile source_;
  std::uint8_t window_[512] = {};
  sf2::Reader reader_;
  sf2::File file_;
  std::unique_ptr<soundfont::Patch[]> patches_;
  std::size_t count_ = 0;
};

// The background task's job: every request waiting, one at a time, until none is left.
void ReadRequestedPatches()
{
  for (;;)
  {
    std::shared_ptr<CardFont> font;
    std::size_t owner = kSynthTrackCount;
    std::size_t index = 0;
    std::size_t budget = 0;
    std::uint32_t ticket = 0;
    {
      std::lock_guard<std::mutex> lock(g_loads.mutex);
      for (std::size_t slot = 0; slot < g_loads.slots.size() && owner == kSynthTrackCount; ++slot)
      {
        LoadSlot& request = g_loads.slots[slot];
        if (!request.waiting) continue;
        request.waiting = false;
        owner = slot;
        font = request.font;
        index = request.index;
        budget = request.budget;
        ticket = request.ticket;
      }
    }
    if (owner == kSynthTrackCount) return;
    std::string message;
    std::shared_ptr<const sf2::Program> program = font->Load(index, budget, message);
    std::shared_ptr<const sf2::Program> dropped;
    {
      std::lock_guard<std::mutex> lock(g_loads.mutex);
      LoadSlot& slot = g_loads.slots[owner];
      if (slot.ticket == ticket && !slot.waiting)
      {
        slot.program = std::move(program);
        slot.message = std::move(message);
        slot.done = true;
      }
      else
      {
        // Asked again or let go meanwhile: read for nobody, freed here.
        dropped = std::move(program);
      }
    }
  }
}

}  // namespace

void DeferSoundFontLoadsForTests(bool defer)
{
  std::lock_guard<std::mutex> lock(g_loads.mutex);
  g_loads.deferred = defer;
}

void RunDeferredSoundFontLoads() { ReadRequestedPatches(); }

bool IsSoundFontFileName(const std::string& fileName)
{
  return fileName.size() <= 64 && EndsWithExtension(fileName) && fileName.front() != '.' &&
         fileName.find_first_of("/\\:\"") == std::string::npos &&
         std::all_of(fileName.begin(), fileName.end(), [](unsigned char c) { return c >= 32 && c < 127; });
}

std::vector<std::string> ListSoundFonts()
{
  std::vector<std::string> names;
  if (!os::storage::CardPresent()) return names;
  for (const os::storage::DirectoryEntry& entry : os::storage::ListDirectory(storage::SoundFontDirectory()))
    if (!entry.directory && IsSoundFontFileName(entry.name)) names.push_back(entry.name);
  std::sort(names.begin(), names.end());
  return names;
}

std::shared_ptr<soundfont::Font> OpenSoundFont(const std::string& fileName, std::string& message)
{
  if (!os::storage::CardPresent())
  {
    message = "NO SD CARD";
    return {};
  }
  const std::string path = os::storage::JoinPath(storage::SoundFontDirectory(), fileName);
  if (!IsSoundFontFileName(fileName) || os::storage::FileSize(path) <= 0)
  {
    message = "FONT MISSING";
    return {};
  }
  std::shared_ptr<CardFont> font(new (std::nothrow) CardFont(fileName, path));
  if (!font)
  {
    message = "NO MEMORY";
    return {};
  }
  const sf2::Status status = font->Open();
  if (status != sf2::Status::Ok)
  {
    message = Explain(status);
    std::printf("Groove: font %s not opened: %s\n", fileName.c_str(), message.c_str());
    return {};
  }
  std::printf("Groove: font %s opened: %u patches\n", fileName.c_str(), static_cast<unsigned>(font->PatchCount()));
  message.clear();
  return font;
}

std::string SoundFontLabel(const std::string& fileName)
{
  std::string label = fileName;
  if (EndsWithExtension(label)) label.resize(label.size() - std::string(kSoundFontExtension).size());
  std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return label;
}

}  // namespace synth

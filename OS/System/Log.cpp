#include "OS/System/Log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>

#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Storage.h"

namespace os::log {

namespace {

// Taken from the ring and written in pieces this size (on the stack of the task that writes).
constexpr std::size_t kChunkBytes = 256;
// A write that takes longer than this says so in the log.
constexpr long long kSlowWriteMs = 40;

std::uint32_t g_lastWriteAt = 0;
bool g_background = true;  // the device's background task writes (Device::Background), until it says it cannot
std::atomic<bool> g_writeAsked{false};  // the background task runs every job when woken: this one only when asked

// One writer at a time: the device's writer task, or the system loop at boot, app switch and shutdown.
// The settings and the card's state belong to the system loop: it decides whether to write and where,
// the writer only appends.
std::mutex g_writeMutex;
std::string g_path;          // under g_writeMutex
std::string g_previousPath;  // under g_writeMutex
std::string g_directory;     // under g_writeMutex
bool g_directoryReady = false;  // under g_writeMutex
std::atomic<bool> g_ioFailed{false};  // the system loop has the card looked at

long long ElapsedMs(std::chrono::steady_clock::time_point since)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - since).count();
}

void SetPathsNoLock()
{
  g_path = FilePath();
  g_previousPath = PreviousFilePath();
  g_directory = os::storage::LogDirectory();
}

// Appends what the ring holds. False when the card refused it.
bool AppendNoLock()
{
  if (Device::Log::Pending() == 0) return true;
  const auto started = std::chrono::steady_clock::now();
  if (!g_directoryReady) g_directoryReady = os::storage::CreateDirectories(g_directory);
  FILE* file = std::fopen(g_path.c_str(), "ab");
  if (file == nullptr)
  {
    g_directoryReady = false;
    g_ioFailed = true;
    return false;
  }
  // The chunks are the buffer: no stream buffer on the heap.
  std::setvbuf(file, nullptr, _IONBF, 0);
  bool written = true;
  char chunk[kChunkBytes];
  // Bounded: what the other tasks print meanwhile waits for the next time.
  for (int round = 0; round < 64 && written; ++round)
  {
    std::size_t lost = 0;
    const std::size_t count = Device::Log::Take(chunk, sizeof(chunk), lost);
    if (lost != 0)
    {
      char note[64];
      const int length = std::snprintf(note, sizeof(note), "\n[log: %lu bytes lost here]\n", static_cast<unsigned long>(lost));
      written = std::fwrite(note, 1, static_cast<std::size_t>(length), file) == static_cast<std::size_t>(length);
    }
    if (count == 0) break;
    written = written && std::fwrite(chunk, 1, count, file) == count;
  }
  const long size = std::ftell(file);
  written = std::fclose(file) == 0 && written;
  if (!written)
  {
    g_ioFailed = true;
    return false;
  }
  if (size > kMaxFileBytes)
  {
    std::remove(g_previousPath.c_str());
    std::rename(g_path.c_str(), g_previousPath.c_str());
  }
  const long long took = ElapsedMs(started);
  if (took > kSlowWriteMs)
  {
    char line[48];
    std::snprintf(line, sizeof(line), "log: writing took %lld ms", took);
    Device::Log::Write(line);
  }
  return true;
}

void WriteInBackground()
{
  if (!g_writeAsked.exchange(false)) return;
  std::lock_guard<std::mutex> lock(g_writeMutex);
  (void)AppendNoLock();
}

// The system loop: a write the writer could not make has the card looked at (it may have gone).
void CheckFailedWrite()
{
  if (g_ioFailed.exchange(false)) os::storage::ReportCardIoError();
}

}  // namespace

std::string FilePath()
{
  return os::storage::JoinPath(os::storage::LogDirectory(), "log.txt");
}

std::string PreviousFilePath()
{
  return os::storage::JoinPath(os::storage::LogDirectory(), "log.1.txt");
}

bool Flush()
{
  CheckFailedWrite();
  // Off, the ring keeps the newest lines; turned on, they are written first.
  if (!os::storage::LogEnabled() || !os::storage::CardMounted()) return false;
  if (Device::Log::Pending() == 0) return true;
  bool written = false;
  {
    std::lock_guard<std::mutex> lock(g_writeMutex);
    SetPathsNoLock();
    written = AppendNoLock();
  }
  CheckFailedWrite();
  return written;
}

void Service(std::uint32_t nowMs)
{
  CheckFailedWrite();
  const std::uint32_t since = nowMs - g_lastWriteAt;
  if (since < kBusyWriteIntervalMs) return;
  if (since < kWriteIntervalMs && Device::Log::Pending() < kBusyPendingBytes) return;
  // While a sample plays from the card the log waits (the ring keeps the newest lines): writing makes
  // the card slow to read from.
  if (os::storage::CardStreaming()) return;
  g_lastWriteAt = nowMs;
  if (!os::storage::LogEnabled() || !os::storage::CardMounted() || Device::Log::Pending() == 0) return;
  if (!g_background)
  {
    (void)Flush();
    return;
  }
  // Busy writing (the card is slow): the writer takes the new text when it is done.
  std::unique_lock<std::mutex> lock(g_writeMutex, std::try_to_lock);
  if (!lock.owns_lock()) return;
  SetPathsNoLock();
  lock.unlock();
  g_writeAsked = true;
  if (!Device::Background::Run(&WriteInBackground))
  {
    g_background = false;
    g_writeAsked = false;
    (void)Flush();
  }
}

}  // namespace os::log

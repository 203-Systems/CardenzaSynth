#include "OS/System/Log.h"

#include <chrono>
#include <cstdio>

#include "Devices/Device.h"
#include "OS/Storage/DeviceSettings.h"
#include "OS/Storage/Storage.h"

namespace os::log {

namespace {

// Taken from the ring and written in pieces this size (on the stack of the system loop).
constexpr std::size_t kChunkBytes = 256;
// A write that takes longer than this says so in the log.
constexpr long long kSlowWriteMs = 40;

std::uint32_t g_lastWriteAt = 0;
bool g_directoryReady = false;

long long ElapsedMs(std::chrono::steady_clock::time_point since)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - since).count();
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
  // Off, the ring keeps the newest lines; turned on, they are written first.
  if (!os::storage::LogEnabled() || !Device::Storage::IsMounted()) return false;
  if (Device::Log::Pending() == 0) return true;
  const auto started = std::chrono::steady_clock::now();
  if (!g_directoryReady) g_directoryReady = os::storage::CreateDirectories(os::storage::LogDirectory());
  const std::string path = FilePath();
  FILE* file = std::fopen(path.c_str(), "ab");
  if (file == nullptr)
  {
    g_directoryReady = false;
    Device::Storage::CheckAfterIoError();
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
    Device::Storage::CheckAfterIoError();
    return false;
  }
  if (size > kMaxFileBytes)
  {
    const std::string previous = PreviousFilePath();
    std::remove(previous.c_str());
    std::rename(path.c_str(), previous.c_str());
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

void Service(std::uint32_t nowMs)
{
  const std::uint32_t since = nowMs - g_lastWriteAt;
  if (since < kBusyWriteIntervalMs) return;
  if (since < kWriteIntervalMs && Device::Log::Pending() < kBusyPendingBytes) return;
  g_lastWriteAt = nowMs;
  (void)Flush();
}

}  // namespace os::log

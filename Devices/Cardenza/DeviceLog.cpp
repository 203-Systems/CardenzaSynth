#include "Devices/Cardenza/DeviceLog.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <utility>

#include <sys/lock.h>

#include <esp_app_desc.h>
#include <esp_attr.h>  // RTC_NOINIT_ATTR
#include <esp_cpu.h>
#include <esp_cpu_utils.h>
#include <esp_debug_helpers.h>
#include <esp_private/panic_internal.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <xtensa_context.h>

#include "CardenzaBuildInfo.h"
#include "Devices/Device.h"

// The console log of the device (Device::Log in Devices/Device.h).
//
// - The ring lives in RTC slow memory: 8 KB that are not part of the heap and keep their contents
//   through a software or panic reset, and through the Launcher, which runs between a crash and the
//   next start of this app (its own RTC data is the boot ticket at the top of RTC fast memory). Power
//   off clears it. So the newest lines before a crash are there at the next start, and OS/System/Log
//   writes them to the card then.
// - stdout and stderr go through the ring on their way to the console: the write function of the two
//   streams every task shares is wrapped (the stream, its buffer and its target stay as they are), so
//   printf and the ESP-IDF log lines are in it with the time since boot in front. CardenzaHAL's USB
//   console reopens those streams on itself (freopen, same objects), which drops the wrapper: the
//   device layer calls Hook() again after it starts USB.
// - The panic handler is wrapped (-Wl,--wrap=esp_panic_handler, Devices/Cardenza/CMakeLists.txt): the
//   crash goes into the ring as "!!!" lines (reason or abort message, task, backtrace) before ESP-IDF
//   prints it and restarts. Decode the backtrace with the ELF of the same build:
//   xtensa-esp32s3-elf-addr2line -pfiaC -e CardenzaSynthFirmware.elf <addresses>
// - Nothing here allocates, and nothing is in IRAM: ESP-IDF turns the flash cache back on before its
//   panic handler runs, which is in flash itself.

extern "C" void __real_esp_panic_handler(panic_info_t* info);
extern "C" void __wrap_esp_panic_handler(panic_info_t* info);

namespace Device::Log {

namespace {

constexpr std::uint32_t kMagic = 0x31474F4CU;  // "LOG1"
#if defined(CARDENZA_STRESS_TEST)
constexpr std::uint32_t kRingBytes = 4608;  // the stress test keeps its heap report beside it (RTC memory is 8 KB)
#else
constexpr std::uint32_t kRingBytes = 6 * 1024;
#endif
// head and taken count every byte ever written and taken; they move back together by whole rings
// long before they could wrap.
constexpr std::uint32_t kRebaseAt = 0x40000000U;
// A heap that keeps failing would fill the ring with the same line.
constexpr std::uint32_t kAllocationNotes = 16;

struct Ring {
  char data[kRingBytes];
  // The bookkeeping goes last: whatever may ever use the start of RTC slow memory can spoil old text,
  // which is shown as it is, but not this.
  std::uint32_t head;
  std::uint32_t taken;
  std::uint32_t crashed;
  std::uint32_t check;
  std::uint32_t magic;
};

RTC_NOINIT_ATTR Ring g_ring;
portMUX_TYPE g_lock = portMUX_INITIALIZER_UNLOCKED;

// A console stream and the write function it had before the wrapper.
using StreamWrite = decltype(std::declval<FILE>()._write);
struct Stream {
  FILE* file = nullptr;
  StreamWrite write = nullptr;
  // The next byte starts a line. Used under the stream's own lock.
  bool lineStart = true;
  // The current line starts with '@': console traffic, not log (OS/System/System.cpp mirrors every key
  // change to the Learn guide as "@K..." / "@H..."). Kept out of the ring, so a few hundred presses do
  // not push out what came before a crash, and out of log.txt with it.
  bool skipping = false;
};
Stream g_out, g_err;
std::uint32_t g_allocationNotes = 0;
std::atomic<std::uint32_t> g_failedAllocations{0};  // every one since the start, noted or not
bool g_previousCrashed = false;
bool g_previousKept = false;

std::uint32_t Check()
{
  return g_ring.magic ^ (g_ring.head * 2654435761U) ^ (g_ring.taken * 2246822519U) ^ (g_ring.crashed * 3266489917U) ^ 0x5A17C3E1U;
}

bool Valid()
{
  return g_ring.magic == kMagic && g_ring.check == Check() && g_ring.head - g_ring.taken <= kRebaseAt;
}

void Rebase()
{
  if (g_ring.head < kRebaseAt) return;
  if (g_ring.head - g_ring.taken > kRingBytes) g_ring.taken = g_ring.head - kRingBytes;
  const std::uint32_t shift = g_ring.taken / kRingBytes * kRingBytes;
  g_ring.head -= shift;
  g_ring.taken -= shift;
}

void PutNoLock(const char* text, std::size_t length)
{
  for (std::size_t index = 0; index < length; ++index) g_ring.data[(g_ring.head + index) % kRingBytes] = text[index];
  g_ring.head += static_cast<std::uint32_t>(length);
  Rebase();
}

void PutText(const char* text)
{
  if (text == nullptr) return;
  std::size_t length = 0;
  while (text[length] != 0 && length < 200) ++length;
  PutNoLock(text, length);
}

void PutHex(std::uint32_t value)
{
  char digits[10] = {'0', 'x'};
  for (int index = 0; index < 8; ++index)
  {
    const std::uint32_t nibble = (value >> (28 - 4 * index)) & 0xF;
    digits[2 + index] = static_cast<char>(nibble < 10 ? '0' + nibble : 'a' + nibble - 10);
  }
  PutNoLock(digits, sizeof(digits));
}

void PutDecimal(std::uint32_t value)
{
  char digits[10];
  std::size_t count = 0;
  do
  {
    digits[sizeof(digits) - 1 - count++] = static_cast<char>('0' + value % 10);
    value /= 10;
  } while (value != 0 && count < sizeof(digits));
  PutNoLock(digits + sizeof(digits) - count, count);
}

// "  12.345 ": seconds since boot, right-aligned, in front of every console line.
std::size_t FormatStamp(char* out)
{
  const std::uint64_t ms = static_cast<std::uint64_t>(esp_timer_get_time()) / 1000;
  std::uint32_t seconds = static_cast<std::uint32_t>(ms / 1000);
  const std::uint32_t millis = static_cast<std::uint32_t>(ms % 1000);
  char text[16];
  std::size_t length = 0;
  text[length++] = ' ';
  text[length++] = static_cast<char>('0' + millis % 10);
  text[length++] = static_cast<char>('0' + millis / 10 % 10);
  text[length++] = static_cast<char>('0' + millis / 100);
  text[length++] = '.';
  do
  {
    text[length++] = static_cast<char>('0' + seconds % 10);
    seconds /= 10;
  } while (seconds != 0 && length < sizeof(text));
  while (length < 10) text[length++] = ' ';
  for (std::size_t index = 0; index < length; ++index) out[index] = text[length - 1 - index];
  return length;
}

void Keep(Stream& stream, const char* data, std::size_t size)
{
  char stamp[16];
  const std::size_t stampLength = FormatStamp(stamp);
  portENTER_CRITICAL(&g_lock);
  if (Valid())
  {
    std::size_t start = 0;
    while (start < size)
    {
      if (stream.lineStart) stream.skipping = data[start] == '@';
      const void* newline = std::memchr(data + start, '\n', size - start);
      const std::size_t end = newline != nullptr ? static_cast<std::size_t>(static_cast<const char*>(newline) - data) + 1 : size;
      if (!stream.skipping)
      {
        if (stream.lineStart) PutNoLock(stamp, stampLength);
        PutNoLock(data + start, end - start);
      }
      stream.lineStart = newline != nullptr;
      start = end;
    }
    g_ring.check = Check();
  }
  portEXIT_CRITICAL(&g_lock);
}

// The streams' write function from now on: the ring, then on as before (the cookie is the stream).
_READ_WRITE_RETURN_TYPE WrappedWrite(struct _reent* reent, void* cookie, const char* data, _READ_WRITE_BUFSIZE_TYPE length)
{
  Stream& stream = cookie == g_err.file ? g_err : g_out;
  if (length > 0) Keep(stream, data, static_cast<std::size_t>(length));
  return stream.write != nullptr ? stream.write(reent, cookie, data, length) : length;
}

// Under the stream's own lock (a console stream is never a string stream, newlib's only lockless kind).
void Wrap(Stream& stream, FILE* file)
{
  if (file == nullptr) return;
  __lock_acquire_recursive(file->_lock);
  if (file->_write != &WrappedWrite)
  {
    stream.file = file;
    stream.write = file->_write;
    stream.lineStart = true;
    file->_write = &WrappedWrite;
  }
  __lock_release_recursive(file->_lock);
}

void RecordCrash(const panic_info_t* info)
{
  // No lock: the other core is stopped, and may have been stopped holding it.
  PutText("\n!!! CRASH core ");
  PutDecimal(static_cast<std::uint32_t>(info->core));
  if (TaskHandle_t task = xTaskGetCurrentTaskHandleForCore(info->core))
  {
    PutText(" task ");
    PutText(pcTaskGetName(task));
  }
  PutText(": ");
  if (g_panic_abort)
  {
    PutText(g_panic_abort_details != nullptr ? g_panic_abort_details : "abort()");
  }
  else
  {
    PutText(info->reason != nullptr ? info->reason : "exception");
    if (info->description != nullptr)
    {
      PutText(" (");
      PutText(info->description);
      PutText(")");
    }
    PutText(" at ");
    PutHex(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(info->addr)));
  }
  PutText("\n!!! backtrace:");
  if (const auto* exception = static_cast<const XtExcFrame*>(info->frame))
  {
    esp_backtrace_frame_t frame{};
    frame.pc = exception->pc;
    frame.sp = exception->a1;
    frame.next_pc = exception->a0;
    frame.exc_frame = exception;
    PutText(" ");
    PutHex(esp_cpu_process_stack_pc(frame.pc));
    for (int depth = 0; depth < 32 && frame.next_pc != 0 && esp_backtrace_get_next_frame(&frame); ++depth)
    {
      PutText(" ");
      PutHex(esp_cpu_process_stack_pc(frame.pc));
    }
  }
  PutText("\n");
  g_ring.crashed = 1;
  g_ring.check = Check();
}

const char* ResetReasonName(esp_reset_reason_t reason)
{
  switch (reason)
  {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    case ESP_RST_USB: return "usb";
    case ESP_RST_JTAG: return "jtag";
    default: return "unknown";
  }
}

}  // namespace

#if defined(CARDENZA_STRESS_TEST)
namespace {
// The stress test (Applications/Synth/StressTest.cpp) reports a crash of the run before: its !!! lines.
char g_previousCrash[1024];
void KeepPreviousCrash()
{
  std::size_t length = 0;
  const std::uint32_t used = g_ring.head - g_ring.taken;
  const std::uint32_t count = used < kRingBytes ? used : kRingBytes;
  bool inCrash = false;
  for (std::uint32_t index = 0; index < count && length + 1 < sizeof(g_previousCrash); ++index)
  {
    const char c = g_ring.data[(g_ring.head - count + index) % kRingBytes];
    if (!inCrash && c == '!' && index + 2 < count && g_ring.data[(g_ring.head - count + index + 1) % kRingBytes] == '!') inCrash = true;
    if (inCrash) g_previousCrash[length++] = c;
    if (inCrash && c == '\n') inCrash = false;
  }
  g_previousCrash[length] = 0;
}
}  // namespace
#endif

void Install()
{
  g_previousKept = Valid();
  g_previousCrashed = g_previousKept && g_ring.crashed != 0;
#if defined(CARDENZA_STRESS_TEST)
  if (g_previousCrashed) KeepPreviousCrash();
#endif
  if (!g_previousKept)
  {
    g_ring.head = g_ring.taken = 0;
    g_ring.magic = kMagic;
  }
  g_ring.crashed = 0;
  g_ring.check = Check();

  Hook();

  const esp_app_desc_t* app = esp_app_get_description();
  char elf[17] = {};
  esp_app_get_elf_sha256(elf, sizeof(elf));
  // version.txt, the Git commit (+: built with local changes) and the ELF's hash, which decodes a backtrace.
  std::printf("\n==== %s %s %s%s (elf %s) started, reset: %s, previous run: %s\n", app->project_name, app->version,
              cardenza::build::kHash, cardenza::build::kDirty ? "+" : "", elf,
              ResetReasonName(esp_reset_reason()),
              g_previousCrashed ? "CRASHED (the !!! lines above)" : (g_previousKept ? "ended" : "unknown (power was off)"));
}

void Hook()
{
  // Every task shares the global streams (ESP-IDF gives each new task these two).
  Wrap(g_out, _REENT_STDOUT(_GLOBAL_REENT));
  Wrap(g_err, _REENT_STDERR(_GLOBAL_REENT));
}

void NoteFailedAllocation(std::size_t size, std::uint32_t caps, const char* function)
{
  g_failedAllocations.fetch_add(1, std::memory_order_relaxed);
  if (g_allocationNotes >= kAllocationNotes) return;
  ++g_allocationNotes;
  portENTER_CRITICAL_SAFE(&g_lock);
  if (Valid())
  {
    PutText("\n!!! allocation failed: ");
    PutDecimal(static_cast<std::uint32_t>(size));
    PutText(" bytes, caps ");
    PutHex(caps);
    PutText(", in ");
    PutText(function != nullptr ? function : "?");
    PutText("\n");
    g_ring.check = Check();
  }
  portEXIT_CRITICAL_SAFE(&g_lock);
}

std::size_t Take(char* destination, std::size_t capacity, std::size_t& lost)
{
  lost = 0;
  std::size_t count = 0;
  portENTER_CRITICAL(&g_lock);
  if (Valid())
  {
    std::uint32_t pending = g_ring.head - g_ring.taken;
    if (pending > kRingBytes)
    {
      lost = pending - kRingBytes;
      g_ring.taken = g_ring.head - kRingBytes;
      pending = kRingBytes;
    }
    count = std::min<std::size_t>(pending, capacity);
    for (std::size_t index = 0; index < count; ++index) destination[index] = g_ring.data[(g_ring.taken + index) % kRingBytes];
    g_ring.taken += static_cast<std::uint32_t>(count);
    g_ring.check = Check();
  }
  portEXIT_CRITICAL(&g_lock);
  // What spoiled old text left (see Ring) is not for a text file.
  for (std::size_t index = 0; index < count; ++index)
  {
    const unsigned char c = static_cast<unsigned char>(destination[index]);
    if ((c < 32 && c != '\n' && c != '\t') || c > 126) destination[index] = '?';
  }
  return count;
}

std::size_t Pending()
{
  std::size_t pending = 0;
  portENTER_CRITICAL(&g_lock);
  if (Valid()) pending = std::min<std::uint32_t>(g_ring.head - g_ring.taken, kRingBytes);
  portEXIT_CRITICAL(&g_lock);
  return pending;
}

void Write(std::string_view line)
{
  std::fwrite(line.data(), 1, line.size(), stdout);
  if (line.empty() || line.back() != '\n') std::fputc('\n', stdout);
}

}  // namespace Device::Log

namespace Device::Background {

namespace {
// Below the UI (5) on its core: it takes whatever time is left, and a card busy for seconds (one was,
// streaming a sample, 2026-11-04) keeps only this task waiting. The stack holds a project file being
// written (the log used under 2.5 KB).
constexpr UBaseType_t kPriority = 2;
constexpr std::uint32_t kStackBytes = 6144;
constexpr std::size_t kMaxJobs = 4;
std::array<std::atomic<void (*)()>, kMaxJobs> g_jobs{};
TaskHandle_t g_task = nullptr;

void Task(void*)
{
  for (;;)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (auto& job : g_jobs)
      if (void (*run)() = job.load(std::memory_order_acquire)) run();
  }
}
}  // namespace

bool Run(void (*job)())
{
  if (job == nullptr) return false;
  if (g_task == nullptr && xTaskCreatePinnedToCore(Task, "background", kStackBytes, nullptr, kPriority, &g_task, 0) != pdPASS)
  {
    g_task = nullptr;
    return false;
  }
  bool registered = false;
  for (auto& slot : g_jobs)
  {
    void (*current)() = slot.load(std::memory_order_relaxed);
    if (current == job)
    {
      registered = true;
      break;
    }
    if (current == nullptr)
    {
      slot.store(job, std::memory_order_release);
      registered = true;
      break;
    }
  }
  if (!registered) return false;
  xTaskNotifyGive(g_task);
  return true;
}

}  // namespace Device::Background

extern "C" void __wrap_esp_panic_handler(panic_info_t* info)
{
  if (info != nullptr && Device::Log::Valid()) Device::Log::RecordCrash(info);
  __real_esp_panic_handler(info);
}

#if defined(CARDENZA_STRESS_TEST)
extern "C" const char* cardenza_previous_crash() { return Device::Log::g_previousCrash; }
// Every allocation that failed since the start, noted or not (the stress test reports it).
extern "C" unsigned cardenza_failed_allocations() { return Device::Log::g_failedAllocations.load(std::memory_order_relaxed); }
#endif

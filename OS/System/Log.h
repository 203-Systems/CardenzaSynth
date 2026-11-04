#pragma once

#include <cstdint>
#include <string>

// The console log on the card while the LOG device setting (sys.log, SETUP > SYSTEM) is on:
// Cardenza/Logs/log.txt, and the one before it as log.1.txt (docs/app-storage-protocol.md). A user
// sends these files when something went wrong.
//
// The device keeps the newest lines in its log ring (Device::Log), with a crash and its backtrace,
// through a crash reset; this appends what is new to the file, so a crash is on the card after the next
// start. Made to cost nothing that shows: no heap buffer of its own, at most every few seconds, in one
// short append, only while the card is mounted; a failed write loses only that text. On the device the
// append runs on the device's background task below the UI (Device::Background): a card busy streaming a
// sample took 2-4 s per append, and in the system loop that froze the screen and the keys
// (2026-11-04).
namespace os::log {

// Every so often, sooner when the ring is half full. A power cut loses at most this much.
inline constexpr std::uint32_t kWriteIntervalMs = 5000;
inline constexpr std::uint32_t kBusyWriteIntervalMs = 1000;
inline constexpr std::size_t kBusyPendingBytes = 3 * 1024;
// log.txt becomes log.1.txt beyond this.
inline constexpr long kMaxFileBytes = 512 * 1024;

std::string FilePath();
std::string PreviousFilePath();
// From the system loop: writes when it is time.
void Service(std::uint32_t nowMs);
// Writes what is waiting now (boot, app switch, shutdown). False when the log is off, there is no card
// or the write failed.
bool Flush();

}  // namespace os::log

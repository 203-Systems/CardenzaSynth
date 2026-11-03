#pragma once

#include <cstddef>
#include <cstdint>

// The Cardenza side of Device::Log (Devices/Device.h): the ring in RTC slow memory, the console tee
// and the crash record. See DeviceLog.cpp.
namespace Device::Log {

// First thing in app_main, before any task is created: checks what the ring kept from the previous run
// and sends stdout and stderr through it from then on (Hook).
void Install();
// Wraps the write function of the shared stdout and stderr streams, if it is not wrapped already.
// Again after anything reopens them (CardenzaHAL's USB console does).
void Hook();
// From the heap's failed-allocation callback (any context): one line in the ring, not on stdout.
void NoteFailedAllocation(std::size_t size, std::uint32_t caps, const char* function);

}  // namespace Device::Log

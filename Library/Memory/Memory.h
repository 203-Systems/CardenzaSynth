#pragma once

// Where data lives (docs/groovebox-design.md 内存, AGENT.md Memory).
//
// Internal RAM: about 280 KB of heap beside the screen buffer and the code in IRAM. Fast, DMA-capable,
// and the only RAM usable while the flash is written: every task stack, the audio path (AMY's
// oscillators, render blocks, the effects) and the drivers' state live here. malloc only ever uses
// it; plain new uses it and spills into PSRAM when it is full instead of ending the program
// (Devices/Cardenza/Device.cpp).
//
// PSRAM: 2 MB on the chip (ESP32-S3R2), quad SPI on the same bus as the flash that AMY's code runs
// from, so a random access costs about ten times more and every byte read competes with the sound.
// What is touched per sample stays internal; what is read per step, per screen or on demand goes
// there with `new (memory::slow) T` or `memory::SlowAllocator`: pattern steps, project snapshots,
// engine parameter tables and presets, samples. Not what the SD card reads into or writes from: its
// DMA cannot reach PSRAM.
//
// Hosts (the simulators and tests) have one RAM: slow allocations are ordinary ones there.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace memory {

struct SlowTag {
  explicit constexpr SlowTag() = default;
};
// `new (memory::slow) T(...)`: PSRAM, internal RAM on a board without it; null when neither has room.
// Delete as usual.
inline constexpr SlowTag slow{};

// Without PSRAM, slow allocations keep this much internal RAM for the plain ones.
inline constexpr std::size_t kFastFloorBytes = 20 * 1024;

#if defined(ESP_PLATFORM)
inline constexpr std::uint32_t kFastCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
inline constexpr std::uint32_t kSlowCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

inline std::size_t FastFree() { return heap_caps_get_free_size(kFastCaps); }
inline std::size_t FastLargest() { return heap_caps_get_largest_free_block(kFastCaps); }
inline std::size_t FastMinimum() { return heap_caps_get_minimum_free_size(kFastCaps); }
inline std::size_t SlowTotal() { return heap_caps_get_total_size(kSlowCaps); }
inline std::size_t SlowFree() { return heap_caps_get_free_size(kSlowCaps); }
inline std::size_t SlowLargest() { return heap_caps_get_largest_free_block(kSlowCaps); }
inline std::size_t SlowMinimum() { return heap_caps_get_minimum_free_size(kSlowCaps); }
inline bool HasSlow() { return SlowTotal() != 0; }

inline void* AllocateSlow(std::size_t bytes) noexcept
{
  if (bytes == 0) bytes = 1;
  if (HasSlow()) return heap_caps_malloc(bytes, kSlowCaps);
  if (FastFree() < bytes + kFastFloorBytes) return nullptr;
  return heap_caps_malloc(bytes, kFastCaps);
}

inline void FreeSlow(void* block) noexcept { heap_caps_free(block); }
inline void* AllocateSlowArray(std::size_t bytes) noexcept { return AllocateSlow(bytes); }
inline void FreeSlowArray(void* block) noexcept { heap_caps_free(block); }
#else
inline std::size_t FastFree() { return std::numeric_limits<std::size_t>::max() / 4; }
inline std::size_t FastLargest() { return std::numeric_limits<std::size_t>::max() / 4; }
inline std::size_t FastMinimum() { return std::numeric_limits<std::size_t>::max() / 4; }
inline std::size_t SlowTotal() { return 0; }
inline std::size_t SlowFree() { return 0; }
inline std::size_t SlowLargest() { return 0; }
inline std::size_t SlowMinimum() { return 0; }
inline bool HasSlow() { return false; }

// Through operator new and new[], so that the usual delete and delete[] match them (sanitizers check
// the pairing).
inline void* AllocateSlow(std::size_t bytes) noexcept { return ::operator new(bytes != 0 ? bytes : 1, std::nothrow); }
inline void FreeSlow(void* block) noexcept { ::operator delete(block); }
inline void* AllocateSlowArray(std::size_t bytes) noexcept { return ::operator new[](bytes != 0 ? bytes : 1, std::nothrow); }
inline void FreeSlowArray(void* block) noexcept { ::operator delete[](block); }
#endif

// Room for something kept in PSRAM, leaving `reserve` there; on a board without PSRAM, room for it in
// internal RAM above `fastFloor`. The largest free block is asked for only for large blocks: finding
// it walks the whole heap.
inline bool SlowRoom(std::size_t bytes, std::size_t reserve, std::size_t fastFloor = kFastFloorBytes)
{
#if defined(ESP_PLATFORM)
  constexpr std::size_t kCheckBlockBytes = 16 * 1024;
  if (HasSlow()) return SlowFree() >= bytes + reserve && (bytes < kCheckBlockBytes || SlowLargest() >= bytes);
  return FastFree() >= bytes + fastFloor && (bytes < kCheckBlockBytes || FastLargest() >= bytes);
#else
  (void)bytes;
  (void)reserve;
  (void)fastFloor;
  return true;
#endif
}

// For standard containers whose blocks belong in PSRAM. Running out ends the program, as it does for
// any plain allocation: callers check SlowRoom first where it can happen.
template <class T>
struct SlowAllocator {
  using value_type = T;

  SlowAllocator() noexcept = default;
  template <class U>
  SlowAllocator(const SlowAllocator<U>&) noexcept
  {
  }

  T* allocate(std::size_t count)
  {
    void* block = AllocateSlow(count * sizeof(T));
    if (block == nullptr) std::abort();
    return static_cast<T*>(block);
  }
  void deallocate(T* block, std::size_t) noexcept { FreeSlow(block); }

  template <class U>
  bool operator==(const SlowAllocator<U>&) const noexcept
  {
    return true;
  }
  template <class U>
  bool operator!=(const SlowAllocator<U>&) const noexcept
  {
    return false;
  }
};

}  // namespace memory

// The non-throwing allocation functions behind `new (memory::slow)`; the matching deallocation is the
// usual operator delete (it frees PSRAM and internal blocks alike).
inline void* operator new(std::size_t bytes, memory::SlowTag) noexcept { return memory::AllocateSlow(bytes); }
inline void* operator new[](std::size_t bytes, memory::SlowTag) noexcept { return memory::AllocateSlowArray(bytes); }
inline void operator delete(void* block, memory::SlowTag) noexcept { memory::FreeSlow(block); }
inline void operator delete[](void* block, memory::SlowTag) noexcept { memory::FreeSlowArray(block); }

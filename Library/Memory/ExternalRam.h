#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace memory {

// Large, non-DMA data must not consume the audio subsystem's internal RAM when
// PSRAM is full. Keep atomics and audio working state out of these allocations.
// Desktop targets retain their normal new/delete ownership.
#if defined(ESP_PLATFORM)
template <typename T>
struct ExternalDelete {
  void operator()(T* value) const noexcept
  {
    if (value != nullptr) {
      value->~T();
      heap_caps_free(value);
    }
  }
};

template <typename T>
struct ExternalArrayDelete {
  void operator()(T* value) const noexcept { heap_caps_free(value); }
};

template <typename T>
using ExternalPtr = std::unique_ptr<T, ExternalDelete<T>>;
template <typename T>
using ExternalArray = std::unique_ptr<T[], ExternalArrayDelete<T>>;
#else
template <typename T>
using ExternalPtr = std::unique_ptr<T>;
template <typename T>
using ExternalArray = std::unique_ptr<T[]>;
#endif

template <typename T>
ExternalPtr<T> MakeExternal()
{
#if defined(ESP_PLATFORM)
  void* storage = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (storage == nullptr) return {};
  return ExternalPtr<T>(::new (storage) T());
#else
  return ExternalPtr<T>(new (std::nothrow) T());
#endif
}

template <typename T>
ExternalArray<T> MakeExternalArray(std::size_t count)
{
  static_assert(std::is_trivial_v<T>, "external arrays contain only trivial data");
  if (count == 0 || count > std::numeric_limits<std::size_t>::max() / sizeof(T)) return {};
#if defined(ESP_PLATFORM)
  return ExternalArray<T>(static_cast<T*>(
      heap_caps_calloc(count, sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
#else
  return ExternalArray<T>(new (std::nothrow) T[count]());
#endif
}

}  // namespace memory

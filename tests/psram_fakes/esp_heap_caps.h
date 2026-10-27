#pragma once

#include <cstddef>
#include <cstdint>

inline constexpr std::uint32_t MALLOC_CAP_SPIRAM = 1u << 10;
inline constexpr std::uint32_t MALLOC_CAP_8BIT = 1u << 2;

void* heap_caps_malloc(std::size_t bytes, std::uint32_t caps);
void* heap_caps_calloc(std::size_t count, std::size_t bytes, std::uint32_t caps);
void heap_caps_free(void* pointer);

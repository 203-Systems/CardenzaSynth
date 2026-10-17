# Cardenza HAL Module

This directory is intended to be a portable Cardenza hardware module.

## Design Goals

- Keep public headers and internal includes rooted in this directory.
- Allow `add_subdirectory(cardenza)` from another CMake project.
- Avoid depending on the repository root include path.
- Keep ESP-IDF-only sources out of non-embedded builds unless explicitly enabled.

## Public Include Root

Consumers should include headers from the module root exposed by the `CardenzaHAL` target.

Examples:

```cpp
#include "cardenzaHAL.h"
#include "audio/audio.h"
```

Avoid hardcoding repository paths like `Devices/Cardenza/cardenza/...` in consumers.

## CMake Usage

As a subdirectory:

```cmake
add_subdirectory(path/to/cardenza)
target_link_libraries(your_target PRIVATE CardenzaHAL)
```

As a standalone project:

```powershell
cmake -S path/to/cardenza -B build/cardenza
cmake --build build/cardenza --target CardenzaHAL
```

## Embedded Drivers

The module defaults to host-safe sources when configured outside ESP-IDF.

To enable ESP-IDF-only driver sources explicitly:

```cmake
set(CARDENZA_HAL_ENABLE_EMBEDDED_DRIVERS ON CACHE BOOL "" FORCE)
```

In the current tree, `storage/storage.cpp` is the source that is only added when embedded drivers are enabled.

## Portability Notes

- Public headers should stay self-contained relative to this directory.
- Optional ESP bring-up should stay behind embedded-only build paths.
- If another source becomes ESP-only, prefer excluding it from non-embedded source lists instead of adding host stub `.cpp` files unless a host symbol is actually required.
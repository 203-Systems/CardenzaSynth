#pragma once

#include <cstdint>
#include <filesystem>

namespace CardenzaSim::Headless {

// Writes an 8-bit RGB (colour type 2) PNG. `rgb` holds width * height * 3 bytes, row-major,
// top row first. The zlib stream uses stored (uncompressed) deflate blocks, so the encoder has
// no external dependencies.
bool WritePngRgb8(const std::filesystem::path& path, const std::uint8_t* rgb, std::uint32_t width, std::uint32_t height);

}  // namespace CardenzaSim::Headless

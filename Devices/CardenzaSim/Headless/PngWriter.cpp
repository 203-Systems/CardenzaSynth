#include "Devices/CardenzaSim/Headless/PngWriter.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <string_view>
#include <vector>

namespace CardenzaSim::Headless {
namespace {

constexpr std::size_t kMaxStoredBlockBytes = 65535;

const std::array<std::uint32_t, 256>& Crc32Table()
{
  static const std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> values{};
    for (std::uint32_t index = 0; index < values.size(); ++index)
    {
      std::uint32_t crc = index;
      for (int bit = 0; bit < 8; ++bit)
      {
        crc = (crc & 1u) != 0 ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
      }
      values[index] = crc;
    }
    return values;
  }();
  return table;
}

std::uint32_t Crc32(const std::vector<std::uint8_t>& bytes)
{
  const auto& table = Crc32Table();
  std::uint32_t crc = 0xFFFFFFFFu;
  for (std::uint8_t byte : bytes)
  {
    crc = table[(crc ^ byte) & 0xFFu] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFu;
}

std::uint32_t Adler32(const std::vector<std::uint8_t>& bytes)
{
  constexpr std::uint32_t kModulus = 65521;
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for (std::uint8_t byte : bytes)
  {
    a = (a + byte) % kModulus;
    b = (b + a) % kModulus;
  }
  return (b << 16) | a;
}

void AppendBe32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
  out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
}

void AppendLe16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
  out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

void AppendChunk(std::vector<std::uint8_t>& file, std::string_view type, const std::vector<std::uint8_t>& data)
{
  AppendBe32(file, static_cast<std::uint32_t>(data.size()));
  std::vector<std::uint8_t> crcInput(type.begin(), type.end());
  crcInput.insert(crcInput.end(), data.begin(), data.end());
  file.insert(file.end(), crcInput.begin(), crcInput.end());
  AppendBe32(file, Crc32(crcInput));
}

std::vector<std::uint8_t> ZlibStored(const std::vector<std::uint8_t>& raw)
{
  std::vector<std::uint8_t> out;
  out.reserve(raw.size() + (raw.size() / kMaxStoredBlockBytes + 1) * 5 + 6);
  out.push_back(0x78);  // CMF: deflate, 32 KiB window.
  out.push_back(0x01);  // FLG: no preset dictionary, check bits make CMF*256+FLG divisible by 31.

  std::size_t offset = 0;
  do
  {
    const std::size_t length = std::min(kMaxStoredBlockBytes, raw.size() - offset);
    const bool final = offset + length >= raw.size();
    out.push_back(final ? 0x01 : 0x00);  // BFINAL + BTYPE=00 (stored).
    AppendLe16(out, static_cast<std::uint16_t>(length));
    AppendLe16(out, static_cast<std::uint16_t>(~length & 0xFFFFu));
    out.insert(out.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
               raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
    offset += length;
  } while (offset < raw.size());

  AppendBe32(out, Adler32(raw));
  return out;
}

}  // namespace

bool WritePngRgb8(const std::filesystem::path& path, const std::uint8_t* rgb, std::uint32_t width, std::uint32_t height)
{
  if (rgb == nullptr || width == 0 || height == 0)
  {
    return false;
  }

  const std::size_t rowBytes = static_cast<std::size_t>(width) * 3u;
  std::vector<std::uint8_t> raw;
  raw.reserve((rowBytes + 1u) * height);
  for (std::uint32_t y = 0; y < height; ++y)
  {
    raw.push_back(0);  // Filter type 0 (None).
    const std::uint8_t* row = rgb + static_cast<std::size_t>(y) * rowBytes;
    raw.insert(raw.end(), row, row + rowBytes);
  }

  std::vector<std::uint8_t> header;
  AppendBe32(header, width);
  AppendBe32(header, height);
  header.push_back(8);  // Bit depth.
  header.push_back(2);  // Colour type: truecolour RGB.
  header.push_back(0);  // Compression: deflate.
  header.push_back(0);  // Filter method.
  header.push_back(0);  // No interlace.

  std::vector<std::uint8_t> file = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  AppendChunk(file, "IHDR", header);
  AppendChunk(file, "IDAT", ZlibStored(raw));
  AppendChunk(file, "IEND", {});

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open())
  {
    return false;
  }
  stream.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
  return stream.good();
}

}  // namespace CardenzaSim::Headless

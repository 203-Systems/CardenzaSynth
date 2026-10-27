#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Device::Display::detail {

struct DamageRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

// Small fingerprints avoid a second RGB565 framebuffer in scarce internal RAM.
// An occasional complete refresh also resynchronizes panel state and bounds the
// effect of an unlikely fingerprint collision. Only completed transfers commit.
template<int Width, int Height, int TileWidth = 16, int TileHeight = 8,
         std::size_t MaxRects = 8, unsigned FullRefreshInterval = 120>
class DamageTracker
{
  static_assert(Width > 0 && Height > 0 && TileWidth > 0 && TileHeight > 0);
  static_assert(MaxRects > 0 && FullRefreshInterval > 0);
  static constexpr int kColumns = (Width + TileWidth - 1) / TileWidth;
  static constexpr int kRows = (Height + TileHeight - 1) / TileHeight;
public:
  struct Plan {
    std::array<DamageRect, MaxRects> rectangles{};
    std::size_t count = 0;
    bool fullRefresh = false;
  };

  void Invalidate() { valid_ = false; }

  Plan Prepare(const std::uint16_t* frame)
  {
    if (!valid_ || ++framesSinceFull_ >= FullRefreshInterval) return FullFrame();
    Plan plan;
    int dirtyArea = 0;
    for (int tileY = 0; tileY < kRows; ++tileY)
    {
      int run = -1;
      for (int tileX = 0; tileX <= kColumns; ++tileX)
      {
        const bool dirty = tileX < kColumns &&
          Fingerprint(frame, tileX, tileY) != fingerprints_[tileY * kColumns + tileX];
        if (dirty)
        {
          const int tileW = std::min(TileWidth, Width - tileX * TileWidth);
          const int tileH = std::min(TileHeight, Height - tileY * TileHeight);
          dirtyArea += tileW * tileH;
          if (dirtyArea > Width * Height / 2) return FullFrame();
          if (run < 0) run = tileX;
        }
        else if (run >= 0)
        {
          const int x = run * TileWidth;
          DamageRect rect{x, tileY * TileHeight, std::min(Width, tileX * TileWidth) - x,
                          std::min(TileHeight, Height - tileY * TileHeight)};
          if (!Append(plan, rect)) return FullFrame();
          run = -1;
        }
      }
    }
    return plan;
  }

  void Commit(const std::uint16_t* frame, const Plan& plan)
  {
    for (std::size_t index = 0; index < plan.count; ++index)
    {
      const auto& rect = plan.rectangles[index];
      const int bottom = (rect.y + rect.height + TileHeight - 1) / TileHeight;
      const int right = (rect.x + rect.width + TileWidth - 1) / TileWidth;
      for (int y = rect.y / TileHeight; y < bottom; ++y)
        for (int x = rect.x / TileWidth; x < right; ++x)
          fingerprints_[y * kColumns + x] = Fingerprint(frame, x, y);
    }
    if (plan.fullRefresh)
    {
      valid_ = true;
      framesSinceFull_ = 0;
    }
  }

  // Pack a rectangle into a contiguous RGB565 DMA buffer. The caller chooses a
  // small band height so this never needs a second full-screen allocation.
  static void CopyBand(const std::uint16_t* frame, const DamageRect& band, std::uint16_t* destination)
  {
    if (band.width == Width)
    {
      std::memcpy(destination, frame + band.y * Width,
                  static_cast<std::size_t>(band.width) * band.height * sizeof(*frame));
      return;
    }
    for (int row = 0; row < band.height; ++row)
      std::memcpy(destination + row * band.width, frame + (band.y + row) * Width + band.x,
                  static_cast<std::size_t>(band.width) * sizeof(*frame));
  }

private:
  static Plan FullFrame()
  {
    Plan plan;
    plan.rectangles[0] = {0, 0, Width, Height};
    plan.count = 1;
    plan.fullRefresh = true;
    return plan;
  }

  static bool Append(Plan& plan, const DamageRect& rect)
  {
    for (std::size_t index = 0; index < plan.count; ++index)
    {
      auto& previous = plan.rectangles[index];
      if (previous.x == rect.x && previous.width == rect.width && previous.y + previous.height == rect.y)
      {
        previous.height += rect.height;
        return true;
      }
    }
    if (plan.count == plan.rectangles.size()) return false;
    plan.rectangles[plan.count++] = rect;
    return true;
  }

  static std::uint64_t Fingerprint(const std::uint16_t* frame, int tileX, int tileY)
  {
    std::uint64_t hash = 14695981039346656037ULL;
    const int left = tileX * TileWidth;
    const int right = std::min(left + TileWidth, Width);
    const int bottom = std::min((tileY + 1) * TileHeight, Height);
    for (int y = tileY * TileHeight; y < bottom; ++y)
    {
      for (int x = left; x < right; x += 2)
      {
        std::uint32_t pair = frame[y * Width + x];
        if (x + 1 < right) pair |= std::uint32_t(frame[y * Width + x + 1]) << 16;
        hash = (hash ^ pair) * 1099511628211ULL;
      }
    }
    return hash;
  }

  std::array<std::uint64_t, kColumns * kRows> fingerprints_{};
  unsigned framesSinceFull_ = 0;
  bool valid_ = false;
};

}  // namespace Device::Display::detail

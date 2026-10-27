#include "Devices/DisplayDamage.h"

#include <array>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}

template<int Width, int Height, class Tracker>
void Transfer(Tracker& tracker, const std::array<std::uint16_t, Width * Height>& frame,
              std::array<std::uint16_t, Width * Height>& panel, const typename Tracker::Plan& plan)
{
  std::array<std::uint16_t, Width * 8 + 2> staging;
  for (std::size_t i = 0; i < plan.count; ++i)
  {
    const auto& rect = plan.rectangles[i];
    Require(rect.x >= 0 && rect.y >= 0 && rect.width > 0 && rect.height > 0 &&
            rect.x + rect.width <= Width && rect.y + rect.height <= Height, "invalid damage bounds");
    for (int y = rect.y; y < rect.y + rect.height; y += 8)
    {
      const Device::Display::detail::DamageRect band{
        rect.x, y, rect.width, std::min(8, rect.y + rect.height - y)};
      staging.fill(0xa55a);
      Tracker::CopyBand(frame.data(), band, staging.data() + 1);
      const int count = band.width * band.height;
      Require(staging.front() == 0xa55a && staging[count + 1] == 0xa55a, "DMA packing overwrote bounds");
      for (int row = 0; row < band.height; ++row)
        for (int x = 0; x < band.width; ++x)
          panel[(band.y + row) * Width + band.x + x] = staging[1 + row * band.width + x];
    }
  }
  tracker.Commit(frame.data(), plan);
  Require(frame == panel, "partial transfers did not reconstruct the complete frame");
}

void TargetedChanges()
{
  using Tracker = Device::Display::detail::DamageTracker<240, 135>;
  Tracker tracker;
  std::array<std::uint16_t, 240 * 135> frame{}, panel{};
  auto plan = tracker.Prepare(frame.data());
  Require(plan.fullRefresh && plan.count == 1, "first present must refresh everything");
  Transfer<240, 135>(tracker, frame, panel, plan);
  Require(tracker.Prepare(frame.data()).count == 0, "unchanged frame sent pixels");

  frame.back() = 0xffff;
  plan = tracker.Prepare(frame.data());
  Require(!plan.fullRefresh && plan.count == 1, "one pixel should update one tile");
  const auto& edge = plan.rectangles[0];
  Require(edge.x == 224 && edge.y == 128 && edge.width == 16 && edge.height == 7,
          "bottom-right partial tile incorrect");
  // A failed or uncommitted transfer must not consume the damage.
  Require(tracker.Prepare(frame.data()).count == 1, "uncommitted change was lost");
  Transfer<240, 135>(tracker, frame, panel, plan);

  frame[0] = 1;
  frame[8 * 240] = 2;
  frame[16 * 240] = 3;
  plan = tracker.Prepare(frame.data());
  Require(plan.count == 1 && plan.rectangles[0].height == 24, "vertical tile runs did not merge");
  Transfer<240, 135>(tracker, frame, panel, plan);

  frame[239] = 10;
  frame[128 * 240] = 11;
  plan = tracker.Prepare(frame.data());
  Require(!plan.fullRefresh && plan.count == 2, "distant changes were not kept separate");
  Transfer<240, 135>(tracker, frame, panel, plan);

  // Nine separated runs, still much less than half the screen, exceed the plan limit.
  for (int y : {32, 48, 64})
    for (int x : {0, 32, 64}) frame[y * 240 + x] = 77;
  plan = tracker.Prepare(frame.data());
  Require(plan.fullRefresh, "fragmented update did not fall back to full refresh");
  Transfer<240, 135>(tracker, frame, panel, plan);
  frame.fill(0x1234);
  plan = tracker.Prepare(frame.data());
  Require(plan.fullRefresh, "large change did not fall back to full refresh");
  Transfer<240, 135>(tracker, frame, panel, plan);

  tracker.Invalidate();
  Require(tracker.Prepare(frame.data()).fullRefresh, "transfer failure did not force resynchronization");
}

void PeriodicRefresh()
{
  using Tracker = Device::Display::detail::DamageTracker<37, 19, 16, 8, 8, 4>;
  Tracker tracker;
  std::array<std::uint16_t, 37 * 19> frame{}, panel{};
  Transfer<37, 19>(tracker, frame, panel, tracker.Prepare(frame.data()));
  for (int i = 0; i < 3; ++i)
    Require(tracker.Prepare(frame.data()).count == 0, "periodic refresh fired early");
  const auto plan = tracker.Prepare(frame.data());
  Require(plan.fullRefresh, "periodic resynchronization missing");
  Transfer<37, 19>(tracker, frame, panel, plan);
  Require(tracker.Prepare(frame.data()).count == 0, "full refresh did not reset interval");
}

template<int Width, int Height>
void RandomFrames()
{
  using Tracker = Device::Display::detail::DamageTracker<Width, Height>;
  Tracker tracker;
  std::array<std::uint16_t, Width * Height> frame{}, panel{};
  std::mt19937 random(12345);
  for (int iteration = 0; iteration < 1500; ++iteration)
  {
    if (iteration % 53 == 0) frame.fill(static_cast<std::uint16_t>(random()));
    else
      for (unsigned i = 0, count = random() % 12; i < count; ++i)
        frame[random() % frame.size()] = static_cast<std::uint16_t>(random());
    auto plan = tracker.Prepare(frame.data());
    if (iteration % 47 == 0)
    {
      // Model a partial failed update; only a fresh full frame may commit after it.
      panel[0] ^= 0xffff;
      tracker.Invalidate();
      plan = tracker.Prepare(frame.data());
      Require(plan.fullRefresh, "partial failed update was not repaired");
    }
    Transfer<Width, Height>(tracker, frame, panel, plan);
  }
}
}  // namespace

int main()
{
  try
  {
    TargetedChanges();
    PeriodicRefresh();
    RandomFrames<240, 135>();
    RandomFrames<37, 19>();  // odd width and partial tiles on both axes
    std::cout << "display damage tests passed\n";
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}

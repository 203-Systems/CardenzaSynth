#include "Applications/Synth/UI/Dx7/Dx7UiRenderer.h"

#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace synth::ui_render {

namespace {

enum class Dx7EdgePort : std::uint8_t {
  Auto,
  Center,
  Top,
  Right,
  Bottom,
  Left,
  TopLeft,
  TopRight,
  BottomRight,
  BottomLeft,
};

enum class Dx7EdgeRoute : std::uint8_t {
  Auto,
  Direct,
  DoglegAbove,
  DoglegBelow,
  DoglegRight,
  DoglegLeft,
};

struct Dx7Edge {
  std::uint8_t from = 0;
  std::uint8_t to = 0;
  Dx7EdgePort fromPort = Dx7EdgePort::Auto;
  Dx7EdgePort toPort = Dx7EdgePort::Auto;
  Dx7EdgeRoute route = Dx7EdgeRoute::Auto;
  std::uint8_t lane = 0;
};

struct Dx7BoxPosition {
  std::uint8_t x = 50;
  std::uint8_t y = 50;
};

using Dx7BoxLayout = std::array<Dx7BoxPosition, 6>;

struct Dx7AlgoVisual {
  std::array<Dx7Edge, 8> edges{};
  std::size_t edgeCount = 0;
  std::uint8_t carriersMask = 0;
  Dx7BoxLayout boxLayout{};
};

constexpr std::uint8_t kDx7InvalidOpIndex = 0xFF;

constexpr Dx7BoxPosition MakeDx7Box(std::uint8_t x, std::uint8_t y)
{
  return Dx7BoxPosition{x, y};
}

constexpr Dx7BoxLayout MakeDx7BoxLayout(Dx7BoxPosition op1,
                                        Dx7BoxPosition op2,
                                        Dx7BoxPosition op3,
                                        Dx7BoxPosition op4,
                                        Dx7BoxPosition op5,
                                        Dx7BoxPosition op6)
{
  return Dx7BoxLayout{{op1, op2, op3, op4, op5, op6}};
}

constexpr std::uint8_t Dx7OperatorIndex(int op)
{
  return op >= 1 && op <= 6 ? static_cast<std::uint8_t>(op - 1) : kDx7InvalidOpIndex;
}

constexpr std::uint8_t Dx7CarrierBit(int op)
{
  const std::uint8_t index = Dx7OperatorIndex(op);
  return index < 6 ? static_cast<std::uint8_t>(1u << index) : 0;
}

constexpr Dx7Edge MakeDx7Edge(int from, int to)
{
  return Dx7Edge{Dx7OperatorIndex(from), Dx7OperatorIndex(to), Dx7EdgePort::Auto, Dx7EdgePort::Auto, Dx7EdgeRoute::Auto, 0};
}

constexpr Dx7Edge MakeDx7Edge(int from, int to, Dx7EdgeRoute route, std::uint8_t lane = 0)
{
  return Dx7Edge{Dx7OperatorIndex(from), Dx7OperatorIndex(to), Dx7EdgePort::Auto, Dx7EdgePort::Auto, route, lane};
}

constexpr Dx7Edge MakeDx7Edge(int from,
                              int to,
                              Dx7EdgePort fromPort,
                              Dx7EdgePort toPort,
                              Dx7EdgeRoute route = Dx7EdgeRoute::Auto,
                              std::uint8_t lane = 0)
{
  return Dx7Edge{Dx7OperatorIndex(from), Dx7OperatorIndex(to), fromPort, toPort, route, lane};
}

template <typename... Ops>
constexpr std::uint8_t Dx7CarrierMask(Ops... ops)
{
  const int carriers[] = {static_cast<int>(ops)...};
  std::uint8_t mask = 0;
  for (int op : carriers)
  {
    mask = static_cast<std::uint8_t>(mask | Dx7CarrierBit(op));
  }
  return mask;
}

#define DX7_CARRIERS(...) Dx7CarrierMask(__VA_ARGS__)

constexpr Dx7BoxLayout kDx7GridSlots = MakeDx7BoxLayout(
    MakeDx7Box(15, 25),
    MakeDx7Box(50, 25),
    MakeDx7Box(85, 25),
    MakeDx7Box(15, 75),
    MakeDx7Box(50, 75),
  MakeDx7Box(85, 75));

constexpr Dx7BoxLayout MakeDx7BoxOrder(std::uint8_t slot1,
                                       std::uint8_t slot2,
                                       std::uint8_t slot3,
                                       std::uint8_t slot4,
                                       std::uint8_t slot5,
                                       std::uint8_t slot6)
{
  const std::array<std::uint8_t, 6> order = {slot1, slot2, slot3, slot4, slot5, slot6};
  Dx7BoxLayout layout{};
  for (std::size_t slotIndex = 0; slotIndex < order.size(); ++slotIndex)
  {
    const std::uint8_t opIndex = Dx7OperatorIndex(order[slotIndex]);
    if (opIndex < 6)
    {
      layout[opIndex] = kDx7GridSlots[slotIndex];
    }
  }
  return layout;
}

constexpr Dx7BoxLayout kDefaultDx7BoxLayout = MakeDx7BoxOrder(1, 2, 3, 4, 5, 6);

template <std::size_t EdgeCount>
constexpr Dx7AlgoVisual MakeDx7Algo(std::array<Dx7Edge, EdgeCount> edges,
                                    std::uint8_t carriersMask,
                                    Dx7BoxLayout boxLayout)
{
  static_assert(EdgeCount <= 8, "DX7 algorithms support up to 8 edges");

  std::array<Dx7Edge, 8> storedEdges{};
  for (std::size_t edgeIndex = 0; edgeIndex < EdgeCount; ++edgeIndex)
  {
    storedEdges[edgeIndex] = edges[edgeIndex];
  }

  return Dx7AlgoVisual{storedEdges, EdgeCount, carriersMask, boxLayout};
}

template <std::size_t EdgeCount>
constexpr Dx7AlgoVisual MakeDx7Algo(std::array<Dx7Edge, EdgeCount> edges, std::uint8_t carriersMask)
{
  return MakeDx7Algo(edges, carriersMask, kDefaultDx7BoxLayout);
}

constexpr std::array<Dx7AlgoVisual, 32> kDx7Algorithms = {{
  // ALG 1
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(6, 6), MakeDx7Edge(3, 4), MakeDx7Edge(4, 5), MakeDx7Edge(5, 6)},
                DX7_CARRIERS(1, 3),
                MakeDx7BoxOrder(1, 2, 6, 3, 4, 5)),
  // ALG 2
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(2, 2), MakeDx7Edge(3, 4), MakeDx7Edge(5, 6), MakeDx7Edge(4, 5)},
                DX7_CARRIERS(1, 3),
                MakeDx7BoxOrder(1, 2, 6, 3, 4, 5)),
  // ALG 3
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(4, 5), MakeDx7Edge(6, 6), MakeDx7Edge(2, 3), MakeDx7Edge(5, 6)},
                DX7_CARRIERS(1, 4)),
  // ALG 4
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6, Dx7EdgeRoute::DoglegAbove), MakeDx7Edge(2, 3), MakeDx7Edge(5, 6)},
                DX7_CARRIERS(1, 4)),
  // ALG 5
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(3, 4), MakeDx7Edge(5, 6), MakeDx7Edge(6, 6)},
                DX7_CARRIERS(1, 3, 5),
                MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 6
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2),
                 MakeDx7Edge(3, 4),
                 MakeDx7Edge(5, 6),
                 MakeDx7Edge(6, 5, Dx7EdgePort::Right, Dx7EdgePort::Right, Dx7EdgeRoute::DoglegRight)},
                DX7_CARRIERS(1, 3, 5),
                MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 7
        MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(3, 4), MakeDx7Edge(5, 6), MakeDx7Edge(3, 5), MakeDx7Edge(6, 6)},
            DX7_CARRIERS(1, 3),
            MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 8
        MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(3, 4), MakeDx7Edge(5, 6), MakeDx7Edge(3, 5), MakeDx7Edge(4, 4)},
            DX7_CARRIERS(1, 3),
            MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 9
        MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(3, 4), MakeDx7Edge(5, 6), MakeDx7Edge(3, 5), MakeDx7Edge(2, 2)},
            DX7_CARRIERS(1, 3),
            MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 10
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(2, 3), MakeDx7Edge(3, 3), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6, Dx7EdgeRoute::DoglegAbove)},
            DX7_CARRIERS(1, 4)),
  // ALG 11
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(2, 3), MakeDx7Edge(6, 6), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6, Dx7EdgeRoute::DoglegAbove)},
            DX7_CARRIERS(1, 4)),
  // ALG 12
        MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(2, 2), MakeDx7Edge(3, 4), MakeDx7Edge(3, 5), MakeDx7Edge(3, 6)},
        DX7_CARRIERS(1, 3),
        MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 13
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(6, 6), MakeDx7Edge(3, 4), MakeDx7Edge(3, 5), MakeDx7Edge(3, 6)},
        DX7_CARRIERS(1, 3),
        MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 14
    MakeDx7Algo(std::array{MakeDx7Edge(6, 6), MakeDx7Edge(1, 2), MakeDx7Edge(3, 4), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6)},
                DX7_CARRIERS(1, 3),
                MakeDx7BoxOrder(1, 2, 5, 3, 4, 6)),
  // ALG 15
    MakeDx7Algo(std::array{MakeDx7Edge(2, 2), MakeDx7Edge(1, 2), MakeDx7Edge(3, 4), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6)},
                DX7_CARRIERS(1, 3),
                MakeDx7BoxOrder(1, 2, 5, 3, 4, 6)),
  // ALG 16
    MakeDx7Algo(std::array{MakeDx7Edge(6, 6), MakeDx7Edge(1, 2), MakeDx7Edge(1, 3), MakeDx7Edge(1, 5), MakeDx7Edge(3, 4), MakeDx7Edge(5, 6)},
                DX7_CARRIERS(1),
                MakeDx7BoxOrder(1, 3, 4, 2, 5, 6)),
  // ALG 17
    MakeDx7Algo(std::array{MakeDx7Edge(2, 2), MakeDx7Edge(1, 2), MakeDx7Edge(1, 3), MakeDx7Edge(1, 5), MakeDx7Edge(3, 4), MakeDx7Edge(5, 6)},
                DX7_CARRIERS(1),
                MakeDx7BoxOrder(1, 3, 4, 2, 5, 6)),
  // ALG 18
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(1, 3), MakeDx7Edge(1, 4), MakeDx7Edge(3, 3), MakeDx7Edge(5, 6), MakeDx7Edge(4, 5)},
                DX7_CARRIERS(1),
                MakeDx7BoxOrder(1, 3, 6, 2, 4, 5)),
  // ALG 19
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(2, 3), MakeDx7Edge(6, 6), MakeDx7Edge(4, 6), MakeDx7Edge(5, 6)},
          DX7_CARRIERS(1, 4, 5),
          MakeDx7BoxOrder(1, 4, 5, 2, 3, 6)),
  // ALG 20
    MakeDx7Algo(std::array{MakeDx7Edge(3, 3), MakeDx7Edge(2, 3), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6), MakeDx7Edge(1, 3)},
                DX7_CARRIERS(1, 2, 4),
                MakeDx7BoxOrder(1, 2, 4, 3, 5, 6)),
  // ALG 21
    MakeDx7Algo(std::array{MakeDx7Edge(3, 3), MakeDx7Edge(1, 3), MakeDx7Edge(2, 3), MakeDx7Edge(4, 6), MakeDx7Edge(5, 6)},
                DX7_CARRIERS(1, 2, 4, 5),
                MakeDx7BoxOrder(1, 3, 5, 2, 6, 4)),
  // ALG 22
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(3, 6), MakeDx7Edge(4, 6), MakeDx7Edge(5, 6), MakeDx7Edge(6, 6)},
                DX7_CARRIERS(1, 3, 4, 5),
                MakeDx7BoxOrder(1, 3, 4, 2, 6, 5)),
  // ALG 23
    MakeDx7Algo(std::array{MakeDx7Edge(6, 6), MakeDx7Edge(1, 3), MakeDx7Edge(2, 3), MakeDx7Edge(4, 6), MakeDx7Edge(5, 6)},
                DX7_CARRIERS(1, 2, 4, 5),
                MakeDx7BoxOrder(1, 3, 5, 2, 6, 4)),
  // ALG 24
    MakeDx7Algo(std::array{MakeDx7Edge(3, 6), MakeDx7Edge(4, 6), MakeDx7Edge(5, 6), MakeDx7Edge(6, 6)},
                DX7_CARRIERS(1, 2, 3, 4, 5),
                MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 25
    MakeDx7Algo(std::array{MakeDx7Edge(4, 6), MakeDx7Edge(5, 6), MakeDx7Edge(6, 6)},
                DX7_CARRIERS(1, 2, 3, 4, 5),
                MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 26
    MakeDx7Algo(std::array{MakeDx7Edge(6, 6), MakeDx7Edge(2, 3), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6)},
                DX7_CARRIERS(1, 2, 4),
                MakeDx7BoxOrder(1, 2, 4, 3, 5, 6)),
  // ALG 27
    MakeDx7Algo(std::array{MakeDx7Edge(3, 3), MakeDx7Edge(2, 3), MakeDx7Edge(4, 5), MakeDx7Edge(4, 6)},
                DX7_CARRIERS(1, 2, 4),
                MakeDx7BoxOrder(1, 2, 4, 3, 5, 6)),
  // ALG 28
    MakeDx7Algo(std::array{MakeDx7Edge(1, 2), MakeDx7Edge(3, 4), MakeDx7Edge(4, 5), MakeDx7Edge(5, 5)},
                DX7_CARRIERS(1, 3, 6),
                MakeDx7BoxOrder(1, 3, 6, 2, 4, 5)),
  // ALG 29
    MakeDx7Algo(std::array{MakeDx7Edge(3, 4), MakeDx7Edge(5, 6), MakeDx7Edge(6, 6)},
                DX7_CARRIERS(1, 2, 3, 5),
                MakeDx7BoxOrder(1, 3, 5, 2, 4, 6)),
  // ALG 30
    MakeDx7Algo(std::array{MakeDx7Edge(3, 4), MakeDx7Edge(4, 5), MakeDx7Edge(5, 5)},
                DX7_CARRIERS(1, 2, 3, 6),
                MakeDx7BoxOrder(1, 3, 6, 2, 4, 5)),
  // ALG 31
    MakeDx7Algo(std::array{MakeDx7Edge(5, 6), MakeDx7Edge(6, 6)},
                DX7_CARRIERS(1, 2, 3, 4, 5)),
  // ALG 32
    MakeDx7Algo(std::array{MakeDx7Edge(6, 6)},
                DX7_CARRIERS(1, 2, 3, 4, 5, 6)),
}};

const Dx7AlgoVisual& LookupDx7Algorithm(int algorithm)
{
  const std::size_t index = static_cast<std::size_t>(std::clamp(algorithm, 1, 32) - 1);
  return kDx7Algorithms[index];
}

int SelectedDx7Operator(const std::string& focusItemId)
{
  if (focusItemId == "par.dx7.group.op1") return 0;
  if (focusItemId == "par.dx7.group.op2") return 1;
  if (focusItemId == "par.dx7.group.op3") return 2;
  if (focusItemId == "par.dx7.group.op4") return 3;
  if (focusItemId == "par.dx7.group.op5") return 4;
  if (focusItemId == "par.dx7.group.op6") return 5;
  if (focusItemId == "snd.dx7.op1") return 0;
  if (focusItemId == "snd.dx7.op2") return 1;
  if (focusItemId == "snd.dx7.op3") return 2;
  if (focusItemId == "snd.dx7.op4") return 3;
  if (focusItemId == "snd.dx7.op5") return 4;
  if (focusItemId == "snd.dx7.op6") return 5;
  if (focusItemId.rfind("op1.", 0) == 0) return 0;
  if (focusItemId.rfind("op2.", 0) == 0) return 1;
  if (focusItemId.rfind("op3.", 0) == 0) return 2;
  if (focusItemId.rfind("op4.", 0) == 0) return 3;
  if (focusItemId.rfind("op5.", 0) == 0) return 4;
  if (focusItemId.rfind("op6.", 0) == 0) return 5;
  return -1;
}

std::array<ui::Rect, 6> MakeDx7OperatorRects(const ui::Rect& rect, const Dx7AlgoVisual& algorithm)
{
  const int columnWidth = std::max(1, rect.width / 3);
  const int rowHeight = std::max(1, rect.height / 2);
  const int boxWidth = std::min(24, std::max(18, columnWidth - 16));
  const int boxHeight = std::min(15, std::max(11, rowHeight - 10));

  std::array<ui::Rect, 6> operatorRects{};
  const int halfWidth = boxWidth / 2;
  const int halfHeight = boxHeight / 2;

  for (std::size_t op = 0; op < operatorRects.size(); ++op)
  {
    const Dx7BoxPosition position = algorithm.boxLayout[op];
    const int centerX = rect.x + std::clamp((rect.width * static_cast<int>(position.x) + 50) / 100,
                                            halfWidth,
                                            std::max(halfWidth, rect.width - halfWidth));
    const int centerY = rect.y + std::clamp(rect.height * static_cast<int>(position.y) / 100,
                                            halfHeight,
                                            std::max(halfHeight, rect.height - halfHeight));
    operatorRects[op] = MakeRect(centerX - halfWidth, centerY - halfHeight, boxWidth, boxHeight);
  }

  return operatorRects;
}

constexpr int kDx7EdgeRouteOffset = 10;
constexpr int kDx7EdgeLaneSpacing = 5;

void DrawDx7FeedbackLoop(ui::Canvas& canvas, const ui::Rect& rect, const Dx7Edge& edge, ui::Color color)
{
  const int centerX = rect.x + rect.width / 2;
  const int topY = rect.y - 4;
  const int bottomY = rect.y + rect.height / 2;
  const int laneOffset = static_cast<int>(edge.lane) * kDx7EdgeLaneSpacing;
  const bool rightSide = edge.fromPort == Dx7EdgePort::Right ||
                         edge.fromPort == Dx7EdgePort::TopRight ||
                         edge.fromPort == Dx7EdgePort::BottomRight;
  const int sideX = rightSide ? rect.x + rect.width + 4 + laneOffset : rect.x - 5 - laneOffset;

  canvas.DrawLine(sideX, bottomY, sideX, topY, color);
  canvas.DrawLine(sideX, topY, centerX, topY, color);
  canvas.DrawLine(centerX, topY, centerX, bottomY, color);
  canvas.DrawLine(sideX, bottomY, centerX, bottomY, color);
}

struct Dx7EdgeAnchor {
  int x = 0;
  int y = 0;
};

Dx7EdgeAnchor ResolveDx7EdgeAnchor(const ui::Rect& rect, Dx7EdgePort port)
{
  const int leftX = rect.x;
  const int centerX = rect.x + rect.width / 2;
  const int rightX = rect.x + rect.width - 1;
  const int topY = rect.y;
  const int centerY = rect.y + rect.height / 2;
  const int bottomY = rect.y + rect.height - 1;

  switch (port)
  {
    case Dx7EdgePort::Top:
      return Dx7EdgeAnchor{centerX, topY};
    case Dx7EdgePort::Right:
      return Dx7EdgeAnchor{rightX, centerY};
    case Dx7EdgePort::Bottom:
      return Dx7EdgeAnchor{centerX, bottomY};
    case Dx7EdgePort::Left:
      return Dx7EdgeAnchor{leftX, centerY};
    case Dx7EdgePort::TopLeft:
      return Dx7EdgeAnchor{leftX, topY};
    case Dx7EdgePort::TopRight:
      return Dx7EdgeAnchor{rightX, topY};
    case Dx7EdgePort::BottomRight:
      return Dx7EdgeAnchor{rightX, bottomY};
    case Dx7EdgePort::BottomLeft:
      return Dx7EdgeAnchor{leftX, bottomY};
    case Dx7EdgePort::Auto:
    case Dx7EdgePort::Center:
    default:
      return Dx7EdgeAnchor{centerX, centerY};
  }
}

Dx7EdgePort ResolveAutoDx7EdgePort(Dx7EdgeRoute route)
{
  switch (route)
  {
    case Dx7EdgeRoute::DoglegAbove:
      return Dx7EdgePort::Top;
    case Dx7EdgeRoute::DoglegBelow:
      return Dx7EdgePort::Bottom;
    case Dx7EdgeRoute::DoglegRight:
      return Dx7EdgePort::Right;
    case Dx7EdgeRoute::DoglegLeft:
      return Dx7EdgePort::Left;
    case Dx7EdgeRoute::Auto:
    case Dx7EdgeRoute::Direct:
    default:
      return Dx7EdgePort::Center;
  }
}

Dx7EdgeAnchor ResolveDx7EdgeAnchor(const ui::Rect& rect, Dx7EdgePort port, Dx7EdgeRoute route)
{
  return ResolveDx7EdgeAnchor(rect, port == Dx7EdgePort::Auto ? ResolveAutoDx7EdgePort(route) : port);
}

bool HasDx7IntermediateBoxOnSameRow(const std::array<ui::Rect, 6>& operatorRects,
                                    const ui::Rect& fromRect,
                                    const ui::Rect& toRect)
{
  const int fromCenterX = fromRect.x + fromRect.width / 2;
  const int fromCenterY = fromRect.y + fromRect.height / 2;
  const int toCenterX = toRect.x + toRect.width / 2;
  const int minX = std::min(fromCenterX, toCenterX);
  const int maxX = std::max(fromCenterX, toCenterX);

  for (const ui::Rect& rect : operatorRects)
  {
    if ((rect.x == fromRect.x && rect.y == fromRect.y && rect.width == fromRect.width && rect.height == fromRect.height) ||
        (rect.x == toRect.x && rect.y == toRect.y && rect.width == toRect.width && rect.height == toRect.height))
    {
      continue;
    }

    const int centerX = rect.x + rect.width / 2;
    const int centerY = rect.y + rect.height / 2;
    const bool sameRow = std::abs(centerY - fromCenterY) <= 1;
    const bool betweenEndpoints = centerX > minX && centerX < maxX;
    if (sameRow && betweenEndpoints)
    {
      return true;
    }
  }

  return false;
}

void DrawDx7Edge(ui::Canvas& canvas,
                 const ui::Rect& bounds,
                 const std::array<ui::Rect, 6>& operatorRects,
                 const Dx7Edge& edge,
                 const ui::Rect& fromRect,
                 const ui::Rect& toRect,
                 ui::Color color)
{
  const int fromCenterX = fromRect.x + fromRect.width / 2;
  const int fromCenterY = fromRect.y + fromRect.height / 2;
  const int toCenterX = toRect.x + toRect.width / 2;
  const int toCenterY = toRect.y + toRect.height / 2;

  const bool sameRow = std::abs(fromCenterY - toCenterY) <= 1;
  const bool skipsIntermediateBox = sameRow && HasDx7IntermediateBoxOnSameRow(operatorRects, fromRect, toRect);

  Dx7EdgeRoute route = edge.route;
  if (route == Dx7EdgeRoute::Auto)
  {
    if (!skipsIntermediateBox)
    {
      route = Dx7EdgeRoute::Direct;
    }
    else
    {
      const int boundsCenterY = bounds.y + bounds.height / 2;
      route = fromCenterY < boundsCenterY ? Dx7EdgeRoute::DoglegBelow : Dx7EdgeRoute::DoglegAbove;
    }
  }

  const Dx7EdgeAnchor fromAnchor = ResolveDx7EdgeAnchor(fromRect, edge.fromPort, route);
  const Dx7EdgeAnchor toAnchor = ResolveDx7EdgeAnchor(toRect, edge.toPort, route);
  if (route == Dx7EdgeRoute::Direct)
  {
    canvas.DrawLine(fromAnchor.x, fromAnchor.y, toAnchor.x, toAnchor.y, color);
    return;
  }

  const int laneOffset = static_cast<int>(edge.lane) * kDx7EdgeLaneSpacing;
  if (route == Dx7EdgeRoute::DoglegRight || route == Dx7EdgeRoute::DoglegLeft)
  {
    const int routeX = route == Dx7EdgeRoute::DoglegRight ?
                           std::max(fromRect.x + fromRect.width, toRect.x + toRect.width) + kDx7EdgeRouteOffset + laneOffset :
                           std::min(fromRect.x, toRect.x) - kDx7EdgeRouteOffset - laneOffset;
    canvas.DrawLine(fromAnchor.x, fromAnchor.y, routeX, fromAnchor.y, color);
    canvas.DrawLine(routeX, fromAnchor.y, routeX, toAnchor.y, color);
    canvas.DrawLine(routeX, toAnchor.y, toAnchor.x, toAnchor.y, color);
    return;
  }

  const int routeY = route == Dx7EdgeRoute::DoglegBelow ?
                         std::max(fromRect.y + fromRect.height, toRect.y + toRect.height) + kDx7EdgeRouteOffset + laneOffset :
                         std::min(fromRect.y, toRect.y) - kDx7EdgeRouteOffset - laneOffset;

  canvas.DrawLine(fromAnchor.x, fromAnchor.y, fromAnchor.x, routeY, color);
  canvas.DrawLine(fromAnchor.x, routeY, toAnchor.x, routeY, color);
  canvas.DrawLine(toAnchor.x, routeY, toAnchor.x, toAnchor.y, color);
}

void DrawDx7AlgorithmGraph(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0 || snapshot.dx7Algorithm <= 0)
  {
    return;
  }

  const ui::Color divider = FadeColor(kDim, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const auto& algorithm = LookupDx7Algorithm(snapshot.dx7Algorithm);
  const auto operatorRects = MakeDx7OperatorRects(rect, algorithm);
  const int selectedOperator = SelectedDx7Operator(snapshot.focusItemId);

  for (std::size_t edgeIndex = 0; edgeIndex < algorithm.edgeCount; ++edgeIndex)
  {
    const Dx7Edge edge = algorithm.edges[edgeIndex];
    if (edge.from >= operatorRects.size() || edge.to >= operatorRects.size())
    {
      continue;
    }

    const bool edgeHighlighted = selectedOperator >= 0 &&
                                 (edge.from == static_cast<std::size_t>(selectedOperator) ||
                                  edge.to == static_cast<std::size_t>(selectedOperator));
    const ui::Color edgeColor = edgeHighlighted ? accent : divider;
    if (edge.from == edge.to)
    {
      DrawDx7FeedbackLoop(canvas, operatorRects[edge.from], edge, edgeColor);
      continue;
    }

    const ui::Rect fromRect = operatorRects[edge.from];
    const ui::Rect toRect = operatorRects[edge.to];
    DrawDx7Edge(canvas, rect, operatorRects, edge, fromRect, toRect, edgeColor);
  }

  // Carriers sit on a lit plate; modulators are outlined, so the audible path reads at a glance.
  for (std::size_t op = 0; op < operatorRects.size(); ++op)
  {
    const bool isCarrier = (algorithm.carriersMask & (1u << op)) != 0;
    const bool isSelected = selectedOperator == static_cast<int>(op);
    const ui::Rect operatorRect = operatorRects[op];
    canvas.FillRect(operatorRect, FadeColor(isCarrier ? MixColor(kRule, kDim, 0.35f) : kBackground, alpha));
    canvas.StrokeRect(operatorRect, isSelected ? accent : (isCarrier ? FadeColor(MixColor(kRule, kDim, 0.35f), alpha) : divider));
    const std::string label = std::to_string(op + 1);
    DrawInk(canvas, operatorRect.x + (operatorRect.width - InkWidth(canvas, label, 1) + 1) / 2,
            operatorRect.y + (operatorRect.height - InkHeight(1)) / 2, label, isSelected ? accent : text, 1);
  }
}

std::string Dx7OperatorRole(const UiScreenSnapshot& snapshot)
{
  const int op = SelectedDx7Operator(snapshot.focusItemId);
  if (op < 0 || snapshot.dx7Algorithm <= 0)
  {
    return {};
  }
  const auto& algorithm = LookupDx7Algorithm(snapshot.dx7Algorithm);
  // Edges are stored as (target, source): the second operator modulates the first.
  std::string targets;
  bool feedback = false;
  for (std::size_t index = 0; index < algorithm.edgeCount; ++index)
  {
    const Dx7Edge edge = algorithm.edges[index];
    if (edge.to != static_cast<std::uint8_t>(op)) continue;
    if (edge.from == edge.to) feedback = true;
    else targets += (targets.empty() ? "OP" : " ") + std::to_string(edge.from + 1);
  }
  const bool carrier = (algorithm.carriersMask & (1u << op)) != 0;
  std::string role = carrier ? "CARRIER" : (targets.empty() ? "MODULATOR" : "MODULATES " + targets);
  return feedback ? role + " + FB" : role;
}

}  // namespace

bool IsDx7ParameterSnapshot(const UiScreenSnapshot& snapshot)
{
  return snapshot.mode == UiMode::ContextMenu && snapshot.modifier == UiModifier::Parameter &&
         snapshot.engineKind == SynthEngineKind::Dx7;
}

void DrawDx7ParameterContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  // An operator page names its role in the title bar: carrier, or which operator it modulates.
  const std::string role = Dx7OperatorRole(snapshot);
  const int top = DrawPageTitle(canvas, rect, snapshot.title, snapshot.meta.empty() ? role : snapshot.meta, kAccent,
                                snapshot.subtitle, snapshot.status, alpha);
  if (snapshot.contentStyle == UiContentStyle::Adjust)
  {
    DrawAdjustValue(canvas, MakeRect(rect.x, top, rect.width, rect.y + rect.height - top), snapshot.primaryText,
                    snapshot.secondaryText, snapshot.valueLevel, alpha);
    return;
  }
  if (snapshot.contentStyle == UiContentStyle::KeyValues)
  {
    DrawKeyValues(canvas, rect, snapshot.rightBarColumns, snapshot.rightBarItems, alpha);
    if (snapshot.focusItemId == "par.dx7.group.alg")
    {
      // ALG and FB sit on the first row; the routing they produce fills the rows below.
      const ui::Rect row1 = KeyValueEntryRect(rect, canvas.Height(), 1, 1, 0);
      DrawDx7AlgorithmGraph(canvas, MakeRect(rect.x + 4, row1.y + 2, rect.width - 8, rect.y + rect.height - row1.y - 8), snapshot, alpha);
      return;
    }
    return;
  }

  DrawDx7AlgorithmGraph(canvas, MakeRect(rect.x + 4, top + 6, rect.width - 8, rect.y + rect.height - top - 12), snapshot, alpha);
}

}  // namespace synth::ui_render

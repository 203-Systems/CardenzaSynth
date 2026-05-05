#include "Library/Synth/SynthUiRenderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace synth {

namespace {

constexpr ui::Color kBackground = ui::Rgb565(0, 0, 0);
constexpr ui::Color kDivider = ui::Rgb565(136, 136, 136);
constexpr ui::Color kAccent = ui::Rgb565(0, 216, 232);
constexpr ui::Color kText = ui::Rgb565(236, 241, 246);

constexpr int kBarWidthSingle = 46;
constexpr int kBarWidthDouble = 100;
constexpr int kBarOuterPadding = 3;
constexpr int kBarCellGap = 2;
constexpr int kHomeContentLeftPadding = 6;
constexpr int kHomeContentRightPadding = 4;
constexpr int kContextContentLeftPadding = 10;
constexpr int kContextContentRightPadding = 8;
constexpr int kLoopBarCount = 4;
constexpr int kLoopBarGap = 3;
constexpr int kMarqueePauseFrames = 24;
constexpr int kMarqueeStepFrames = 2;
constexpr int kMarqueeGapPixels = 12;

std::uint32_t gMarqueeFrame = 0;

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

std::string TruncateText(const ui::Canvas& canvas, std::string value, int maxWidth, int scale);

bool IsDx7AlgorithmSummaryText(std::string_view text)
{
  return text.rfind("ALG ", 0) == 0 && text.find(" FB ") != std::string_view::npos;
}

enum class BarAnchor {
  Left,
  Right,
};

struct ScreenLayout {
  ui::Rect leftBar;
  ui::Rect content;
  ui::Rect rightBar;
};

ui::Rect MakeRect(int x, int y, int width, int height)
{
  return ui::Rect{x, y, width, height};
}

ui::Rect InsetRect(const ui::Rect& rect, int insetX, int insetY)
{
  return MakeRect(rect.x + insetX,
                  rect.y + insetY,
                  std::max(0, rect.width - insetX * 2),
                  std::max(0, rect.height - insetY * 2));
}

float Clamp01(float value)
{
  return std::clamp(value, 0.0f, 1.0f);
}

ui::Color MixColor(ui::Color from, ui::Color to, float t)
{
  t = Clamp01(t);

  const int fromRed = ((from >> 11) & 0x1F) * 255 / 31;
  const int fromGreen = ((from >> 5) & 0x3F) * 255 / 63;
  const int fromBlue = (from & 0x1F) * 255 / 31;

  const int toRed = ((to >> 11) & 0x1F) * 255 / 31;
  const int toGreen = ((to >> 5) & 0x3F) * 255 / 63;
  const int toBlue = (to & 0x1F) * 255 / 31;

  const auto lerp = [t](int a, int b) {
    return static_cast<int>(static_cast<float>(a) + static_cast<float>(b - a) * t + 0.5f);
  };

  return ui::Rgb565(static_cast<std::uint8_t>(lerp(fromRed, toRed)),
                    static_cast<std::uint8_t>(lerp(fromGreen, toGreen)),
                    static_cast<std::uint8_t>(lerp(fromBlue, toBlue)));
}

ui::Color FadeColor(ui::Color color, float alpha)
{
  return MixColor(kBackground, color, alpha);
}

void DrawMarqueeTextInRect(ui::Canvas& canvas,
                           const ui::Rect& rect,
                           std::string_view text,
                           ui::Color color,
                           int scale = 1,
                           ui::TextAlign fitAlign = ui::TextAlign::Left)
{
  if (rect.width <= 0 || rect.height <= 0 || text.empty())
  {
    return;
  }

  const int textWidth = canvas.MeasureTextWidth(text, scale);
  if (textWidth <= rect.width)
  {
    canvas.DrawTextInRect(rect, text, color, scale, fitAlign, ui::VerticalAlign::Middle);
    return;
  }

  const int gapWidth = std::max(kMarqueeGapPixels, canvas.MeasureTextWidth("   ", scale));
  const std::uint32_t steppedFrame = gMarqueeFrame / kMarqueeStepFrames;
  const int cycleWidth = textWidth + gapWidth;
  const int phase = static_cast<int>(steppedFrame % static_cast<std::uint32_t>(kMarqueePauseFrames + cycleWidth));
  const int offset = phase < kMarqueePauseFrames ? 0 : phase - kMarqueePauseFrames;
  const int textY = rect.y + (rect.height - canvas.FontHeight(scale)) / 2;

  canvas.DrawTextClipped(rect, rect.x - offset, textY, text, color, scale);
  canvas.DrawTextClipped(rect, rect.x - offset + cycleWidth, textY, text, color, scale);
}

ui::Rect LerpRect(const ui::Rect& from, const ui::Rect& to, float t)
{
  t = Clamp01(t);
  const auto lerp = [t](int a, int b) {
    return static_cast<int>(static_cast<float>(a) + static_cast<float>(b - a) * t + 0.5f);
  };

  return MakeRect(lerp(from.x, to.x), lerp(from.y, to.y), lerp(from.width, to.width), lerp(from.height, to.height));
}

int BarWidthForColumns(std::size_t columns, float widthScale)
{
  const int baseWidth = columns == 1 ? kBarWidthSingle : kBarWidthDouble;
  const float normalizedScale = std::max(1.0f, widthScale);
  return std::max(baseWidth, static_cast<int>(static_cast<float>(baseWidth) * normalizedScale + 0.5f));
}

bool IsDx7ParameterSnapshot(const UiScreenSnapshot& snapshot)
{
  return snapshot.mode == UiMode::ContextMenu && snapshot.modifier == UiModifier::Parameter &&
         snapshot.engineKind == SynthEngineKind::Dx7;
}

bool IsJunoParameterSnapshot(const UiScreenSnapshot& snapshot)
{
  return snapshot.mode == UiMode::ContextMenu && snapshot.modifier == UiModifier::Parameter &&
         snapshot.engineKind == SynthEngineKind::Juno;
}

float JunoNormalizedContinuous(int value)
{
  return Clamp01(static_cast<float>(std::clamp(value, 0, 127)) / 127.0f);
}

float JunoNormalizedChoice(int value, int maxValue)
{
  return maxValue <= 0 ? 0.0f : Clamp01(static_cast<float>(std::max(0, value)) / static_cast<float>(maxValue));
}

bool IsJunoFocusedParameter(const UiScreenSnapshot& snapshot, const char* parameterId)
{
  return snapshot.focusItemId == parameterId;
}

bool IsJunoDiscreteValuePage(const UiScreenSnapshot& snapshot)
{
  return snapshot.focusItemId == "dco.range" || snapshot.focusItemId == "dco.saw";
}

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

std::string Dx7OperatorTitle(const UiScreenSnapshot& snapshot)
{
  const int selectedOperator = SelectedDx7Operator(snapshot.focusItemId);
  return selectedOperator >= 0 ? "Operator " + std::to_string(selectedOperator + 1) : std::string{};
}

bool ShouldShowDx7OperatorPane(const UiScreenSnapshot& snapshot)
{
  return SelectedDx7Operator(snapshot.focusItemId) >= 0;
}

std::array<ui::Rect, 6> MakeDx7OperatorRects(const ui::Rect& rect, const Dx7AlgoVisual& algorithm)
{
  const int columnWidth = std::max(1, rect.width / 3);
  const int rowHeight = std::max(1, rect.height / 2);
  const int boxWidth = std::min(32, std::max(22, columnWidth - 10));
  const int boxHeight = std::min(22, std::max(14, rowHeight - 6));

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

  const ui::Color divider = FadeColor(kDivider, alpha);
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

  for (std::size_t op = 0; op < operatorRects.size(); ++op)
  {
    const bool isCarrier = (algorithm.carriersMask & (1u << op)) != 0;
    const bool isSelected = selectedOperator == static_cast<int>(op);
    const ui::Rect operatorRect = operatorRects[op];
    canvas.FillRect(InsetRect(operatorRect, 1, 1), kBackground);
    if (isCarrier)
    {
      canvas.FillRect(InsetRect(operatorRect, 1, 1), FadeColor(kAccent, alpha * 0.12f));
    }
    canvas.StrokeRect(operatorRect, isSelected || isCarrier ? accent : text);
    canvas.DrawTextInRect(operatorRect,
                          std::to_string(op + 1),
                          isSelected ? accent : text,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
  }
}

void DrawDx7OperatorPane(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const std::string title = Dx7OperatorTitle(snapshot);
  if (title.empty())
  {
    return;
  }
  const bool showingParameterValue = snapshot.focusItemId.rfind("op", 0) == 0;
  const std::string parameterText = showingParameterValue ? snapshot.primaryText : "Parameters";
  const int valueScale = showingParameterValue ? 5 : 2;

  canvas.DrawText(rect.x,
                  rect.y + 10,
                  TruncateText(canvas, title, rect.width, 1),
                  accent,
                  1);
  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 30, rect.width, 24),
                        TruncateText(canvas, parameterText, rect.width, 2),
                        text,
                        2,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
  canvas.DrawLine(rect.x, rect.y + 58, rect.x + rect.width - 1, rect.y + 58, divider);
  canvas.DrawTextInRect(MakeRect(rect.x,
                                 rect.y + 64,
                                 rect.width,
                                 rect.height - 68),
                        TruncateText(canvas, snapshot.secondaryText, rect.width, valueScale),
                        text,
                        valueScale,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

void DrawDx7ParameterContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const bool showingParameterValue = snapshot.focusItemId.rfind("op", 0) == 0 ||
                                     snapshot.focusItemId.rfind("global.", 0) == 0;
  if (ShouldShowDx7OperatorPane(snapshot))
  {
    DrawDx7OperatorPane(canvas, rect, snapshot, alpha);
    return;
  }

  const int valueScale = showingParameterValue ? 4 : 2;
  const bool showAlgorithmSummary = !showingParameterValue && IsDx7AlgorithmSummaryText(snapshot.secondaryText);
  constexpr int kDx7AlgorithmSummaryHeight = 28;
  constexpr int kDx7SummaryHeight = 46;
  constexpr int kDx7EmptySummaryHeight = 34;
  const int summaryHeight = showAlgorithmSummary ? kDx7AlgorithmSummaryHeight
                                                 : (snapshot.showSecondaryText ? kDx7SummaryHeight : kDx7EmptySummaryHeight);
  const int graphTop = rect.y + 2;
  const int summaryTop = rect.y + rect.height - summaryHeight;
  const ui::Rect graphRect = MakeRect(rect.x,
                                      graphTop,
                                      rect.width,
                                      std::max(0, summaryTop - graphTop - 2));
  DrawDx7AlgorithmGraph(canvas, graphRect, snapshot, alpha);

  canvas.DrawLine(rect.x, summaryTop - 2, rect.x + rect.width - 1, summaryTop - 2, divider);

  if (!snapshot.showSecondaryText)
  {
    return;
  }

  if (showAlgorithmSummary)
  {
    const std::string algorithmText = "ALG " + std::to_string(snapshot.dx7Algorithm);
    const std::string feedbackText = "FB " + std::to_string(snapshot.dx7Feedback);
    const int columnGap = 4;
    const int columnWidth = std::max(0, (rect.width - columnGap) / 2);
    const ui::Rect leftRect = MakeRect(rect.x,
                                       summaryTop + 2,
                                       columnWidth,
                                       summaryHeight - 2);
    const ui::Rect rightRect = MakeRect(rect.x + rect.width - columnWidth,
                                        summaryTop + 2,
                                        columnWidth,
                                        summaryHeight - 2);
    canvas.DrawTextInRect(leftRect,
                          algorithmText,
                          text,
                          2,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
    canvas.DrawTextInRect(rightRect,
                          feedbackText,
                          text,
                          2,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
    return;
  }

  canvas.DrawTextInRect(MakeRect(rect.x, summaryTop + 4, rect.width, summaryHeight - 4),
                        TruncateText(canvas, snapshot.secondaryText, rect.width, valueScale),
                        text,
                        valueScale,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

std::string JunoRangeText(int value)
{
  static const std::array<const char*, 3> kRanges = {"16'", "8'", "4'"};
  return kRanges[static_cast<std::size_t>(std::clamp(value, 0, 2))];
}

std::string JunoChorusText(int value)
{
  static const std::array<const char*, 3> kModes = {"OFF", "I", "II"};
  return kModes[static_cast<std::size_t>(std::clamp(value, 0, 2))];
}

std::string JunoToggleText(int value)
{
  return value > 0 ? "ON" : "OFF";
}

void DrawJunoEnvGraphic(ui::Canvas& canvas,
                        const ui::Rect& rect,
                        const UiScreenSnapshot& snapshot,
                        float alpha,
                        bool showValues);

int JunoOverviewRowHeight(const ui::Canvas& canvas)
{
  return canvas.FontHeight(1) + 2;
}

ui::Rect JunoOverviewEnvRect(const ui::Canvas& canvas, const ui::Rect& rect)
{
  const int gap = 2;
  const int rowHeight = JunoOverviewRowHeight(canvas);
  const int lowerHeight = rowHeight * 3 + gap * 2;
  const int topHeight = std::max(24, rect.height - lowerHeight - 4);
  return MakeRect(rect.x + 2, rect.y, rect.width - 4, topHeight);
}

void DrawJunoEnvValueGrid(ui::Canvas& canvas,
                          const ui::Rect& rect,
                          const UiScreenSnapshot& snapshot,
                          float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const std::array<std::pair<const char*, int>, 4> values = {{
      {"ATK", snapshot.junoEnvAttack},
      {"DEC", snapshot.junoEnvDecay},
      {"SUS", snapshot.junoEnvSustain},
      {"REL", snapshot.junoEnvRelease},
  }};
  const std::array<const char*, 4> ids = {"env.attack", "env.decay", "env.sustain", "env.release"};
  const int gap = 2;
  const int cellWidth = std::max(18, (rect.width - gap) / 2);
  const int cellHeight = std::max(12, (rect.height - gap) / 2);

  for (std::size_t index = 0; index < values.size(); ++index)
  {
    const int column = static_cast<int>(index % 2);
    const int row = static_cast<int>(index / 2);
    const ui::Rect cell = MakeRect(rect.x + column * (cellWidth + gap),
                                   rect.y + row * (cellHeight + gap),
                                   cellWidth,
                                   cellHeight);
    const bool focused = IsJunoFocusedParameter(snapshot, ids[index]);
    const ui::Color valueColor = focused ? accent : text;
    const ui::Rect textRect = MakeRect(cell.x + 2, cell.y + 1, std::max(0, cell.width - 4), std::max(0, cell.height - 2));
    const std::string valueText = std::string(values[index].first) + " " + std::to_string(values[index].second);
    const int textY = textRect.y + (textRect.height - canvas.FontHeight(1)) / 2;

    canvas.DrawTextClipped(textRect, textRect.x, textY, valueText, valueColor, 1);
  }
}

void DrawJunoEnvPage(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int gap = 4;
  const ui::Rect envRect = JunoOverviewEnvRect(canvas, rect);
  DrawJunoEnvGraphic(canvas, envRect, snapshot, alpha, false);

  const int gridTop = envRect.y + envRect.height + gap;
  const ui::Rect gridRect = MakeRect(rect.x,
                                     gridTop,
                                     rect.width,
                                     std::max(0, rect.y + rect.height - gridTop));
  DrawJunoEnvValueGrid(canvas, gridRect, snapshot, alpha);
}

void DrawJunoOverviewStrip(ui::Canvas& canvas,
                           const ui::Rect& rect,
                           const std::string& label,
                           const std::string& value,
                           ui::Color accent,
                           ui::Color divider,
                           ui::Color text,
                           ui::Color fill)
{
  (void)fill;
  (void)divider;
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int labelScale = 1;
  const int labelGap = 14;
  const int labelWidth = canvas.MeasureTextWidth(label, labelScale);
  const int valueX = rect.x + labelWidth + labelGap;
  const int valueWidth = std::max(0, rect.width - (valueX - rect.x));

  canvas.DrawText(rect.x, rect.y, TruncateText(canvas, label, 24, 1), accent, 1);
  canvas.DrawTextInRect(MakeRect(valueX, rect.y - 1, valueWidth, rect.height + 2),
                        TruncateText(canvas, value, std::max(0, valueWidth - 4), 1),
                        text,
                        1,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

void DrawJunoOverview(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.12f);
  const int gap = 2;
  const int rowHeight = JunoOverviewRowHeight(canvas);
  const ui::Rect envRect = JunoOverviewEnvRect(canvas, rect);
  DrawJunoEnvGraphic(canvas, envRect, snapshot, alpha, false);

  const int lowerTop = envRect.y + envRect.height + gap;

  std::string osc = JunoRangeText(snapshot.junoDcoRange);
  osc += snapshot.junoDcoSaw > 0 ? " SAW" : " PWM";

  std::string filter = "C";
  filter += std::to_string(snapshot.junoVcfCutoff);
  filter += " R";
  filter += std::to_string(snapshot.junoVcfResonance);

  std::string mod = "E";
  mod += std::to_string(snapshot.junoVcfEnv);
  mod += " ";
  mod += snapshot.junoChorusMode == 0 ? "-" : JunoChorusText(snapshot.junoChorusMode);

  DrawJunoOverviewStrip(canvas,
                        MakeRect(rect.x, lowerTop, rect.width, rowHeight),
                        "OSC",
                        osc,
                        accent,
                        divider,
                        text,
                        fill);
  DrawJunoOverviewStrip(canvas,
                        MakeRect(rect.x, lowerTop + rowHeight + gap, rect.width, rowHeight),
                        "FIL",
                        filter,
                        accent,
                        divider,
                        text,
                        fill);
  DrawJunoOverviewStrip(canvas,
                        MakeRect(rect.x, lowerTop + (rowHeight + gap) * 2, rect.width, rowHeight),
                        "MOD",
                        mod,
                        accent,
                        divider,
                        text,
                        fill);
}

void DrawJunoDcoGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const ui::Rect inner = InsetRect(rect, 2, 2);
  if (inner.width <= 0 || inner.height <= 0)
  {
    return;
  }

  const int panelGap = 6;
  const int rangeWidth = std::clamp(inner.width / 4, 34, 46);
  const int detailX = inner.x + rangeWidth + panelGap;
  const int detailWidth = std::max(0, inner.x + inner.width - detailX);
  const int pwmHeight = std::clamp(inner.height / 2, 22, 30);
  const ui::Rect rangeRect = MakeRect(inner.x, inner.y, rangeWidth, inner.height);
  const ui::Rect pwmRect = MakeRect(detailX, inner.y, detailWidth, pwmHeight);
  const ui::Rect statusRect = MakeRect(detailX,
                                       pwmRect.y + pwmRect.height + panelGap,
                                       detailWidth,
                                       std::max(0, inner.y + inner.height - (pwmRect.y + pwmRect.height + panelGap)));

  const auto drawValueCard = [&](const ui::Rect& card,
                                 const char* label,
                                 const std::string& value,
                                 float normalizedValue,
                                 bool focused) {
    if (card.width <= 0 || card.height <= 0)
    {
      return;
    }

    const ui::Color cardColor = focused ? accent : text;
    const ui::Color borderColor = focused ? accent : divider;
    if (focused)
    {
      canvas.FillRect(InsetRect(card, 1, 1), fill);
    }
    canvas.StrokeRect(card, borderColor);
    canvas.DrawTextInRect(MakeRect(card.x, card.y + 2, card.width, 8),
                          label,
                          cardColor,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
    canvas.DrawTextInRect(MakeRect(card.x + 1, card.y + 10, std::max(0, card.width - 2), std::max(0, card.height - 18)),
                          value,
                          cardColor,
                          value.size() > 2 ? 1 : 2,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);

    const ui::Rect meterRect = MakeRect(card.x + 3, card.y + card.height - 6, std::max(0, card.width - 6), 3);
    if (meterRect.width > 0)
    {
      canvas.FillRect(meterRect, divider);
      const int fillWidth = std::clamp(static_cast<int>(normalizedValue * static_cast<float>(meterRect.width) + 0.5f),
                                       0,
                                       meterRect.width);
      if (fillWidth > 0)
      {
        canvas.FillRect(MakeRect(meterRect.x, meterRect.y, fillWidth, meterRect.height), focused ? accent : text);
      }
    }
  };

  const bool rangeFocused = IsJunoFocusedParameter(snapshot, "dco.range");
  canvas.StrokeRect(rangeRect, rangeFocused ? accent : divider);
  canvas.DrawTextInRect(MakeRect(rangeRect.x, rangeRect.y + 2, rangeRect.width, 8),
                        "RNG",
                        rangeFocused ? accent : text,
                        1,
                        ui::TextAlign::Center,
                        ui::VerticalAlign::Middle);

  const std::array<const char*, 3> rangeLabels = {"16'", "8'", "4'"};
  const int rangeChipGap = 4;
  const int rangeChipTop = rangeRect.y + 12;
  const int rangeChipHeight = std::max(10, (rangeRect.height - 16 - rangeChipGap * 2) / 3);
  for (std::size_t index = 0; index < rangeLabels.size(); ++index)
  {
    const ui::Rect chip = MakeRect(rangeRect.x + 3,
                                   rangeChipTop + static_cast<int>(index) * (rangeChipHeight + rangeChipGap),
                                   std::max(0, rangeRect.width - 6),
                                   rangeChipHeight);
    const bool selected = snapshot.junoDcoRange == static_cast<int>(index);
    if (selected)
    {
      canvas.FillRect(InsetRect(chip, 1, 1), fill);
    }
    canvas.StrokeRect(chip, selected || rangeFocused ? accent : divider);
    canvas.DrawTextInRect(chip,
                          rangeLabels[index],
                          selected || rangeFocused ? accent : text,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
  }

  const bool pwmFocused = IsJunoFocusedParameter(snapshot, "dco.pwm");
  if (pwmRect.width > 0 && pwmRect.height > 0)
  {
    if (pwmFocused)
    {
      canvas.FillRect(InsetRect(pwmRect, 1, 1), fill);
    }
    canvas.StrokeRect(pwmRect, pwmFocused ? accent : divider);
    canvas.DrawText(pwmRect.x + 4, pwmRect.y + 3, "PWM", pwmFocused ? accent : text, 1);
    canvas.DrawTextInRect(MakeRect(pwmRect.x, pwmRect.y + 2, pwmRect.width - 4, 10),
                          std::to_string(snapshot.junoDcoPwm),
                          pwmFocused ? accent : text,
                          1,
                          ui::TextAlign::Right,
                          ui::VerticalAlign::Middle);

    const int railLeft = pwmRect.x + 8;
    const int railRight = pwmRect.x + pwmRect.width - 9;
    const int railY = pwmRect.y + pwmRect.height - 10;
    canvas.FillRect(MakeRect(railLeft, railY, std::max(0, railRight - railLeft + 1), 2), divider);

    const float pwmNormalized = JunoNormalizedContinuous(snapshot.junoDcoPwm);
    const int pwmX = railLeft + static_cast<int>(pwmNormalized * static_cast<float>(std::max(1, railRight - railLeft)) + 0.5f);
    const int filledWidth = std::max(0, pwmX - railLeft);
    if (filledWidth > 0)
    {
      canvas.FillRect(MakeRect(railLeft, railY, filledWidth, 2), pwmFocused ? accent : text);
    }
    canvas.DrawLine(pwmX, railY - 5, pwmX, railY + 5, pwmFocused ? accent : text);
  }

  const int statusGap = 4;
  const int statusWidth = std::max(18, (statusRect.width - statusGap * 2) / 3);
  drawValueCard(MakeRect(statusRect.x, statusRect.y, statusWidth, statusRect.height),
                "SAW",
                JunoToggleText(snapshot.junoDcoSaw),
                snapshot.junoDcoSaw > 0 ? 1.0f : 0.0f,
                IsJunoFocusedParameter(snapshot, "dco.saw"));
  drawValueCard(MakeRect(statusRect.x + statusWidth + statusGap, statusRect.y, statusWidth, statusRect.height),
                "SUB",
                std::to_string(snapshot.junoDcoSub),
                JunoNormalizedContinuous(snapshot.junoDcoSub),
                IsJunoFocusedParameter(snapshot, "dco.sub"));
  drawValueCard(MakeRect(statusRect.x + (statusWidth + statusGap) * 2, statusRect.y, statusWidth, statusRect.height),
                "NOI",
                std::to_string(snapshot.junoDcoNoise),
                JunoNormalizedContinuous(snapshot.junoDcoNoise),
                IsJunoFocusedParameter(snapshot, "dco.noise"));
}

void DrawJunoVcfGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const int left = rect.x + 2;
  const int right = rect.x + rect.width - 2;
  const int top = rect.y + 2;
  const int curveBaseY = top + 12;
  const int curveBottomY = top + 22;
  const int cutoffX = left + 8 + static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcfCutoff) * static_cast<float>(std::max(1, right - left - 16)));
  const int peakHeight = static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcfResonance) * 10.0f);
  const int peakY = curveBaseY - peakHeight;

  canvas.DrawLine(left, curveBaseY, cutoffX - 5, curveBaseY, IsJunoFocusedParameter(snapshot, "vcf.cutoff") ? accent : text);
  canvas.DrawLine(cutoffX - 5, curveBaseY, cutoffX, peakY, IsJunoFocusedParameter(snapshot, "vcf.resonance") ? accent : text);
  canvas.DrawLine(cutoffX, peakY, cutoffX + 5, curveBaseY, IsJunoFocusedParameter(snapshot, "vcf.resonance") ? accent : text);
  canvas.DrawLine(cutoffX + 5, curveBaseY, right, curveBottomY, IsJunoFocusedParameter(snapshot, "vcf.cutoff") ? accent : text);

  const int valueY = top + 30;
  const ui::Color cutColor = IsJunoFocusedParameter(snapshot, "vcf.cutoff") ? accent : text;
  const ui::Color resColor = IsJunoFocusedParameter(snapshot, "vcf.resonance") ? accent : text;
  const ui::Color envColor = IsJunoFocusedParameter(snapshot, "vcf.env") ? accent : text;
  const ui::Color lfoColor = IsJunoFocusedParameter(snapshot, "vcf.lfo") ? accent : text;
  canvas.DrawText(left, valueY, "C", cutColor, 1);
  canvas.DrawText(left + 10, valueY, std::to_string(snapshot.junoVcfCutoff), cutColor, 1);
  canvas.DrawText(left + 40, valueY, "R", resColor, 1);
  canvas.DrawText(left + 50, valueY, std::to_string(snapshot.junoVcfResonance), resColor, 1);
  canvas.DrawText(left, valueY + 10, "E", envColor, 1);
  canvas.DrawText(left + 10, valueY + 10, std::to_string(snapshot.junoVcfEnv), envColor, 1);
  canvas.DrawText(left + 40, valueY + 10, "L", lfoColor, 1);
  canvas.DrawText(left + 50, valueY + 10, std::to_string(snapshot.junoVcfLfo), lfoColor, 1);

  const int chipY = valueY + 20;
  canvas.DrawText(left, chipY + 1, "HPF", IsJunoFocusedParameter(snapshot, "hpf.mode") ? accent : text, 1);
  const int chipWidth = 10;
  const int chipGap = 4;
  const int chipStartX = left + 26;
  for (int index = 0; index < 4; ++index)
  {
    const ui::Rect chip = MakeRect(chipStartX + index * (chipWidth + chipGap), chipY, chipWidth, 8);
    if (snapshot.junoHpfMode == index)
    {
      canvas.FillRect(InsetRect(chip, 1, 1), fill);
    }
    canvas.StrokeRect(chip, snapshot.junoHpfMode == index ? accent : divider);
  }
}

void DrawJunoEnvGraphic(ui::Canvas& canvas,
                        const ui::Rect& rect,
                        const UiScreenSnapshot& snapshot,
                        float alpha,
                        bool showValues)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const int left = rect.x + 4;
  const int right = rect.x + rect.width - 4;
  const int top = rect.y + 4;
  const int bottom = rect.y + rect.height - (showValues ? 16 : 6);
  const int width = std::max(20, right - left);
  const int attackWidth = std::max(6, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvAttack) * width * 0.22f));
  const int decayWidth = std::max(6, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvDecay) * width * 0.24f));
  const int releaseWidth = std::max(6, static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvRelease) * width * 0.24f));
  const int sustainStartX = std::min(right - releaseWidth - 6, left + attackWidth + decayWidth);
  const int sustainY = bottom - static_cast<int>(JunoNormalizedContinuous(snapshot.junoEnvSustain) * static_cast<float>(bottom - top));

  canvas.DrawLine(left, bottom, left + attackWidth, top, IsJunoFocusedParameter(snapshot, "env.attack") ? accent : divider);
  canvas.DrawLine(left + attackWidth,
                  top,
                  sustainStartX,
                  sustainY,
                  IsJunoFocusedParameter(snapshot, "env.decay") ? accent : divider);
  canvas.DrawLine(sustainStartX,
                  sustainY,
                  right - releaseWidth,
                  sustainY,
                  IsJunoFocusedParameter(snapshot, "env.sustain") ? accent : divider);
  canvas.DrawLine(right - releaseWidth,
                  sustainY,
                  right,
                  bottom,
                  IsJunoFocusedParameter(snapshot, "env.release") ? accent : divider);

  if (showValues)
  {
    const int valueY = rect.y + rect.height - 9;
    const int cellWidth = std::max(1, rect.width / 4);
    const std::array<std::pair<const char*, int>, 4> values = {{
        {"A", snapshot.junoEnvAttack},
        {"D", snapshot.junoEnvDecay},
        {"S", snapshot.junoEnvSustain},
        {"R", snapshot.junoEnvRelease},
    }};
    const std::array<const char*, 4> ids = {"env.attack", "env.decay", "env.sustain", "env.release"};
    for (std::size_t index = 0; index < values.size(); ++index)
    {
      const ui::Color valueColor = IsJunoFocusedParameter(snapshot, ids[index]) ? accent : text;
      const int x = rect.x + static_cast<int>(index) * cellWidth;
      const std::string valueText = std::string(values[index].first) + std::to_string(values[index].second);
      canvas.DrawTextInRect(MakeRect(x, valueY, cellWidth, 8),
                            TruncateText(canvas, valueText, cellWidth - 2, 1),
                            valueColor,
                            1,
                            ui::TextAlign::Center,
                            ui::VerticalAlign::Middle);
    }
  }
}

void DrawJunoLfoGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const int left = rect.x + 4;
  const int right = rect.x + rect.width - 4;
  const int top = rect.y + 4;
  const int waveBottom = top + rect.height / 2 - 2;
  const int middleY = (top + waveBottom) / 2;
  const int cycleWidth = std::max(16, 18 + static_cast<int>(JunoNormalizedContinuous(snapshot.junoLfoRate) * 18.0f));

  for (int x = left; x + cycleWidth <= right; x += cycleWidth)
  {
    canvas.DrawLine(x, middleY, x + cycleWidth / 2, top, divider);
    canvas.DrawLine(x + cycleWidth / 2, top, x + cycleWidth, middleY, divider);
  }

  const int delayX = left + static_cast<int>(JunoNormalizedContinuous(snapshot.junoLfoDelay) * static_cast<float>(right - left));
  canvas.DrawLine(delayX, top - 1, delayX, waveBottom + 2, IsJunoFocusedParameter(snapshot, "lfo.delay") ? accent : text);

  const int rateY = waveBottom + 10;
  const ui::Color rateColor = IsJunoFocusedParameter(snapshot, "lfo.rate") ? accent : text;
  const ui::Color delayColor = IsJunoFocusedParameter(snapshot, "lfo.delay") ? accent : text;
  canvas.DrawText(left, rateY, "RATE", rateColor, 1);
  canvas.DrawText(left + 30, rateY, std::to_string(snapshot.junoLfoRate), rateColor, 1);
  canvas.DrawText(left, rateY + 10, "DLY", delayColor, 1);
  canvas.DrawText(left + 30, rateY + 10, std::to_string(snapshot.junoLfoDelay), delayColor, 1);
}

void DrawJunoAmpGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const int meterWidth = 18;
  const int meterHeight = std::max(8, rect.height - 10);
  const int meterX = rect.x + rect.width / 2 - meterWidth / 2;
  const int meterY = rect.y + 4;
  const int fillHeight = static_cast<int>(JunoNormalizedContinuous(snapshot.junoVcaLevel) * static_cast<float>(meterHeight - 2));
  const ui::Rect meterRect = MakeRect(meterX, meterY, meterWidth, meterHeight);
  canvas.StrokeRect(meterRect, divider);
  canvas.FillRect(MakeRect(meterX + 1, meterY + meterHeight - 1 - fillHeight, meterWidth - 2, fillHeight),
                  IsJunoFocusedParameter(snapshot, "vca.level") ? accent : fill);
}

void DrawJunoFxGraphic(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color fill = FadeColor(kAccent, alpha * 0.18f);
  const std::array<const char*, 3> labels = {"OFF", "I", "II"};
  const int gap = 6;
  const int boxWidth = std::max(12, (rect.width - gap * 2) / 3);
  const int boxHeight = 18;
  const int startX = rect.x + (rect.width - (boxWidth * 3 + gap * 2)) / 2;
  const int boxY = rect.y + rect.height / 2 - boxHeight / 2;
  for (int index = 0; index < 3; ++index)
  {
    const ui::Rect box = MakeRect(startX + index * (boxWidth + gap), boxY, boxWidth, boxHeight);
    if (snapshot.junoChorusMode == index)
    {
      canvas.FillRect(InsetRect(box, 1, 1), fill);
    }
    canvas.StrokeRect(box, snapshot.junoChorusMode == index ? accent : divider);
    canvas.DrawTextInRect(box,
                          labels[index],
                          snapshot.junoChorusMode == index || IsJunoFocusedParameter(snapshot, "chorus.mode") ? accent : text,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
  }
}

void DrawJunoSimpleValuePage(ui::Canvas& canvas,
                             const ui::Rect& rect,
                             const std::string& section,
                             const std::string& label,
                             const std::string& value,
                             float alpha)
{
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const int valueScale = value.size() > 14 ? 2 : (value.size() > 8 ? 3 : 5);

  canvas.DrawText(rect.x, rect.y + 10, TruncateText(canvas, section, rect.width, 1), accent, 1);
  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 30, rect.width, 24),
                        TruncateText(canvas, label, rect.width, 2),
                        text,
                        2,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
  canvas.DrawLine(rect.x, rect.y + 58, rect.x + rect.width - 1, rect.y + 58, divider);
  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 66, rect.width, rect.height - 72),
                        TruncateText(canvas, value, rect.width, valueScale),
                        text,
                        valueScale,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

void DrawJunoParameterContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const std::string section = snapshot.headerPrimary.empty() ? std::string{"Juno"} : snapshot.headerPrimary;
  if (snapshot.focusItemId.empty())
  {
    DrawJunoOverview(canvas, rect, snapshot, alpha);
    return;
  }

  if ((section == "VCA" && snapshot.focusItemId == "vca.level") ||
      (section == "CHO" && (snapshot.focusItemId == "chorus.mode" || snapshot.focusItemId == "par.juno.group.fx")) ||
      (section == "HPF" && (snapshot.focusItemId == "hpf.mode" || snapshot.focusItemId == "par.juno.group.hpf")))
  {
    DrawJunoSimpleValuePage(canvas, rect, section, snapshot.primaryText, snapshot.secondaryText, alpha);
    return;
  }

  if (section == "DCO" && IsJunoDiscreteValuePage(snapshot))
  {
    DrawJunoSimpleValuePage(canvas, rect, section, snapshot.primaryText, snapshot.secondaryText, alpha);
    return;
  }

  if (section == "ENV")
  {
    DrawJunoEnvPage(canvas, rect, snapshot, alpha);
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const bool compactGraphicPage = section == "DCO" || section == "VCF" || section == "LFO";

  canvas.DrawText(rect.x, rect.y + 10, TruncateText(canvas, section, rect.width, 1), accent, 1);
  if (!compactGraphicPage)
  {
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 26, rect.width, 18),
                          TruncateText(canvas, snapshot.primaryText, rect.width, 2),
                          text,
                          2,
                          ui::TextAlign::Left,
                          ui::VerticalAlign::Middle);
  }

  const int dividerY = compactGraphicPage ? rect.y + 24 : rect.y + 46;
  canvas.DrawLine(rect.x, dividerY, rect.x + rect.width - 1, dividerY, divider);

  const ui::Rect graphRect = MakeRect(rect.x,
                                      dividerY + 6,
                                      rect.width,
                                      std::max(24, rect.height - (compactGraphicPage ? 36 : 80)));
  if (section == "DCO")
  {
    DrawJunoDcoGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "VCF")
  {
    DrawJunoVcfGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "LFO")
  {
    DrawJunoLfoGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "VCA")
  {
    DrawJunoAmpGraphic(canvas, graphRect, snapshot, alpha);
  }
  else if (section == "CHO")
  {
    DrawJunoFxGraphic(canvas, graphRect, snapshot, alpha);
  }
  else
  {
    DrawJunoOverview(canvas, graphRect, snapshot, alpha);
  }

  if (section != "DCO" && section != "VCF" && section != "LFO")
  {
    const int footerScale = snapshot.secondaryText.size() > 18 ? 1 : (snapshot.secondaryText.size() > 10 ? 2 : 3);
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + rect.height - 24, rect.width, 18),
                          TruncateText(canvas, snapshot.secondaryText, rect.width, footerScale),
                          text,
                          footerScale,
                          ui::TextAlign::Left,
                          ui::VerticalAlign::Middle);
  }
}

void DrawSoundContextContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);

  if (!snapshot.showSecondaryText)
  {
    canvas.DrawText(rect.x,
                    rect.y + 10,
                    TruncateText(canvas, snapshot.headerPrimary, rect.width, 1),
                    accent,
                    1);
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 34, rect.width, 34),
                          TruncateText(canvas, snapshot.primaryText, rect.width, 3),
                          text,
                          3,
                          ui::TextAlign::Left,
                          ui::VerticalAlign::Middle);
    return;
  }

  if (snapshot.showPresetBrowser)
  {
    canvas.DrawText(rect.x,
                    rect.y + 10,
                    TruncateText(canvas, snapshot.primaryText, rect.width, 2),
                    accent,
                    2);
    canvas.DrawLine(rect.x,
                    rect.y + 34,
            rect.x + rect.width - 1,
                    rect.y + 34,
                    divider);

    const int lineScale = 1;
    const int lineHeight = canvas.FontHeight(lineScale) + 2;
    int lineY = rect.y + 42;
    std::size_t start = 0;
    for (int lineIndex = 0; lineIndex < 7 && start <= snapshot.secondaryText.size(); ++lineIndex)
    {
      const std::size_t newline = snapshot.secondaryText.find('\n', start);
      const std::string encodedLine = newline == std::string::npos
                                          ? snapshot.secondaryText.substr(start)
                                          : snapshot.secondaryText.substr(start, newline - start);
      const bool selected = !encodedLine.empty() && encodedLine.front() == '>';
      const std::string line = !encodedLine.empty() && (encodedLine.front() == '>' || encodedLine.front() == ' ')
                                   ? encodedLine.substr(1)
                                   : encodedLine;
      const ui::Color lineColor = selected ? accent : text;
                      canvas.DrawText(rect.x,
                      lineY,
                              TruncateText(canvas, line, rect.width, lineScale),
                      lineColor,
                      lineScale);
      if (newline == std::string::npos)
      {
        break;
      }

      start = newline + 1;
      lineY += lineHeight;
    }
    return;
  }

  canvas.DrawText(rect.x,
                  rect.y + 10,
                  TruncateText(canvas, snapshot.headerPrimary, rect.width, 1),
                  accent,
                  1);

  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 30, rect.width, 24),
                        TruncateText(canvas, snapshot.primaryText, rect.width, 2),
                        text,
                        2,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
  canvas.DrawLine(rect.x,
                  rect.y + 58,
                  rect.x + rect.width - 1,
                  rect.y + 58,
                  divider);
  const int presetScale = canvas.MeasureTextWidth(snapshot.secondaryText, 5) <= rect.width ? 5 : 4;
  DrawMarqueeTextInRect(canvas,
                        MakeRect(rect.x, rect.y + 64, rect.width, 42),
                        snapshot.secondaryText,
                        text,
                        presetScale,
                        ui::TextAlign::Left);
}

void DrawHomeTrackLabel(ui::Canvas& canvas, const ui::Rect& rect, const std::string& trackLabel, float alpha)
{
  if (trackLabel.empty() || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  const std::size_t separator = trackLabel.find_last_of(' ');
  if (separator == std::string::npos || separator + 1 >= trackLabel.size())
  {
    canvas.DrawTextInRect(rect, trackLabel, text, 1, ui::TextAlign::Right, ui::VerticalAlign::Top);
    return;
  }

  const std::string label = trackLabel.substr(0, separator + 1);
  const std::string number = trackLabel.substr(separator + 1);
  const int numberWidth = canvas.MeasureTextWidth(number, 1);
  const int labelWidth = canvas.MeasureTextWidth(label, 1);
  const int rightEdge = rect.x + rect.width;
  const int labelX = std::max(rect.x, rightEdge - numberWidth - labelWidth);
  const int numberX = std::max(rect.x, rightEdge - numberWidth);

  canvas.DrawText(labelX, rect.y, label, text, 1);
  canvas.DrawText(numberX, rect.y, number, accent, 1);
}

ScreenLayout LayoutForSnapshot(const ui::Canvas& canvas, const UiScreenSnapshot& snapshot)
{
  ScreenLayout layout;
  const int leftWidth = BarWidthForColumns(snapshot.leftBarColumns, snapshot.leftBarWidthScale);
  layout.leftBar = MakeRect(snapshot.leftBarVisible ? 0 : -leftWidth, 0, leftWidth, canvas.Height());

  const int rightWidth = snapshot.rightBarVisible ? BarWidthForColumns(snapshot.rightBarColumns, snapshot.rightBarWidthScale) : 0;
  layout.rightBar = MakeRect(canvas.Width() - rightWidth, 0, rightWidth, canvas.Height());

  const int contentLeftPadding = snapshot.mode == UiMode::Home ? kHomeContentLeftPadding : kContextContentLeftPadding;
  const int contentRightPadding = snapshot.mode == UiMode::Home ? kHomeContentRightPadding : kContextContentRightPadding;
  const int contentLeft = (snapshot.leftBarVisible ? leftWidth : 0) + contentLeftPadding;
  const int contentRight = layout.rightBar.x - contentRightPadding;
  layout.content = MakeRect(contentLeft, 0, std::max(0, contentRight - contentLeft), canvas.Height());
  return layout;
}

std::string TruncateText(const ui::Canvas& canvas, std::string value, int maxWidth, int scale)
{
  if (canvas.MeasureTextWidth(value, scale) <= maxWidth)
  {
    return value;
  }

  while (value.size() > 3 && canvas.MeasureTextWidth(value + "...", scale) > maxWidth)
  {
    value.pop_back();
  }
  return value + "...";
}

void DrawFilledCircle(ui::Canvas& canvas, int centerX, int centerY, int radius, ui::Color color)
{
  if (radius <= 0)
  {
    return;
  }

  int x = radius;
  int y = 0;
  int error = 1 - x;

  while (x >= y)
  {
    canvas.DrawLine(centerX - x, centerY + y, centerX + x, centerY + y, color);
    canvas.DrawLine(centerX - y, centerY + x, centerX + y, centerY + x, color);
    canvas.DrawLine(centerX - x, centerY - y, centerX + x, centerY - y, color);
    canvas.DrawLine(centerX - y, centerY - x, centerX + y, centerY - x, color);

    ++y;
    if (error < 0)
    {
      error += 2 * y + 1;
    }
    else
    {
      --x;
      error += 2 * (y - x) + 1;
    }
  }
}

void DrawBarIcon(ui::Canvas& canvas, const ui::Rect& rect, UiBarIcon icon, ui::Color color)
{
  if (icon == UiBarIcon::None || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int centerX = rect.x + rect.width / 2;
  const int centerY = rect.y + rect.height / 2;
  const int iconRadius = std::max(3, std::min(rect.width, rect.height) / 4);
  const int halfHeight = std::max(3, rect.height / 4);

  switch (icon)
  {
    case UiBarIcon::Plus:
      canvas.DrawLine(centerX - iconRadius, centerY, centerX + iconRadius, centerY, color);
      canvas.DrawLine(centerX, centerY - iconRadius, centerX, centerY + iconRadius, color);
      break;
    case UiBarIcon::Minus:
      canvas.DrawLine(centerX - iconRadius, centerY, centerX + iconRadius, centerY, color);
      break;
    case UiBarIcon::Circle:
      DrawFilledCircle(canvas, centerX, centerY, std::max(5, iconRadius), color);
      break;
    case UiBarIcon::Play:
    {
      const int leftX = centerX - std::max(3, rect.width / 6);
      const int rightX = centerX + std::max(3, rect.width / 6);
      for (int y = -halfHeight; y <= halfHeight; ++y)
      {
        const int horizontalExtent = static_cast<int>(static_cast<float>(rightX - leftX) *
                                                      (1.0f - static_cast<float>(std::abs(y)) / std::max(1, halfHeight)) +
                                                      0.5f);
        canvas.DrawLine(leftX, centerY + y, leftX + horizontalExtent, centerY + y, color);
      }
      break;
    }
    case UiBarIcon::None:
    default:
      break;
  }
}

void DrawBarItem(ui::Canvas& canvas, const ui::Rect& rect, const UiBarItemSnapshot& item, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0 ||
      (item.label.empty() && item.icon == UiBarIcon::None))
  {
    return;
  }

  const ui::Color border = FadeColor(item.active ? kAccent : kDivider, alpha);
  const ui::Color text = FadeColor(item.active ? kAccent : kText, alpha);

  canvas.StrokeRect(rect, border);

  if (item.icon != UiBarIcon::None)
  {
    DrawBarIcon(canvas, MakeRect(rect.x + 4, rect.y + 4, std::max(0, rect.width - 8), std::max(0, rect.height - 8)), item.icon, text);
    return;
  }

  const ui::Rect textRect = MakeRect(rect.x + 2, rect.y + 1, std::max(0, rect.width - 4), std::max(0, rect.height - 2));

  canvas.DrawTextInRect(textRect,
                        TruncateText(canvas, item.label, textRect.width, 1),
                        text,
                        1,
                        ui::TextAlign::Center,
                        ui::VerticalAlign::Middle);
}

void DrawSegmentedBar(ui::Canvas& canvas,
                      const ui::Rect& rect,
                      std::size_t columns,
                      const UiBarSnapshot& items,
                      BarAnchor anchor,
                      float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0 || columns == 0)
  {
    return;
  }

  (void)anchor;

  const int innerX = rect.x + kBarOuterPadding;
  const int innerY = rect.y + kBarOuterPadding;
  const int innerWidth = std::max(0, rect.width - kBarOuterPadding * 2);
  const int innerHeight = std::max(0, rect.height - kBarOuterPadding * 2);
  const int cellWidth = std::max(0,
                                 (innerWidth - static_cast<int>(columns - 1) * kBarCellGap) /
                                     static_cast<int>(columns));
  const int cellHeight = std::max(0,
                                  (innerHeight - static_cast<int>(kContextMenuRows - 1) * kBarCellGap) /
                                      static_cast<int>(kContextMenuRows));

  for (std::size_t row = 0; row < kContextMenuRows; ++row)
  {
    const int y0 = innerY + static_cast<int>(row) * (cellHeight + kBarCellGap);
    for (std::size_t column = 0; column < columns; ++column)
    {
      const std::size_t index = columns == 1 ? row : row * kContextMenuColumns + column;
      const int x0 = innerX + static_cast<int>(column) * (cellWidth + kBarCellGap);
      DrawBarItem(canvas, MakeRect(x0, y0, cellWidth, cellHeight), items[index], alpha);
    }
  }
}

void DrawMiniKeyboard(ui::Canvas& canvas,
                      const ui::Rect& rect,
                      const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                      float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);

  canvas.StrokeRect(rect, divider);

  constexpr int kWhiteKeyCount = 10;
  for (int index = 1; index < kWhiteKeyCount; ++index)
  {
    const int x = rect.x + index * rect.width / kWhiteKeyCount;
    canvas.DrawLine(x, rect.y, x, rect.y + rect.height - 1, divider);
  }

  const int blackKeyHeight = rect.height * 3 / 5;
  const int blackKeyWidth = std::max(4, rect.width / 28);
  constexpr std::array<int, 7> kBlackKeySlots = {1, 2, 4, 5, 6, 8, 9};
  for (int slot : kBlackKeySlots)
  {
    const int centerX = rect.x + slot * rect.width / kWhiteKeyCount;
    canvas.StrokeRect(MakeRect(centerX - blackKeyWidth / 2, rect.y, blackKeyWidth, blackKeyHeight), divider);
  }

  for (std::size_t index = 0; index < activeKeys.size(); ++index)
  {
    if (!activeKeys[index])
    {
      continue;
    }

    const int x = rect.x + 2 + static_cast<int>(index) * std::max(1, rect.width - 4) / static_cast<int>(activeKeys.size());
    canvas.DrawLine(x, rect.y + rect.height - 4, x + 3, rect.y + rect.height - 4, accent);
  }
}

void DrawThinLooperBar(ui::Canvas& canvas, const ui::Rect& rect, float progress, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  const int segmentWidth = std::max(0, (rect.width - (kLoopBarCount - 1) * kLoopBarGap) / kLoopBarCount);

  for (int index = 0; index < kLoopBarCount; ++index)
  {
    const int x = rect.x + index * (segmentWidth + kLoopBarGap);
    const ui::Rect segmentRect = MakeRect(x, rect.y, segmentWidth, rect.height);
    canvas.FillRect(segmentRect, divider);

    const float segmentProgress = Clamp01(progress * static_cast<float>(kLoopBarCount) - static_cast<float>(index));
    const int fillWidth = std::clamp(static_cast<int>(static_cast<float>(segmentRect.width) * segmentProgress), 0, segmentRect.width);
    if (fillWidth > 0)
    {
      canvas.FillRect(MakeRect(segmentRect.x, segmentRect.y, fillWidth, segmentRect.height), accent);
    }
  }
}

void DrawHomeContent(ui::Canvas& canvas,
                     const ui::Rect& rect,
                     const UiScreenSnapshot& snapshot,
                     const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                     float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const int headerX = rect.x + 4;
  const int headerY = rect.y + 8;
  const int headerGap = 8;
  const int trackLabelWidth = std::min(std::max(56, canvas.MeasureTextWidth(snapshot.headerSecondary, 1) + 6),
                                       std::max(56, rect.width / 2));
  const int trackLabelX = rect.x + rect.width - 4 - trackLabelWidth;
  const int titleWidth = std::max(0, trackLabelX - headerGap - headerX);

  canvas.DrawText(headerX,
                  headerY,
                  TruncateText(canvas, snapshot.headerPrimary, titleWidth, 1),
                  accent,
                  1);
  DrawHomeTrackLabel(canvas,
                     MakeRect(trackLabelX, headerY, trackLabelWidth, 10),
                     snapshot.headerSecondary,
                     alpha);
  canvas.DrawLine(rect.x + 4, rect.y + 24, rect.x + rect.width - 5, rect.y + 24, divider);
  DrawMarqueeTextInRect(canvas,
                        MakeRect(rect.x + 4, rect.y + 34, rect.width - 8, 34),
                        snapshot.primaryText,
                        text,
                        3,
                        ui::TextAlign::Center);

  DrawMiniKeyboard(canvas, MakeRect(rect.x + 16, rect.y + 74, rect.width - 32, 28), activeKeys, alpha);
  DrawThinLooperBar(canvas, MakeRect(rect.x + 4, rect.y + rect.height - 11, rect.width - 8, 4), snapshot.looperProgress, alpha);
}

void DrawContextContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  if (IsDx7ParameterSnapshot(snapshot))
  {
    DrawDx7ParameterContent(canvas, rect, snapshot, alpha);
    return;
  }

  if (IsJunoParameterSnapshot(snapshot))
  {
    DrawJunoParameterContent(canvas, rect, snapshot, alpha);
    return;
  }

  if (snapshot.modifier == UiModifier::Sound)
  {
    DrawSoundContextContent(canvas, rect, snapshot, alpha);
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);

  const int titleScale = snapshot.showSecondaryText ? 2 : 2;
  const int titleY = rect.y + 10;
  const int titleHeight = canvas.FontHeight(titleScale);
  const std::string title = snapshot.primaryText.empty() ? snapshot.headerPrimary : snapshot.primaryText;
  canvas.DrawText(rect.x,
                  titleY,
                  TruncateText(canvas, title, rect.width, titleScale),
                  accent,
                  titleScale);

  const int dividerY = titleY + titleHeight + 6;
  canvas.DrawLine(rect.x, dividerY, rect.x + rect.width - 1, dividerY, divider);

  if (!snapshot.showSecondaryText)
  {
    return;
  }

  canvas.DrawTextInRect(MakeRect(rect.x,
                                 dividerY + 8,
                                 rect.width,
                                 rect.height - (dividerY - rect.y) - 12),
                        TruncateText(canvas, snapshot.secondaryText, rect.width, 5),
                        text,
                        5,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

void DrawSnapshotContent(ui::Canvas& canvas,
                         const ui::Rect& rect,
                         const UiScreenSnapshot& snapshot,
                         const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                         float alpha)
{
  if (snapshot.mode == UiMode::Home)
  {
    DrawHomeContent(canvas, rect, snapshot, activeKeys, alpha);
    return;
  }

  DrawContextContent(canvas, rect, snapshot, alpha);
}

void DrawSnapshotFrame(ui::Canvas& canvas,
                       const UiScreenSnapshot& snapshot,
                       const ScreenLayout& layout,
                       const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                       float alpha)
{
  DrawSegmentedBar(canvas, layout.leftBar, snapshot.leftBarColumns, snapshot.leftBarItems, BarAnchor::Left, alpha);
  DrawSegmentedBar(canvas, layout.rightBar, snapshot.rightBarColumns, snapshot.rightBarItems, BarAnchor::Right, alpha);
  DrawSnapshotContent(canvas, layout.content, snapshot, activeKeys, alpha);
}

void DrawTransitionFrame(ui::Canvas& canvas,
                         const UiTransitionSnapshot& transition,
                         const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys)
{
  const float progress = Clamp01(transition.progress);
  const ScreenLayout fromLayout = LayoutForSnapshot(canvas, transition.from);
  const ScreenLayout toLayout = LayoutForSnapshot(canvas, transition.to);

  ScreenLayout blendedLayout;
  blendedLayout.leftBar = LerpRect(fromLayout.leftBar, toLayout.leftBar, progress);
  blendedLayout.content = LerpRect(fromLayout.content, toLayout.content, progress);
  blendedLayout.rightBar = LerpRect(fromLayout.rightBar, toLayout.rightBar, progress);

  DrawSnapshotFrame(canvas, transition.from, blendedLayout, activeKeys, 1.0f - progress);
  DrawSnapshotFrame(canvas, transition.to, blendedLayout, activeKeys, progress);
}

}  // namespace

void RenderSynthUiFrame(ui::Canvas& canvas,
                        const SynthUiModel& model,
                        const ISynthEngine* engine,
                        const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys)
{
  ++gMarqueeFrame;
  canvas.Clear(kBackground);

  if (model.Transition().active)
  {
    DrawTransitionFrame(canvas, model.Transition(), activeKeys);
    return;
  }

  const UiScreenSnapshot snapshot = model.CurrentScreenSnapshot(engine);
  DrawSnapshotFrame(canvas, snapshot, LayoutForSnapshot(canvas, snapshot), activeKeys, 1.0f);
}

}  // namespace synth

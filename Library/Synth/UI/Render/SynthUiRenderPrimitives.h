#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Library/Synth/SynthUiModel.h"
#include "Library/Synth/Core/SynthTypes.h"
#include "OS/UI/Canvas.h"

namespace synth::ui_render {

inline constexpr ui::Color kBackground = ui::Rgb565(0, 0, 0);
inline constexpr ui::Color kDivider = ui::Rgb565(136, 136, 136);
inline constexpr ui::Color kAccent = ui::Rgb565(0, 216, 232);
inline constexpr ui::Color kText = ui::Rgb565(236, 241, 246);

inline constexpr int kBarWidthSingle = 46;
inline constexpr int kBarWidthDouble = 100;
inline constexpr int kBarOuterPadding = 3;
inline constexpr int kBarCellGap = 2;
inline constexpr int kHomeContentLeftPadding = 6;
inline constexpr int kHomeContentRightPadding = 4;
inline constexpr int kContextContentLeftPadding = 10;
inline constexpr int kContextContentRightPadding = 8;
inline constexpr int kLoopBarCount = 4;
inline constexpr int kLoopBarGap = 3;
inline constexpr int kMarqueePauseFrames = 24;
inline constexpr int kMarqueeStepFrames = 2;
inline constexpr int kMarqueeGapPixels = 12;

enum class BarAnchor {
  Left,
  Right,
};

struct ScreenLayout {
  ui::Rect leftBar;
  ui::Rect content;
  ui::Rect rightBar;
};

void AdvanceMarqueeFrame();

ui::Rect MakeRect(int x, int y, int width, int height);
ui::Rect InsetRect(const ui::Rect& rect, int insetX, int insetY);
float Clamp01(float value);
ui::Color MixColor(ui::Color from, ui::Color to, float t);
ui::Color FadeColor(ui::Color color, float alpha);
void DrawMarqueeTextInRect(ui::Canvas& canvas,
                           const ui::Rect& rect,
                           std::string_view text,
                           ui::Color color,
                           int scale = 1,
                           ui::TextAlign fitAlign = ui::TextAlign::Left);
ui::Rect LerpRect(const ui::Rect& from, const ui::Rect& to, float t);
int BarWidthForColumns(std::size_t columns, float widthScale);
std::string TruncateText(const ui::Canvas& canvas, std::string value, int maxWidth, int scale);
void DrawFilledCircle(ui::Canvas& canvas, int centerX, int centerY, int radius, ui::Color color);
void DrawBarIcon(ui::Canvas& canvas, const ui::Rect& rect, UiBarIcon icon, ui::Color color);
void DrawBarItem(ui::Canvas& canvas, const ui::Rect& rect, const UiBarItemSnapshot& item, float alpha);
void DrawSegmentedBar(ui::Canvas& canvas,
                      const ui::Rect& rect,
                      std::size_t columns,
                      const UiBarSnapshot& items,
                      BarAnchor anchor,
                      float alpha);
void DrawMiniKeyboard(ui::Canvas& canvas,
                      const ui::Rect& rect,
                      const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                      float alpha);
void DrawThinLooperBar(ui::Canvas& canvas, const ui::Rect& rect, float progress, float alpha);

}  // namespace synth::ui_render
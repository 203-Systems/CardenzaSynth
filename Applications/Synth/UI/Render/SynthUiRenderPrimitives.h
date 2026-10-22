#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "Applications/Synth/UI/SynthUiModel.h"
#include "Library/Synth/Core/SynthTypes.h"
#include "OS/UI/Canvas.h"

namespace synth::ui_render {

// Paper-like light surfaces reduce large bright areas on the physical LCD.
// RGB565 values are chosen for warm neutral greys after channel expansion.
inline constexpr ui::Color kBackground = ui::Rgb565(232, 232, 224);
inline constexpr ui::Color kSurface = ui::Rgb565(216, 220, 216);
inline constexpr ui::Color kHeader = ui::Rgb565(200, 208, 200);
inline constexpr ui::Color kSelection = ui::Rgb565(184, 216, 208);
inline constexpr ui::Color kRule = ui::Rgb565(128, 144, 144);
inline constexpr ui::Color kDim = ui::Rgb565(64, 80, 80);
inline constexpr ui::Color kDivider = kRule;
inline constexpr ui::Color kAccent = ui::Rgb565(0, 104, 124);
inline constexpr ui::Color kPlayback = ui::Rgb565(24, 119, 66);
inline constexpr ui::Color kDanger = ui::Rgb565(181, 38, 63);
inline constexpr ui::Color kText = ui::Rgb565(24, 32, 32);
inline constexpr ui::Color kInverseText = ui::Rgb565(240, 244, 232);
// Physical key colors are independent of the page's text/background roles.
inline constexpr ui::Color kPianoWhite = ui::Rgb565(240, 240, 232);
inline constexpr ui::Color kPianoBlack = ui::Rgb565(32, 42, 52);

// Soft-key strips mirror the physical key columns; widths are visual only.
inline constexpr int kBarWidthSingle = 50;
inline constexpr int kBarWidthDouble = 103;
inline constexpr int kHomeContentPadding = 6;
inline constexpr int kContextContentPadding = 8;
// Every screen reserves a full-width title bar; content and soft keys share the area below.
inline constexpr int kTitleBarHeight = 24;
inline constexpr int kTitleBarInset = 6;
// Soft keys are separate outlined tiles so each column reads as four keys.
inline constexpr int kBarTilePadding = 2;
inline constexpr int kBarTileGap = 3;
inline constexpr int kListRowPitch = 14;
inline constexpr int kLoopBarHeight = 3;
inline constexpr int kLoopBarGap = 2;
inline constexpr int kMarqueePauseFrames = 24;
inline constexpr int kMarqueeStepFrames = 2;
inline constexpr int kMarqueeGapPixels = 12;

// Shared horizontal grid for everything that mirrors performance columns 1-8.
inline constexpr int kKeyColumnWidth = 13;
inline constexpr int kKeyColumnGap = 3;
inline constexpr int kKeyGridWidth = kKeyColumnWidth * 8 + kKeyColumnGap * 7;

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
ui::Rect LerpRect(const ui::Rect& from, const ui::Rect& to, float t);
int BarWidthForColumns(std::size_t columns, float widthScale);
std::string TruncateText(const ui::Canvas& canvas, std::string value, int maxWidth, int scale);
std::string NoteName(int midiNote);

// Ink helpers place visible 6x9 body / scaled 5x7 heading glyphs, not line boxes.
int InkHeight(int scale);
int InkWidth(const ui::Canvas& canvas, std::string_view text, int scale);
void DrawInk(ui::Canvas& canvas, int x, int top, std::string_view text, ui::Color color, int scale = 1);
void DrawInkClipped(ui::Canvas& canvas, const ui::Rect& clip, int x, int top, std::string_view text, ui::Color color, int scale = 1);
// Largest scale from maxScale down to 1 whose ink fits in maxWidth.
int FitScale(const ui::Canvas& canvas, std::string_view text, int maxWidth, int maxScale);
void DrawMarqueeTextInRect(ui::Canvas& canvas,
                           const ui::Rect& rect,
                           std::string_view text,
                           ui::Color color,
                           int scale = 1,
                           ui::TextAlign fitAlign = ui::TextAlign::Left);

void DrawFilledCircle(ui::Canvas& canvas, int centerX, int centerY, int radius, ui::Color color);
void DrawFrame(ui::Canvas& canvas, const ui::Rect& rect, ui::Color color, int thickness = 2);
void DrawBarIcon(ui::Canvas& canvas, const ui::Rect& rect, UiBarIcon icon, ui::Color color);
ui::Rect BarTileRect(const ui::Rect& bar, std::size_t columns, std::size_t row, std::size_t column);
void DrawBarItem(ui::Canvas& canvas, const ui::Rect& rect, const UiBarItemSnapshot& item, float alpha);
void DrawSegmentedBar(ui::Canvas& canvas,
                      const ui::Rect& rect,
                      std::size_t columns,
                      const UiBarSnapshot& items,
                      BarAnchor anchor,
                      float alpha);

// Title bar: dark 2x page name; to its right a teal 1x first line and an optional
// grey 1x second line; optional extra info at the top right.
void DrawTitleBar(ui::Canvas& canvas,
                  std::string_view title,
                  std::string_view line1,
                  ui::Color line1Color,
                  std::string_view line2,
                  std::string_view status,
                  ui::Color statusColor,
                  float alpha);
// Title bar for a context page. Returns the first body y inside rect.
int DrawPageTitle(ui::Canvas& canvas,
                  const ui::Rect& rect,
                  std::string_view title,
                  std::string_view line1,
                  ui::Color line1Color,
                  std::string_view line2,
                  std::string_view status,
                  float alpha);
// Title with a single first line; red and green first lines are kept, any other color becomes cyan.
int DrawPageHeader(ui::Canvas& canvas,
                   const ui::Rect& rect,
                   std::string_view title,
                   std::string_view meta,
                   ui::Color titleColor,
                   ui::Color metaColor,
                   float alpha);
void DrawMeter(ui::Canvas& canvas, const ui::Rect& rect, float level, ui::Color fill, float alpha);

// Values drawn on the same rows as their soft keys.
ui::Rect KeyValueEntryRect(const ui::Rect& content, int canvasHeight, std::size_t columns, std::size_t row, std::size_t column);
void DrawKeyValues(ui::Canvas& canvas,
                       const ui::Rect& content,
                       std::size_t columns,
                       const UiBarSnapshot& items,
                       float alpha);
void DrawListRows(ui::Canvas& canvas, const ui::Rect& rect, const std::string& encodedLines, float alpha);
void DrawTextEntry(ui::Canvas& canvas, const ui::Rect& rect, std::string_view text, float alpha);
void DrawSubject(ui::Canvas& canvas, const ui::Rect& rect, std::string_view subject, std::string_view note, bool dangerous, float alpha);
void DrawAdjustValue(ui::Canvas& canvas,
                     const ui::Rect& rect,
                     std::string_view name,
                     std::string_view value,
                     float level,
                     float alpha);
std::string WrapTextToWidth(const ui::Canvas& canvas, std::string_view text, int maxWidth, int scale, int maxLines);

// One-octave keyboard that lights the pitch classes being played.
void DrawMiniPiano(ui::Canvas& canvas,
                   const ui::Rect& rect,
                   const UiScreenSnapshot& snapshot,
                   const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                   float alpha);
// The 8x4 performance zone as the player sees it under the screen; shows layout and scale on KEY pages.
void DrawPerformanceKeyMap(ui::Canvas& canvas,
                           int x,
                           int y,
                           const UiScreenSnapshot& snapshot,
                           const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                           float alpha);
// "OCT +1" while the octave is shifted or was just changed; empty at the default octave.
std::string OctaveStatusText(const UiScreenSnapshot& snapshot);
std::string HeldNoteNames(const UiScreenSnapshot& snapshot,
                          const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys);
void DrawLoopBar(ui::Canvas& canvas,
                 const ui::Rect& rect,
                 std::size_t segmentCount,
                 std::size_t activeSegment,
                 float progress,
                 bool playing,
                 float alpha);

}  // namespace synth::ui_render

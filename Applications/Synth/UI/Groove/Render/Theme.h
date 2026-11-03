#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "OS/UI/Canvas.h"

// Terminal / neo-minimal / dither palette and layout grid. See docs/groovebox-design.md section 5.
namespace groove::theme {

using ui::Color;
using ui::Rgb565;

inline constexpr Color kBlack = Rgb565(0, 0, 0);
inline constexpr Color kInk = Rgb565(232, 232, 224);
inline constexpr Color kDim = Rgb565(132, 132, 126);
inline constexpr Color kFaint = Rgb565(58, 58, 54);
inline constexpr Color kShade = Rgb565(26, 26, 24);

// One color per hidden left key; the same hue marks what that key controls everywhere.
inline constexpr Color kEsc = Rgb565(255, 90, 54);     // scenes / back
inline constexpr Color kTab = Rgb565(54, 214, 230);    // tracks
inline constexpr Color kFn = Rgb565(255, 208, 48);     // second functions
inline constexpr Color kCtrl = Rgb565(160, 112, 255);  // pages

inline constexpr Color kPlay = Rgb565(72, 255, 120);
inline constexpr Color kRec = Rgb565(255, 52, 68);

// One color per track, everywhere a track is named or drawn (header, TAB strip, maps, MIX, SENDS,
// TAPE rows). Kept apart from the four left-key colors and from play green / record red.
inline constexpr Color kTrackColors[4] = {
    Rgb565(255, 92, 176),   // T1 pink
    Rgb565(72, 152, 255),   // T2 blue
    Rgb565(176, 236, 56),   // T3 lime
    Rgb565(255, 164, 40),   // T4 amber
};
inline constexpr Color TrackColor(std::size_t track) { return kTrackColors[track & 3u]; }

// Standby's bouncing word takes the next of these at every wall: bright, so they read on a dimmed screen.
inline constexpr std::array<Color, 8> kStandbyPalette{{
    Rgb565(232, 232, 224),  // white
    Rgb565(54, 214, 230),   // cyan
    Rgb565(160, 112, 255),  // violet
    Rgb565(72, 255, 120),   // green
    Rgb565(255, 208, 48),   // yellow
    Rgb565(255, 92, 176),   // pink
    Rgb565(51, 177, 255),   // sky blue
    Rgb565(255, 140, 60),   // orange
}};

// Ordered dither densities (0..16 of a 4x4 Bayer cell).
inline constexpr int kDitherIdle = 3;
inline constexpr int kDitherLatched = 8;
inline constexpr int kDitherSolid = 16;
inline constexpr int kDitherLevelOn = 12;
inline constexpr int kDitherLevelOff = 2;
inline constexpr int kDitherFill = 6;
inline constexpr int kDitherScrim = 10;
inline constexpr int kDitherPanel = 2;

inline constexpr int kScreenWidth = 240;
inline constexpr int kScreenHeight = 135;
inline constexpr int kStripWidth = 8;
inline constexpr int kGlyphColumnX = 232;
inline constexpr int kContentX = 10;
inline constexpr int kContentRight = 229;
inline constexpr int kContentWidth = kContentRight - kContentX;
inline constexpr int kHeaderY = 1;
inline constexpr int kHeaderRuleY = 11;
inline constexpr int kContentY = 14;
inline constexpr int kContentBottom = 122;
inline constexpr int kContentHeight = kContentBottom - kContentY;
inline constexpr int kFooterY = 125;
// Footer items and their highlight blocks (2 pixels around the text) stay 2 pixels clear of the
// edge caps on both sides.
inline constexpr int kFooterX = kContentX + 2;
inline constexpr int kFooterRight = kContentRight - 2;
inline constexpr int kFooterWidth = kFooterRight - kFooterX;

// The 4x4 cell grid mirrors the GRID keys (matrix columns 9..12).
inline constexpr int kCellGap = 3;
inline constexpr int kCellWidth = (kContentWidth - 3 * kCellGap) / 4;
inline constexpr int kCellHeight = (kContentHeight - 3 * kCellGap) / 4;

}  // namespace groove::theme

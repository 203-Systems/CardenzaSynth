#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "OS/UI/Canvas.h"

// Terminal / neo-minimal / dither palette and layout grid.
namespace groove::theme {

using ui::Color;
using ui::Rgb565;

// A backlit monochrome LCD, the way an MPC's screen reads: pale green paper, dark ink, and nothing
// between them but shades of the same ink. The names keep their old meaning in the code - kBlack is
// still the paper a panel is cleared to and kInk still the mark that reads on it - so the whole UI
// inverts with these five values and no drawing code has to know which way round it is.
//
// The ladder runs light to dark: kBlack (paper) > kShade (a key's shadow) > kFaint (a hairline) >
// kDim (a second reading) > kInk (the mark). Anything that used to glow on black now sits as ink on
// green, and a dither reads as the screen's own dot grid.
inline constexpr Color kBlack = Rgb565(170, 196, 150);  // the paper: backlit green
inline constexpr Color kInk = Rgb565(24, 32, 20);       // the mark
inline constexpr Color kDim = Rgb565(82, 101, 68);      // a second reading
inline constexpr Color kFaint = Rgb565(116, 140, 98);   // a hairline on the paper
inline constexpr Color kShade = Rgb565(148, 172, 126);  // a shade under a key, barely there

// One mark per hidden left key. On green paper a colour has to be dark enough to read as ink, so
// these are the muted versions of the four - a dyed ink rather than a light behind glass.
inline constexpr Color kEsc = Rgb565(158, 76, 56);    // scenes / back
inline constexpr Color kTab = Rgb565(56, 104, 126);   // tracks
inline constexpr Color kFn = Rgb565(150, 120, 44);    // second functions
inline constexpr Color kCtrl = Rgb565(108, 88, 138);  // pages

inline constexpr Color kPlay = Rgb565(0, 0, 0);       // running: the darkest the ink goes
inline constexpr Color kRec = Rgb565(150, 52, 46);    // armed

// One colour per track, everywhere a track is named or drawn (header, TAB strip, maps, MIX, SENDS,
// TAPE rows). Six inks of equal weight - none lighter than the screen's own second reading - so a
// track is told apart by its dye and not by how bright it shouts.
inline constexpr Color kTrackColors[6] = {
    Rgb565(150, 70, 58),    // T1 terracotta
    Rgb565(58, 82, 128),    // T2 indigo
    Rgb565(94, 112, 52),    // T3 olive
    Rgb565(148, 114, 44),   // T4 ochre
    Rgb565(104, 84, 132),   // T5 plum
    Rgb565(52, 108, 104),   // T6 teal
};
inline constexpr Color TrackColor(std::size_t track) { return kTrackColors[track % 6u]; }

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

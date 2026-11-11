#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "OS/UI/Canvas.h"

// Terminal / neo-minimal / dither palette and layout grid.
namespace groove::theme {

using ui::Color;
using ui::Rgb565;

// Three screens to choose between, one set of names to draw with.
//
// kBlack is whatever a panel is cleared to and kInk whatever reads on it, so the same drawing code
// serves a dark screen (a mark on black), a light one (ink on paper) and a backlit green LCD
// without knowing which way round it is. The five-value ladder keeps its order in all three: kBlack
// is the field, kInk the mark, and kShade / kFaint / kDim the three steps between them.
enum class ThemeId : std::uint8_t {
  Dark = 0,   // the shipped screen
  Light = 1,  // white paper, black ink, the same colours as the dark one
  Retro = 2,  // backlit green LCD
};

struct Palette {
  Color paper;   // kBlack: what a panel is cleared to
  Color ink;     // kInk: the mark
  Color dim;     // kDim: a second reading
  Color faint;   // kFaint: a hairline
  Color shade;   // kShade: a shade under a key
  Color esc;
  Color tab;
  Color fn;
  Color ctrl;
  Color play;
  Color rec;
  std::array<Color, 6> tracks;
};

// The shipped look: a terminal's colours on black, each one bright enough to glow.
inline constexpr Palette kDarkPalette{
    Rgb565(0, 0, 0),        // paper
    Rgb565(232, 232, 224),  // ink
    Rgb565(132, 132, 126),  // dim
    Rgb565(58, 58, 54),     // faint
    Rgb565(26, 26, 24),     // shade
    Rgb565(255, 90, 54),    // esc
    Rgb565(54, 214, 230),   // tab
    Rgb565(255, 208, 48),   // fn
    Rgb565(160, 112, 255),  // ctrl
    Rgb565(72, 255, 120),   // play
    Rgb565(255, 52, 68),    // rec
    {Rgb565(255, 92, 176), Rgb565(72, 152, 255), Rgb565(176, 236, 56), Rgb565(255, 164, 40), Rgb565(196, 132, 255),
     Rgb565(96, 240, 200)},
};

// The same colours on paper: white field, black ink, and the six hues exactly as the dark screen has
// them, so a track keeps its identity whichever screen it is read on.
inline constexpr Palette kLightPalette{
    Rgb565(236, 236, 230),  // paper
    Rgb565(22, 22, 20),     // ink
    Rgb565(104, 104, 98),   // dim
    Rgb565(148, 148, 142),  // faint
    Rgb565(216, 216, 208),  // shade
    Rgb565(255, 90, 54),    // esc
    Rgb565(54, 214, 230),   // tab
    Rgb565(255, 208, 48),   // fn
    Rgb565(160, 112, 255),  // ctrl
    Rgb565(20, 150, 60),    // play: the dark screen's green is invisible here, so a deeper one
    Rgb565(235, 40, 56),    // rec
    {Rgb565(255, 92, 176), Rgb565(72, 152, 255), Rgb565(176, 236, 56), Rgb565(255, 164, 40), Rgb565(196, 132, 255),
     Rgb565(96, 240, 200)},
};

// A backlit monochrome LCD, the way an MPC's screen reads: pale green paper, dark ink, and nothing
// between them but shades of the same ink. The ladder runs light to dark - paper, shade, faint, dim,
// ink - and a dither reads as the screen's own dot grid. Colours survive as dye rather than light:
// each is dark enough to sit on the paper as ink.
inline constexpr Palette kRetroPalette{
    Rgb565(170, 196, 150),  // paper
    Rgb565(24, 32, 20),     // ink
    Rgb565(82, 101, 68),    // dim
    Rgb565(116, 140, 98),   // faint
    Rgb565(148, 172, 126),  // shade
    Rgb565(158, 76, 56),    // esc
    Rgb565(56, 104, 126),   // tab
    Rgb565(150, 120, 44),   // fn
    Rgb565(108, 88, 138),   // ctrl
    Rgb565(0, 0, 0),        // play: the darkest the ink goes
    Rgb565(150, 52, 46),    // rec
    {Rgb565(150, 70, 58), Rgb565(58, 82, 128), Rgb565(94, 112, 52), Rgb565(148, 114, 44), Rgb565(104, 84, 132),
     Rgb565(52, 108, 104)},
};

// The palette in use, written by SetTheme and read by every drawing call.
inline Color kBlack = kDarkPalette.paper;
inline Color kInk = kDarkPalette.ink;
inline Color kDim = kDarkPalette.dim;
inline Color kFaint = kDarkPalette.faint;
inline Color kShade = kDarkPalette.shade;

inline Color kEsc = kDarkPalette.esc;
inline Color kTab = kDarkPalette.tab;
inline Color kFn = kDarkPalette.fn;
inline Color kCtrl = kDarkPalette.ctrl;

inline Color kPlay = kDarkPalette.play;
inline Color kRec = kDarkPalette.rec;

// One colour per track, everywhere a track is named or drawn (header, TAB strip, maps, MIX, SENDS,
// TAPE rows).
inline Color kTrackColors[6] = {kDarkPalette.tracks[0], kDarkPalette.tracks[1], kDarkPalette.tracks[2],
                                kDarkPalette.tracks[3], kDarkPalette.tracks[4], kDarkPalette.tracks[5]};
inline Color TrackColor(std::size_t track) { return kTrackColors[track % 6u]; }

inline ThemeId CurrentTheme = ThemeId::Dark;

// Swaps every colour in one go: the next frame draws in the new palette, nothing cached anywhere.
inline void SetTheme(ThemeId id)
{
  const Palette& palette = id == ThemeId::Light ? kLightPalette : id == ThemeId::Retro ? kRetroPalette : kDarkPalette;
  kBlack = palette.paper;
  kInk = palette.ink;
  kDim = palette.dim;
  kFaint = palette.faint;
  kShade = palette.shade;
  kEsc = palette.esc;
  kTab = palette.tab;
  kFn = palette.fn;
  kCtrl = palette.ctrl;
  kPlay = palette.play;
  kRec = palette.rec;
  for (std::size_t track = 0; track < 6; ++track) kTrackColors[track] = palette.tracks[track];
  CurrentTheme = id;
}

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

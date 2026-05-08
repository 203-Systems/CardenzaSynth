# UI Framework V2 Design

Superseded in behavior by `OS/UI/SynthUiFunctionalAlignment.md`.

This file still contains useful renderer architecture ideas such as the font backend direction and scene separation, but its original persistent three-zone scaffold assumption does not match the approved synth UI behavior from `synth_ui_implementation_design_doc.md` and the `图2` mockups.

## Why The Current UI Feels Wrong

The current renderer in `Applications/Synth/UI/SynthUiRenderer.cpp` is still a direct scene painter:

- it hardcodes filled panels and button blocks via `DrawPanel()` and `DrawButton()`
- it assumes a fixed left bar width and a fixed right menu width
- it mixes scaffold layout, scene rendering, and visual state in one file
- it only has a built-in 5x7 bitmap font in `OS/UI/Canvas.cpp`

That is why the screen still looks like a collection of blocks instead of a single instrument UI with three stable zones.

## Decision Summary

UI V2 should keep the framebuffer architecture, but replace the current renderer with a scaffold-based layout system.

The scaffold has exactly three zones:

1. left bar
2. main display
3. right bar

The scaffold is separator-based, not panel-based.

- No boxed buttons
- No filled block cards as the default style
- Bars are lanes separated by lines
- Main display uses header rules, section rules, and inline meters

The visual emphasis comes from typography, spacing, accent lines, and motion.

## Font Decision

### Recommended font backend: Adafruit GFX `gfxfont.h` format

Use the Adafruit GFX font data format as the first TFT font backend, but do not adopt the whole Adafruit display stack as the renderer.

Why this is the right first step:

- it is a widely used TFT font format
- proportional bitmap fonts already exist
- TTF to GFXfont conversion already exists via `fontconvert`
- the format is just font data plus glyph metadata, so it can be rendered into our framebuffer on both simulator and hardware
- it does not force an Arduino or driver-specific display abstraction into this codebase

This means we can keep `Canvas` as our framebuffer primitive and only replace the text layer.

### Not recommended as the core UI library: TFT_eSPI

TFT_eSPI is strong for driver integration, sprites, and smooth fonts, but it is too tightly coupled to board/display setup and an Arduino-style graphics stack to become the core of this UI framework.

It is useful as reference material, but not as the main dependency for ComputeCardOS UI rendering.

### Optional phase 2

If we later want anti-aliased text, the same text API can gain a second backend for smooth font atlases or VLW-style assets. That should be a later decision, not a blocker for UI V2.

## Visual Language

The screen should read as a single instrument surface, not a dashboard of widgets.

### Global rules

- background stays flat and quiet
- only use 1px separators and accent rules between regions
- avoid filled containers except for meters, selection bands, and temporary focus states
- text hierarchy does most of the work
- active state should be shown with accent lines, left-edge markers, underline motion, or inverted text only when necessary

### Left bar

The left bar is a vertical mode rail.

- rows: always 4
- columns: 0 or 1
- rows are separated by rules, not boxed buttons
- active modifier gets an accent stripe and stronger text

### Main display

The main display is the content surface.

- no outer panel box
- top header line is allowed
- section separators are allowed
- progress bars and value meters are thin inline rules
- large preset/value text can sit directly on the background

### Right bar

The right bar is the action and page rail.

- rows: always 4
- columns: 0, 1, or 2
- cells are a grid of lanes separated by rules
- focus moves with a slide or underline animation

## Layout Model

UI V2 needs a real scaffold calculation step before rendering.

```cpp
enum class UiBarColumns : uint8_t {
  Zero = 0,
  One = 1,
  Two = 2,
};

struct UiScaffoldSpec {
  int screenWidth = 240;
  int screenHeight = 135;
  UiBarColumns leftColumns = UiBarColumns::One;
  UiBarColumns rightColumns = UiBarColumns::One;
  uint8_t rows = 4;

  int outerPaddingX = 4;
  int outerPaddingY = 4;
  int zoneGap = 6;
  int rowGap = 2;
  int columnGap = 4;
  int barCellWidth = 28;
};

struct UiScaffoldLayout {
  ui::Rect leftBar;
  ui::Rect main;
  ui::Rect rightBar;
  std::array<ui::Rect, 4> leftCells;
  std::array<ui::Rect, 8> rightCells;
};

UiScaffoldLayout ComputeUiScaffold(const UiScaffoldSpec& spec);
```

### Width calculation

```cpp
leftWidth = leftColumns == 0 ? 0 : leftColumns * barCellWidth + (leftColumns - 1) * columnGap;
rightWidth = rightColumns == 0 ? 0 : rightColumns * barCellWidth + (rightColumns - 1) * columnGap;
gapCount = (leftColumns != 0 ? 1 : 0) + (rightColumns != 0 ? 1 : 0);
mainWidth = screenWidth - 2 * outerPaddingX - leftWidth - rightWidth - gapCount * zoneGap;
```

### Example on 240x135

With `outerPaddingX=4`, `zoneGap=6`, `barCellWidth=28`, `columnGap=4`:

- left `1x4`, right `1x4` -> main width `164`
- left `0x4`, right `1x4` -> main width `198`
- left `1x4`, right `2x4` -> main width `132`

That is the behavior you asked for: the bars are configurable and the center surface is computed from them.

## New UI Layers

The renderer should be split into four layers.

### 1. Font layer

New files under `OS/UI`:

- `FontTypes.h`
- `GfxFontAdapter.h/.cpp`
- `FontCatalog.h/.cpp`

`Canvas` grows from fixed 5x7 text to style-based text:

```cpp
struct TextStyle {
  const FontFace* font = nullptr;
  ui::Color color = 0;
  int tracking = 0;
  TextAlign align = TextAlign::Left;
  VerticalAlign verticalAlign = VerticalAlign::Top;
};

class Canvas {
public:
  void DrawText(int x, int y, std::string_view text, const TextStyle& style);
  void DrawTextInRect(const Rect& rect, std::string_view text, const TextStyle& style);
  int MeasureTextWidth(std::string_view text, const TextStyle& style) const;
  int FontHeight(const TextStyle& style) const;
};
```

### 2. Scaffold layout layer

New files:

- `ScaffoldLayout.h/.cpp`
- `ScaffoldModel.h`

This layer only computes rectangles and separators. It knows nothing about synths.

### 3. Motion layer

New files:

- `UiAnimator.h/.cpp`
- `UiTransition.h`

This layer owns stateful transitions between old and new scene snapshots.

### 4. Scene presenter layer

New synth-side files:

- `Applications/Synth/UI/SynthUiSceneModel.h`
- `Applications/Synth/UI/SynthUiPresenter.h/.cpp`
- `Applications/Synth/UI/SynthUiRenderer.cpp` becomes a scaffold consumer instead of a direct painter

This layer maps synth state into a generic scene model.

## Scene Model

The renderer should consume a scene description rather than query `SynthUiModel` ad hoc while drawing.

```cpp
struct UiBarCellModel {
  std::string id;
  std::string label;
  bool active = false;
  bool visible = true;
};

struct UiBarModel {
  UiBarColumns columns = UiBarColumns::One;
  std::array<UiBarCellModel, 8> cells{};
};

enum class UiMainTemplate {
  HeroValue,
  ListDetail,
  ParameterList,
  ParameterFocus,
  StatusSummary,
};

struct UiMainModel {
  UiMainTemplate kind = UiMainTemplate::StatusSummary;
  std::string title;
  std::string subtitle;
  std::string hero;
  std::string footer;
};

struct UiSceneModel {
  UiScaffoldSpec scaffold;
  UiBarModel leftBar;
  UiBarModel rightBar;
  UiMainModel main;
};
```

This makes the scaffold stable while allowing different page presenters.

## Animation Model

Animations are required, but they should be cheap enough for embedded rendering.

Do not make alpha-heavy compositing the default. Use position, clipping, and accent motion first.

### Required transitions

1. Modifier change
   - left bar accent stripe slides to the next row
   - main content shifts 8 to 12 px and settles

2. Right bar focus change
   - active underline or edge marker slides to the new cell
   - label tint updates after motion begins

3. Scene change
   - main surface uses horizontal slide between pages
   - incoming page starts slightly offset and settles in

4. Value change
   - numeric text can tick or snap
   - meters animate to new length over 80 to 120 ms

### Timing

- focus motion: 80 ms to 100 ms
- scene transition: 120 ms to 160 ms
- large preset swap: 160 ms to 200 ms

### Implementation note

The animator should keep two scene snapshots: previous and current. The renderer samples an eased progress value and draws positional interpolation. That is hardware-safe and simulator-safe.

## Sound Section Design

`Sound` is shared across synths. The right bar is fixed and predictable.

### Right bar content for `Sound`

Rows:

1. `ENGINE`
2. `PRESET`
3. `VOLUME`
4. `FX`

Default right bar width: `1x4`

### Main display behavior

When `ENGINE` is focused:

- main title: current track and current engine
- main body: engine list or engine summary
- hero text: current engine display name

When `PRESET` is focused:

- main title: preset browser
- main body: paged preset list
- hero text: current preset

When `VOLUME` is focused:

- main title: track volume
- hero text: numeric volume
- thin horizontal meter under the value

When `FX` is focused:

- main title: FX chain or sends
- main body: slot list, send amounts, or active FX summary

## Parameter Section Design

`Parameter` must be synth-customizable, but the scaffold and navigation rules stay shared.

### Rule

The UI framework owns:

- left bar behavior
- right bar layout
- main surface templates
- animation rules
- text styles

Each synth engine can optionally own:

- parameter page list
- page labels
- page ordering
- main content template hints
- parameter grouping and summary strings

### New optional synth UI interface

Do not overload the renderer with synth-specific `if` chains.

Add an optional UI provider interface:

```cpp
struct SynthUiPageInfo {
  std::string id;
  std::string label;
  UiMainTemplate templateHint = UiMainTemplate::ParameterList;
};

struct SynthUiPageModel {
  std::string pageId;
  std::string title;
  UiMainTemplate templateKind = UiMainTemplate::ParameterList;
  std::vector<SynthParameterInfo> parameters;
  std::string selectedParameterId;
  std::string summaryValue;
};

class ISynthUiProvider {
public:
  virtual ~ISynthUiProvider() = default;
  virtual SynthParameterPage ListUiPages(const SynthPageRequest& request) const = 0;
  virtual bool BuildUiPage(const std::string& pageId,
                           const SynthPageRequest& request,
                           SynthUiPageModel& out) const = 0;
};
```

### Default behavior when a synth does not implement `ISynthUiProvider`

The presenter groups `ListParameters()` by `group` and synthesizes pages automatically.

That keeps the framework generic while allowing DX7 and Juno to become more expressive later.

### DX7 example

Right bar in `Parameter` mode can be `2x4`:

- `GLOBAL`
- `OP1`
- `OP2`
- `OP3`
- `OP4`
- `OP5`
- `OP6`
- `MOD`

Main display can use:

- top title: page name
- large selected parameter label
- current value in hero text
- thin list of surrounding parameters under it

### Juno example

Right bar in `Parameter` mode can be `1x4`:

- `DCO`
- `VCF`
- `ENV`
- `FX`

This still uses the same scaffold and animation rules.

## Proposed File Changes

### New common UI files

- `OS/UI/FontTypes.h`
- `OS/UI/GfxFontAdapter.h`
- `OS/UI/GfxFontAdapter.cpp`
- `OS/UI/FontCatalog.h`
- `OS/UI/FontCatalog.cpp`
- `OS/UI/ScaffoldLayout.h`
- `OS/UI/ScaffoldLayout.cpp`
- `OS/UI/UiAnimator.h`
- `OS/UI/UiAnimator.cpp`

### Files to refactor

- `OS/UI/Canvas.h`
- `OS/UI/Canvas.cpp`
- `Applications/Synth/UI/SynthUiRenderer.h`
- `Applications/Synth/UI/SynthUiRenderer.cpp`
- `Applications/Synth/UI/SynthUiModel.h`
- `Applications/Synth/UI/SynthUiModel.cpp`
- `Library/Synth/Core/ISynthEngine.h`

### Optional synth-side additions

- `Applications/Synth/UI/ISynthUiProvider.h`
- `Applications/Synth/UI/Dx7SynthUiProvider.h/.cpp`
- `Applications/Synth/UI/JunoSynthUiProvider.h/.cpp`

## Implementation Order

### Stage 1

Replace the text system.

- add `GFXfont` support to `Canvas`
- introduce a font catalog with 3 to 4 open fonts and fixed roles
- keep old renderer temporarily, but switch it to the new text API

### Stage 2

Introduce the scaffold layout engine.

- configurable `0x4`, `1x4`, `2x4` bars
- separator-only bars
- no more `DrawPanel()` and `DrawButton()` as the primary language

### Stage 3

Ship the new `Sound` page first.

- right rail: `ENGINE`, `PRESET`, `VOLUME`, `FX`
- main display driven by selection
- basic motion enabled

### Stage 4

Add synth-customizable `Parameter` pages.

- default grouping for all synths
- custom DX7 and Juno page definitions

### Stage 5

Remove the old block renderer entirely.

## Recommendation

The best next implementation move is:

1. adopt `GFXfont` as the text backend
2. build `ScaffoldLayout` and `UiAnimator`
3. rewrite `Sound` first on the new scaffold
4. then add `Parameter` page providers for DX7 and Juno

That gives the project a stable UI framework instead of another one-off renderer rewrite.
# Synth UI Functional Alignment

This document is the corrected design baseline for the synth UI.

Source of truth order:

1. `C:\Users\NengzhuoCai\Downloads\synth_ui_implementation_design_doc.md`
2. the target mockup set the user labeled as `图2`
3. current code only as an implementation reference, not as a behavior reference

The previous `OS/UI/UIFrameworkV2Design.md` document captured some useful renderer architecture ideas, but its persistent three-zone scaffold and menu semantics do not match the intended synth UI behavior.

## 1. Current Status Audit

### 1.1 Already satisfied

These parts already exist in the current codebase and should be kept:

- Shared framebuffer rendering exists through `Device::Display` and `OS/UI/Canvas`.
- UI has two high-level modes: `Home` and `ContextMenu`.
- Modifier input comes from the left matrix column `col 0`.
- Context menu input comes from the right matrix columns `col 12` and `col 13`.
- Releasing the active modifier returns the UI to `Home`.
- Bottom-right menu cell already acts as a generic `NextPage` trigger.
- Current screen already shows engine name, track, patch name, and a looper/progress concept.

### 1.2 Partially satisfied

These behaviors exist, but not in the correct final form:

- Context mode already renders a main content area plus a right-side `2x4` grid, but the layout is still block-heavy and uses filled panels.
- The active menu cell is highlighted, but the visual style is wrong for the intended design.
- Page switching exists, but the page content is generated from engine preset/parameter paging rather than from fixed modifier menu definitions.
- `PAR` can already enter parameter adjustment, but it does so through a generic browse-then-adjust flow instead of the spec's dedicated menu actions.

### 1.3 Not satisfied or currently wrong

These are the main reasons the current UI still looks like `图1` instead of `图2`:

- Home screen still renders a left modifier bar and a right action bar as large boxed blocks.
- Home screen still contains the wrong middle content structure: `LOOPER` label, `PERFORMANCE` label, and a boxed activity area instead of a line keyboard visualization and a thin quiet looper bar.
- Context screen still renders a left-side framed main panel and boxed right menu cells instead of the lighter line-and-separator look.
- `SND` currently pages through presets directly. The spec requires fixed menu pages like `OSC / FLT / ENV / FX / LFO / MOD / AMP / 1/3`.
- `PAR` currently browses full parameter lists from the engine. The spec requires fixed edit actions such as `PREV / NEXT / FINE / RST / MIN / MAX / LOCK / 1/2`.
- `LAY` and `SYS` menus do not match the functional design document's labels or page structure.
- The current UI model has a `300 ms` hold delay before entering modifier context. The spec implies immediate hold behavior, not a delayed popup.
- Text rendering is still limited to the built-in `5x7` bitmap alphabet in `Canvas.cpp`, so the typography cannot match the target.
- The main content presenter is still driven by generic engine paging semantics rather than a dedicated synth UI state model.

## 2. Corrected UI Direction

The intended UI is not a persistent dashboard with three permanent blocks.

It is a compact two-state instrument UI:

1. `Home Screen`
2. `Context Menu Screen`

The visual target is the cleaner mockup set the user attached as `图2`:

- lighter outlines
- bigger typography
- no heavy filled panel blocks
- simple keyboard line art
- right-side `2x4` menu only when editing context is shown

## 3. Corrected Screen Model

### 3.1 Home Screen

Home is for performance monitoring, not editing.

It shows only:

- current engine name
- current track number
- current patch name
- line-based mini keyboard visualization
- thin segmented looper bar

It should not show:

- preset lists
- parameter lists
- system diagnostics
- boxed performance widgets
- generic right-side context menu grid

Visual center of gravity:

- patch name is the largest element
- engine and track are small header elements
- keyboard visualization is a low-complexity line drawing
- looper bar is minimal and low emphasis

### 3.2 Context Menu Screen

Context screen is temporary and modifier-driven.

When a modifier is held:

- main area shows the current section label, parameter name, divider line, and large value
- right side shows the active `2x4` menu page
- bottom-right cell is the page indicator and page advance key
- releasing the modifier returns immediately to `Home`

The main content hierarchy is:

1. section label, cyan
2. parameter name, large white
3. divider line
4. current value, very large white

## 4. Corrected Menu Semantics

The current code generates menu pages from synth presets and parameter arrays. That is not the intended product behavior.

The correct model is:

- each modifier owns fixed menu pages
- right menu labels are stable and predictable
- synth state fills the main content, not the right grid itself

### 4.1 Sound menu

The `SND` modifier should use fixed pages like:

Page 1:

- `OSC`
- `FLT`
- `ENV`
- `FX`
- `LFO`
- `MOD`
- `AMP`
- `1/3`

Page 2 and Page 3 follow the labels from the user spec.

### 4.2 Parameter menu

`PAR` is not a parameter browser. It is an edit/control page.

Its content should be fixed actions like:

- `PREV`
- `NEXT`
- `FINE`
- `RST`
- `MIN`
- `MAX`
- `LOCK`
- `1/2`

### 4.3 Layout and System menus

`LAY` and `SYS` should also be fixed page definitions, not generated from engine lists.

## 5. Corrected Data Model

The UI should stop treating the right menu as a direct paged view over engine data.

Instead, the UI should operate on a synth-facing presentation state like:

```cpp
struct SynthUiState {
  std::string engineName;
  uint8_t trackNumber = 1;
  std::string patchName;

  std::string currentSection;
  std::string currentParameterName;
  std::string currentValueText;

  float looperProgress = 0.0f;
  KeyboardState keyboard;
};
```

And a menu definition layer like:

```cpp
enum class ModifierMenuId {
  Sound,
  Parameter,
  Layout,
  System,
};

enum class MenuCellType {
  Action,
  NextPage,
};

struct MenuCell {
  const char* label = "";
  MenuCellType type = MenuCellType::Action;
  MenuActionId action = MenuActionId::None;
};

struct MenuPage {
  MenuCell cells[4][2];
};
```

This separation is important:

- menu structure is fixed by product design
- main content is filled from synth state
- engines do not decide the right menu layout directly

## 5.1 String overflow behavior

The current renderer truncates long strings with ellipsis. That is not the intended final behavior for key visible UI text.

Correct behavior:

- patch names should scroll horizontally when they overflow the home title width
- parameter names should scroll horizontally when they overflow the context title width
- preset names and other important labels should prefer marquee-style scrolling over aggressive shrinking

Ellipsis can remain as a temporary fallback for secondary labels, but primary readable text should scroll.

## 6. Renderer Corrections

### 6.1 What to remove from the current renderer

Remove these as the dominant visual language:

- `DrawPanel()` as the main scaffold primitive
- `DrawButton()` as filled boxed cells everywhere
- framed `PERFORMANCE` box on home
- framed `LIVE PARAMETER` footer chip on context view

### 6.2 What to replace them with

Use:

- thin outlines
- separators
- typography hierarchy
- sparse cyan highlights
- line keyboard drawing
- thin segmented looper bar

### 6.3 Typography

`GFXfont` remains the right font data format choice for implementation.

But text hierarchy must match the spec:

- home patch name: largest
- context value: largest
- parameter name: medium-large
- right menu labels: medium
- header labels: small

## 7. Interaction Corrections

### 7.1 Modifier entry

The current `300 ms` hold threshold should be removed or reduced to a near-zero debounce-only behavior.

Desired behavior:

- user holds modifier
- UI enters corresponding context immediately
- user presses right-side menu key
- user releases modifier
- UI returns home immediately

### 7.2 Page behavior

For v1, each time a modifier is newly held:

- reset to page `0`
- reset active menu cell to `0,0`

This already matches the user's preferred predictability model.

### 7.3 Selection persistence

For v1:

- current synth parameter/value can persist internally
- visible context page does not need deep navigation history
- right menu should remain easy to predict and stateless

## 8. Revised Implementation Direction

The next implementation should not start from animation or from a generic scaffold engine.

It should start from product-correct behavior.

### Step 1

Refactor `SynthUiModel` so modifier menus are fixed data tables instead of engine paging views.

### Step 2

Refactor `SynthUiRenderer` so:

- `Home` matches the home spec
- `ContextMenu` matches the `main + 2x4` editing layout
- line keyboard and thin looper bar replace the current placeholder blocks

### Step 3

Switch text rendering to a proportional TFT font backend.

### Step 4

Only after the behavior and layout are correct, add animation polish.

## 9. Product Decision To Carry Forward

The important correction is this:

The synth UI is not a general dashboard framework with arbitrary left and right bars.

It is a performance-first `Home` screen plus a modifier-held temporary `Context Menu` screen, using a fixed right `2x4` menu grammar and a simple large-type main content area.

That is the model future implementation should follow.
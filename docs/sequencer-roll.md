# ROLL in the sequencer

One step is a sixteenth note. ROLL retriggers that step's note *inside* the step, so a
drum hit can be a buzz instead of a single hit:

| ROLL | hits inside one 16th | pulses apart (96 PPQ) | reads as |
|---|---|---|---|
| off (1) | 1 | — | one 16th |
| ×2 | 2 | 12 | two 32nds |
| ×3 | 3 | 8 | three 48ths (16th triplets) |
| ×4 | 4 | 6 | four 64ths |

24 pulses per step divides by 2, 3 and 4 exactly, so every sub-hit lands on a whole pulse
and the clock never has to be interpolated. `kSequencerPulsesPerStep % kSequencerMaxRoll == 0`
is asserted in `Library/Synth/Core/SynthTypes.h`.

## Playing it

`SynthSequencer::FireStepNotesLocked` already decided whether a note is due by comparing its
offset in the pattern (`NoteOffsetPulsesLocked`, which folds in the microstep and the swing
delay) with the pulse being processed. ROLL just turns that single comparison into the set of
offsets the note fires on:

```cpp
firstOffset      = NoteOffsetPulsesLocked(stepIndex, note.microstep);
subHitPulses     = kSequencerPulsesPerStep / roll;      // 12, 8 or 6
hit k fires when firstOffset + k * subHitPulses == targetOffset
```

A rolled note's gate is one sub-step (`subHitPulses`); an unrolled note keeps the length it was
given. Microstep and swing still apply to the whole group, so a rolled note sits inside the same
swung step as before, and a roll at microstep 5 spills into the next step exactly like a long
note does today.

## Storing it

`NoteEvent` was six bytes and `StepData` (eight events) forty-eight, so a `StepStore` page is 768
bytes for sixteen steps. A seventh byte would have broken that. The spare bits of the last byte
carry the count instead:

```cpp
struct NoteEvent {
  std::uint8_t note, velocity;
  std::uint16_t lengthPulses;
  std::uint8_t microstep;
  std::uint8_t active : 1;
  std::uint8_t roll : 3;      // 1..4, 0 reads as 1
  std::uint8_t Roll() const;  // clamped read
  void SetRoll(std::uint8_t);
};
static_assert(sizeof(NoteEvent) == 6);
static_assert(sizeof(StepData) == 48);
```

Three bits, not two: two bits hold 0..3, so a count of four silently truncated to zero and played
as a plain note. The bitfield has to cover the value it names.

`roll == 0` reads as 1, so every `NoteEvent{}` reset and every file written before version 8
plays exactly as it did.

## Editing it

Three ways, all funnelling into the same place:

- **Hold a PAD and tap a step.** The first tap writes the hit. Taps that follow — same step, pad
  still down — walk the count on: ×2, ×3, ×4 and round to off again.
- **Lift the pad, put it down again, tap that step.** The hit comes out; the next tap writes it
  back with **the velocity it had**, not the velocity of the pad row you happen to be on, and the
  taps walk the count on from there. Velocity never moves under the gesture.
- `FN` + the step is the plain toggle it always was.
- **The edit strip.** `ENTER` cycles `NOTE → LEN → VEL → TIME → ROLL`; `ADD` / `SUB` walks the
  count 1..4 with wrap-around (`CycleEditLane`, `AdjustSelectedSteps`).
- **The API.** `SetStepRoll(step, roll, pitch = -1)`, `CycleStepRoll(step, pitch = -1)`,
  `StepRoll(step)`, `DefaultRoll()` / `CycleDefaultRoll()`.

The gesture is remembered per pad hold (`Controller::RollTap`): `ReleasePad` and `ReleaseAllPads`
clear it, which is what makes the first tap of a new hold take the hit out rather than count on.

Whichever way, the last count chosen becomes the track's **brush** (`TrackData::defaultRoll`), so
the next note written on that track starts with it — the same way `defaultVelocity` and
`defaultLengthPulses` work. Selecting an empty step and editing ROLL edits the brush alone
(`NEW ROLL X3` in the footer).

## Showing it

`DrawRollNotches` cuts a note's bar into as many pieces as it retriggers on, so a ×4 sixteenth
reads at a glance as four 64ths. It runs in the piano roll and in the step machine (where a step is
only four pixels wide, so the cuts read as density rather than as four bars). The step cell carries
the highest count on the step (`SequencerStepUiState::roll`), and the edit strip shows `ROLL X3`.

The big GRID square is left alone for this: a hit fills it whole, and the count is a **2 × 2 corner
of dots** (`DrawRollDots`) filled clockwise — two for ×2, the top row plus the right one of the
second row for ×3, all four for ×4, nothing when it is off. The square answers "is there a hit",
the dots answer "how many times", and neither costs the other its shape.

Nothing about the gesture raises a pop-up: the step is under the fingers and the square already
says what happened.

### Velocity is height, not density

Until ROLL existed a bar said how hard it was hit with a dither: solid for 127, a checker for 100,
a quarter for 70, an eighth for 40. Cuts through a dither are unreadable — a quiet rolled note turns
into scattered dots — so velocity became height instead:

```cpp
Rect VelocityBar(const Rect& space, std::uint8_t velocity)  // bottom-aligned, 3 px minimum
```

A bar is as tall as it is loud, standing on the bottom of the space its row gives it, and never
shorter than three pixels: a hit too quiet to see is a hit you cannot edit. The rule is used by the
two places that show a hit as a bar — the piano roll and the step machine — so they speak one
language for "how hard". The GRID square keeps a full body (it is the one tile big enough to carry
the ROLL dots) and shows no velocity at all.

A selected step keeps the inverse fill it always had, but it is the bar that turns over, not the
cell: at full velocity the whole square inverts (there the height has nothing left to say), and
below it only the bar does — so the selection stays obvious and `ADD` / `SUB` on VEL still moves
something on screen.

Where a piano roll row is only three pixels tall (a wide pitch range on the 240×135 screen) the
height still varies, but only between two and three pixels: that is the screen's limit, not the
rule's.

## Project format

Version 8 adds a seventh number to every note line:

```
CARDENZA_PROJECT 8
...
NOTES 3
0 12 60 100 24 0 2      <- pattern, step, note, velocity, length, microstep, ROLL
```

Version 7 and older files load with every note rolled off, and a version 8 file fails cleanly on
an older build instead of loading with wrong timing.

## Checking it

`tests/roll_test.cpp` (target `CardenzaRollTests`) steps the clock by hand through the sequencer's
test hooks and checks the note-ons the sink receives: one hit for off, two twelve pulses apart for
×2, three eight apart for ×3, four six apart for ×4, each with a one-sub-step gate; plus the
editing API, the chord scope, the snapshot round trip and the edit-lane cycle. It runs in `ctest`
with the rest of the host tests.

To see it, build the browser simulator and put ×4 on a drum step: the GRID square splits into four
strips and the footer reads `ROLL X4`.

```sh
make web && make web-serve      # https://github.com/203-Systems/CardenzaSynth#构建
```

## Not done

- ROLL is per note, not per step: a chord can roll its notes independently. There is no
  "roll the whole track" setting.
- No velocity ramp inside a roll, and no "ratchet the last hit longer" option. Both would need
  another field; the two spare bits of `NoteEvent` are the only room left in the step page.
- The old UI (`-DCARDENZA_GROOVE_UI=OFF`) builds, but its lane strip still shows four lanes; only
  the Groove UI was extended.

# Getting started {#start}

Cardenza Groovebox is a pocket music workstation: four tracks (drums, a Juno synth, a DX7 FM synth and a sampler), each with its own sequencer patterns, plus arrangement, mixing, effects and project files.

The keyboard is split into four blocks, and **colour means role**: every key in this manual is drawn in its own colour, the same colours the device uses on its screen.

{{keymap:Four blocks: navigation on the left, playing in the middle, the GRID on the right matches the screen's cells, values and transport on the far right
ESC TAB FN CTRL = Left four keys: where to go
PADS = Pads: play the current track
GRID = GRID: press the cells on screen
RIGHT = Right four keys: values · play
}}

| Colour | Key | Looks after |
|---|---|---|
| {{key:ESC}} red-orange | scenes | 1 INSTR / 2 SEQ / 3 TAPE |
| {{key:TAB}} cyan | tracks | {{track:1}} {{track:2}} {{track:3}} {{track:4}} |
| {{key:FN}} yellow | second functions | FN panel, shortcuts |
| {{key:CTRL}} violet | pages | the tabs at the bottom of the screen, like {{page:ROLL}} |
| {{key:PLAY}} green | transport | play / stop |

How to use this manual: {{icon:map}} the **Map** (top right) jumps straight to any key or page you click; {{icon:search}} search for any word at the top ("音量", "volume" and "too quiet" all find the same place); {{icon:play}} under every screenshot, **Try it live here** swaps in a running device; {{icon:usb}} connect your Cardenza over USB and play the manual's device with its real keys (see [[#web-live]], [[#web-usb]]).

<!-- keywords: overview, introduction, about, what is, beginner, colours, 介绍, 概览 -->

## One-minute start {#quick-start}

Follow these steps once and you know most of it:

{{strip
- > seq-empty @scene @track | After power-on you are in {{scene:SEQ}}, on track {{track:2}}: the pads are a piano, white keys on the bottom row, black keys above, the gaps (Shift, D, J) dark; keys you are not pressing are half lit.
hold r3c1~C + G1 > seq-hold-tap @27,52,14,10 | Hold a note and tap a step: the note is written. The held key is fully lit.
hold TAB + G1 > seq-drum-held @track | Hold {{key:TAB}} and tap GRID 1: you are on {{track:1}}, the drums, one drum per column.
hold r3c1~Kick + G1, G5, G9, G13 > seq-drums @26,119,198,9 | Hold the kick key and tap GRID 1, 5, 9 and 13: a kick on all four beats is written.
PLAY > seq-drums-playing | Play! Each step sounds when the green line reaches it. Press again to stop.
hold OK > record | Whatever you just played is not lost: hold {{key:OK}} to put it into a pattern.
}}

Three more things to remember:

{{strip
ESC > instr-voice @scene | Tap {{key:ESC}} to walk the three scenes: {{scene:INSTR}} pick sounds → {{scene:SEQ}} write notes → {{scene:TAPE}} arrange and mix.
hold FN + 7 > fn-volume @14,102,212,26 | Hold {{key:FN}}: {{key:6}} {{key:Y}} set the BPM, {{key:7}} {{key:U}} the volume, {{key:8}} {{key:I}} the octave.
FN+S > save-done @0,123,120,12 | {{key:FN+S}} saves the project; the first time it asks you for a name.
}}

<!-- keywords: quick start, tutorial, first steps, how to begin, getting started, 快速开始, 上手 -->

## What is on the screen {#screen-layout}

The screen is 240×135 pixels in five areas, each placed where its keys are:

{{shot:seq-notes|The SEQ note page (ROLL) with a few notes on T2. Below it the key lights of the same moment.
mark scene = Scene
mark track = Track (in its colour)
mark bpm = BPM · play state
mark strip:ESC = ESC
mark strip:TAB = TAB
mark strip:FN = FN
mark strip:CTRL = CTRL
mark content = Content = GRID
mark strip:PLAY = PLAY
mark footer = Page tabs: CTRL changes page
key PADS = Can sound: half lit
key G1 = Has a note: half lit
key G2 = Empty step: off
}}

| Area | Where | Shows |
|---|---|---|
| Status line | top | scene, current track (in its colour), engine and sound, BPM, play / record |
| Four left strips | left edge | the lights of the {{key:ESC}} {{key:TAB}} {{key:FN}} {{key:CTRL}} keys; the small marks = scenes, tracks, recording, pages |
| Content | middle | a 4×4 of cells = the 4×4 GRID on the right: the key you press is that cell |
| Page tabs | bottom | the pages of the current scene, the current one on violet {{page:ROLL}}; {{key:CTRL}} changes page |
| Four right strips | right edge | the lights of {{key:ADD}} {{key:SUB}} {{key:OK}} {{key:PLAY}} |

> [!tip] The cell in row 2, column 3 on screen is GRID row 2, key 3 (the `[` cap). Every page follows this rule: **wherever a cell is on the screen, its key is in the same place.**

The edge strips follow the key lights: when a light is {{led:off|off}} its strip is faint, {{led:half|half lit}} a 30% dither, {{led:full|fully lit}} a solid block. See [[#lights-edges]].

<!-- keywords: screen, display, layout, status line, header, page tabs, tabs, footer, content area, 屏幕, 界面 -->

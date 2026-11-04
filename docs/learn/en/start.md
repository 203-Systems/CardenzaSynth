# Getting started {#start}

> [!tip] New to it? Take the [guide](./) first: a few minutes a lesson, one step at a time. This manual is the reference, for looking things up.

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
| {{key:ESC}} red-orange | scenes | 1 INSTR / 2 SEQ / 3 TAPE, and 0 SETUP |
| {{key:TAB}} cyan | tracks | {{track:1}} {{track:2}} {{track:3}} {{track:4}} |
| {{key:FN}} yellow | second functions | the menu of this screen, shortcuts |
| {{key:CTRL}} violet | pages | the tabs at the bottom of the screen, like {{page:ROLL}} |
| {{key:PLAY}} green | transport | play / stop |

How to use this manual: {{icon:map}} the **Map** (top right) jumps straight to any key or page you click; {{icon:search}} search for any word at the top ("音量", "volume" and "too quiet" all find the same place); {{icon:play}} under every screenshot, **Try it live here** swaps in a running device; {{icon:usb}} connect your Cardenza over USB and play the manual's device with its real keys (see [[#web-live]], [[#web-usb]]).

<!-- keywords: overview, introduction, about, what is, beginner, colours, 介绍, 概览 -->

## One-minute start {#quick-start}

Follow these steps once and you know most of it:

{{strip
- > instr-boot @8,0,41,11 @51,0,15,11 @10,89,220,38 | After power-on you are in {{scene:INSTR}} on the VOICE page, on track {{track:1}}, the drums (808 KIT): each column of pads is one drum, the top row hits hardest.
ESC > seq-empty @scene | Tap {{key:ESC}}: {{scene:SEQ}}, where the track's notes are written. Still {{track:1}}: on a drum track this is the drum page, the drums on the left, the steps of the chosen drum on the GRID.
hold r3c1~Kick + G1, G5, G9, G13 > seq-drums @9,17,92,10 | Hold the kick key and tap GRID 1, 5, 9 and 13: a kick on all four beats is written. Snare and hats go in the same way.
PLAY > seq-drums-playing | Play! Each step sounds when the green line reaches it. Press again to stop.
TAB > seq-keyboard @track | Tap {{key:TAB}}: {{track:2}}, the Juno. Now the pads are a piano: two whole octaves, rows 2 and 4 white keys, rows 1 and 3 black keys, the gaps dark; C3 at the bottom left.
hold r3c1~C3 + G1 > seq-hold-tap @28,57,15,10 | Hold a note and tap a step: the note is written. The held key is fully lit.
hold OK > record | Whatever you just played is not lost: hold {{key:OK}} to put it into a pattern.
}}

Four more things to remember:

{{strip
hold ESC > map-esc @cells:1,1-4,4 | Tap {{key:ESC}} to walk the three music scenes: {{scene:INSTR}} sounds → {{scene:SEQ}} notes → {{scene:TAPE}} arrange and mix. Hold it for this map: column 1 is {{scene:SETUP}}, the project and the settings.
hold FN > fn-held @cells:1,1-4,4 | Hold {{key:FN}}: the menu of this screen on the GRID (copy, paste, save, record…); FN + a GRID key runs an item.
hold FN + 7 > fn-volume @14,94,212,28 | With FN held: {{key:6}} {{key:Y}} set the BPM, {{key:7}} {{key:U}} the volume, {{key:8}} {{key:I}} the octave.
FN+S > save-done @footer | {{key:FN+S}} saves the project; an untitled one gets the first free name, SONG 01, without asking.
}}

<!-- keywords: quick start, tutorial, first steps, how to begin, getting started, first beat, power on, 快速开始, 上手 -->

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
mark content = Content: its 16 steps = the GRID
mark strip:PLAY = PLAY
mark footer = Page tabs: CTRL changes page
key PADS = Can sound: half lit
key G1 = Has a note: half lit
key G2 = Empty step: off
}}

| Area | Where | Shows |
|---|---|---|
| Status line | top | scene number and name, current track (in its colour), engine and sound, REC, play / stop, BPM, `*` unsaved changes |
| Four left strips | left edge | the {{key:ESC}} {{key:TAB}} {{key:FN}} {{key:CTRL}} keys with their icons (back, tracks, menu, pages); a held key fills its strip |
| Content | middle | a 4×4 of cells = the 4×4 GRID on the right: the key you press is that cell |
| Page tabs | bottom | the pages of the current scene, the current one on violet {{page:ROLL}}; {{key:CTRL}} changes page |
| Four right strips | right edge | {{key:ADD}} {{key:SUB}} {{key:OK}} {{key:PLAY}}; the ADD / SUB icons say what they do here (+ −, ↑ ↓ in a list, → ← sideways), PLAY is green while playing, red with recording armed |

> [!tip] The cell in row 2, column 3 on screen is GRID row 2, key 3 (the `[` cap). Every page follows this rule: **wherever a cell is on the screen, its key is in the same place.**

The edge strips show the state of their keys: {{led:off|off}} faint, {{led:half|half lit}} a 30% dither, {{led:full|fully lit}} a solid block. The eight edge keys themselves glow at most {{led:half|half lit}}, so the strip is where you read them. See [[#lights-edges]].

<!-- keywords: screen, display, layout, status line, header, page tabs, tabs, footer, content area, unsaved, asterisk, icons, 屏幕, 界面 -->

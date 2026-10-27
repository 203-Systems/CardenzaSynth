# FN panel and shortcuts {#fn}

{{key:FN}} is the second-function key: **hold** it and the screen becomes the FN panel, with the keys that have an FN function lit; **tap** it and the same panel stays open (SETUP).

<!-- keywords: fn, function, second function, settings, setup, shift -->

## The FN panel {#fn-panel}

{{shot:fn-held|Holding FN: the FN panel (on 1 KEYS here). Only the keys with an FN function are lit.
mark 10,0,100,11 = Page tabs: FN + 1…5 to switch
mark cells:1,1-2,4 = This page's settings: pick with FN + GRID
mark footer = Shortcut hints
mark strip:FN = FN, held
key FN = Hold
key 1 2 3 4 5 = Change page
key 6 7 8 Y U I = Quick controls
key G1-G8 = Settings
key ADD SUB = Change value
}}

| To | {{hold:FN}} While holding FN | {{icon:tap}} With SETUP open |
|---|---|---|
| change page | {{key:FN+1}} … {{key:FN+5}}, or {{key:FN+CTRL}} | the same, or tap {{key:CTRL}} |
| pick a setting | {{key:FN}} + a GRID key | a GRID key |
| change it | {{key:FN+ADD}} / {{key:FN+SUB}} (repeats) | {{key:ADD}} / {{key:SUB}} |
| step an option | the same GRID key again | the same, or {{key:OK}} |
| restore the default | – | {{key:FN+OK}} |

- **Hold {{key:FN}}**: the FN panel shows until you let go.
- **Tap {{key:FN}}**: opens SETUP, the FN panel kept open; tap FN again or press ESC to close. While SETUP is open the pads keep playing, so you hear every change at once.
- The panel remembers its last page and the setting picked on each page.

<!-- keywords: fn panel, setup, settings panel, panel, settings page -->

## The five pages {#fn-pages}

{{strip
FN, FN+1 > setup-keys @footer | **1 KEYS**: layout, scale, root, octave, velocity, scale lock.
FN+2 > setup-chord @footer | **2 CHORD**: chord, voicing.
FN+3 > setup-arp @footer | **3 ARP**: mode, rate, range, GATE, LATCH.
FN+4 > setup-project @footer | **4 PROJECT**: BPM, swing, LOOP / SONG, steps, record arm.
FN+5 > setup-system @footer | **5 SYSTEM**: master volume, key lights, screen brightness; device status.
}}

| Page | Stored |
|---|---|
| 1 KEYS · 2 CHORD · 3 ARP | app settings (restored at power-on, kept when you change project) |
| 4 PROJECT | BPM, swing and steps belong to the project |
| 5 SYSTEM | device settings (shared by all apps) |

Every setting of every page with its range and default (generated from the firmware code):

{{ref:fn}}

> [!tip] With KEY LED focused on 5 SYSTEM in SETUP, the pads show every brightness the key lights can give, from left to right: handy to judge the brightness setting and whether the lights themselves work (see [[#lights-ramp]]).

<!-- keywords: keys, chord, arp, project, system, master volume, brightness, key lights, screen brightness, bpm, swing, settings list -->

## Quick controls: BPM, volume, octave {#fn-quick}

The three most used settings need no page. Hold {{key:FN}}: the 2×3 block at the right end of the pads, next to the GRID, is three plus/minus pairs, **upper row up, lower row down** (like ADD above SUB):

{{keymap:Hold FN: 6 7 8 up, Y U I down
FN = Hold
6 Y = BPM + / −
7 U = Volume + / −
8 I = Octave + / −
}}

{{strip
hold FN + 6 > fn-quick @14,102,212,26 | BPM +1 ({{key:Y}} is −1). Hold to keep changing.
hold FN + 7 > fn-volume @14,102,212,26 | Volume: too quiet, press {{key:7}} (louder); too loud, press {{key:U}}.
hold FN + 8 > fn-octave @14,102,212,26 | Octave: {{key:8}} up, {{key:I}} down.
}}

{{ref:fn-quick}}

They work on any FN page, repeat while held, and pop the new value up at the bottom of the screen. While FN is held these six keys are {{led:half|half lit}}, {{led:full|fully lit}} while pressed.

<!-- keywords: volume, bpm, octave, quick, shortcut, tempo, faster, slower, volume, louder, quieter, octave, turn up, turn down, 快捷, 音量 -->

## Second functions (letters and the keys around) {#fn-shortcuts}

{{keymap:Keys with a second function while FN is held
FN = Hold
ESC = Stop all
TAB = Previous track
CTRL = Next page
C V = Copy, paste
X = Clear
Q = Quantize
D = Duplicate pattern
T = Tap tempo
S = Save
OK = Second function
PLAY = Record
}}

| FN + | Does |
|---|---|
| {{key:1}}–{{key:5}} | FN panel page |
| GRID | pick an FN panel setting |
| {{key:ADD}} / {{key:SUB}} | change the picked setting (repeats) |
| {{key:CTRL}} | next FN panel page |
| {{key:ESC}} | stop all (like HOME) |
| {{key:TAB}} | previous track |
| {{key:OK}} | instrument / sends / master FX: the focused parameter back to its default; SEQ: which chord tone to edit (all / one); SESSION: capture what plays as a new SONG row; SONG: SONG / LOOP; RECORD panel: merge a take into the current pattern; sample list: rename / delete |
| {{key:PLAY}} | live recording on / off (REC) |
| {{key:C}} / {{key:V}} | copy / paste (selected steps or the current pattern; a whole row in SONG) |
| {{key:X}} | clear (selected steps, or the current pattern after confirming; deletes the row in SONG) |
| {{key:Q}} | quantize (selected steps or the current pattern) |
| {{key:D}} | duplicate the current pattern into the next free slot; duplicate the row in SONG |
| {{key:T}} | tap tempo: tap two or more times, the interval sets the BPM |
| {{key:S}} | save the project (asks for a name first if it has none) |

> [!note] With SETUP open the letter shortcuts do nothing (the panel is for settings). Each letter function shows its name and result at the bottom of the screen.

<!-- keywords: shortcut, shortcuts, hotkeys, copy, paste, clear, delete, quantize, duplicate, tap tempo, save, 快捷键 -->

## FN in input screens: the yellow legends {#fn-yellow}

In text entry, the preset and sample lists and confirmations, holding FN shows no panel; it uses the **yellow legends** printed on the key caps instead:

{{keymap:Hold FN: the four keys at the lower right of the GRID are arrows
FN = Hold
; = Up
. = Down
, = Left
/ = Right
ADD SUB = Page up / down
}}

<!-- keywords: arrows, arrow keys, cursor keys, up down left right, yellow legend, list, 方向键 -->

## Typing text {#fn-text}

When naming something (a project, a sound, a sample) the whole keyboard types its caps:

{{shot:text-entry|Typing a name: SAVE AS.
mark 0,13,240,10 = What you are naming
mark 8,40,222,30 = Name and cursor
mark 222,13,16,9 = Characters / 24
mark footer = OK accept · DEL erase · ESC cancel
key ADD = Erase (del)
key OK = Accept
key ESC = Cancel
key PLAY = Space
}}

- Letters, digits, space (the PLAY key) and `-`; up to 24 characters.
- The top-right key (the del cap) erases before the cursor; {{key:OK}} accepts; {{key:ESC}} cancels.
- {{key:FN}} + {{key:,}} / {{key:/}} move the cursor, {{key:FN}} + {{key:;}} / {{key:.}} jump to the start / end, {{key:FN}} + del deletes after the cursor.
- The cursor is a bar between characters, a block at the end.

<!-- keywords: type, typing, name, rename, text entry, cursor, backspace, delete character, 打字, 起名 -->

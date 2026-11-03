# Quick reference {#reference}

<!-- keywords: reference, cheat sheet, summary, overview, 速查 -->

{{keymap:All keys at a glance
ESC = Scenes · back
TAB = Tracks
FN = FN menu · second functions
CTRL = Pages
PADS = Play the current track
GRID = Screen cells
ADD SUB = Increase / decrease
OK = Confirm · record
PLAY = Play / stop
}}

## Every key at a glance {#ref-shortcuts}

**Edge keys**

| Keys | Does |
|---|---|
| {{key:ESC}} tap | walk the scenes 1 INSTR → 2 SEQ → 3 TAPE (in SETUP: back to where you came from; in a pop-up: cancel; in SEQ with steps selected: clear the selection) |
| {{hold:ESC}} + {{key:GRID}} | scene map: the columns are 0 SETUP, 1 INSTR, 2 SEQ, 3 TAPE; column 1 is the way into SETUP |
| {{key:TAB}} tap | next track |
| {{hold:TAB}} + {{key:GRID}} | track map, laid out like MIX: rows select / mute / solo / sequencer on-off, one column per track |
| {{key:TAB}} + {{key:ADD}} / {{key:SUB}} | next / previous track |
| {{key:CTRL}} tap | next page |
| {{hold:CTRL}} + {{key:GRID}} | page map: every page of the scene, press one to go there |
| {{key:CTRL}} + {{key:ADD}} / {{key:SUB}} | next / previous page |
| {{key:FN}} tap | open / close the menu of this screen (it stays open; a GRID key runs an item and closes it) |
| {{hold:FN}} | the same menu while held; the letters and edge keys below are its second functions |
| {{key:ADD}} {{key:SUB}} | increase / decrease, move in a list; held they repeat and speed up |
| {{key:OK}} | confirm, open, switch; on the SEQ note page a tap switches ROLL / STEP, a hold opens RECORD, with steps selected it walks NOTE → LEN → VEL → TIME |
| {{key:PLAY}} | play / stop |
| HOME side key | not used by the groovebox (stop all is {{key:FN+ESC}}) |
| hold a parameter cell on the GRID | the pads become its values: press one to set it |

**FN + a key**

| Keys | Does |
|---|---|
| {{key:FN+GRID}} | run that item of the menu |
| {{key:FN}} + {{key:6}} / {{key:Y}} | BPM + / − (any screen, held it repeats) |
| {{key:FN}} + {{key:7}} / {{key:U}} | master volume + / − |
| {{key:FN}} + {{key:8}} / {{key:I}} | octave + / − |
| {{key:FN+ESC}} | stop all: the sequencer, every ARP and every sounding note |
| {{key:FN+TAB}} | previous track |
| {{key:FN+CTRL}} | previous page |
| {{key:FN+ADD}} / {{key:FN+SUB}} | big steps (BPM ±10, settings ×5); on the SEQ note page: next / previous page of a long pattern; in lists: a page at a time |
| {{key:FN+OK}} | the screen's second OK: RESET (default value), SCOPE (SEQ), ADD TO SONG (SESSION), SONG / LOOP (SONG); in the sample list: rename / delete |
| {{key:FN+PLAY}} | record on / off (REC) |
| {{key:FN+C}} {{key:FN+V}} | copy / paste (steps, pattern or song row) |
| {{key:FN+X}} | clear (a whole pattern asks first) / delete a song row |
| {{key:FN+Q}} | quantize |
| {{key:FN+D}} | duplicate the pattern / the song row |
| {{key:FN+T}} | tap tempo |
| {{key:FN+S}} | save the project (a new one gets the name SONG 01, SONG 02 …) |

**FN in input screens** (lists, text fields, confirmations)

| Keys | Does |
|---|---|
| {{key:FN}} + {{key:;}} {{key:.}} {{key:,}} {{key:/}} | up / down / left / right (in a text field: cursor to the start / end, left / right) |
| {{key:FN+F}} | search in the preset / sample list |
| {{key:FN+ADD}} / {{key:FN+SUB}} | a page at a time |
| {{key:ADD~del}} / {{key:FN}} + {{key:ADD~del}} | in a text field (the top-right key): delete before / after the cursor |
| {{key:OK}} / {{key:ESC}} | yes / no, keep / cancel |

<!-- keywords: shortcut list, all shortcuts, key list, cheat sheet, hotkeys, fn letters, hold map, 快捷键表 -->

## Every page {#ref-pages}

{{ref:pages}}

<!-- keywords: all pages, page list, 页面列表 -->

## Option lists {#ref-lists}

{{ref:lists}}

<!-- keywords: scale list, chord list, options, arp modes, 选项列表 -->

## Glossary {#glossary}

| Term | Meaning |
|---|---|
| Track | one of four, each with one engine, its own patterns, chord and ARP; you start on T2 |
| Engine | the synth that makes the sound: DRUMS, JUNO, DX7, SAMPLER |
| Preset / sound | a set of an engine's parameters |
| Step | the sequencer's smallest cell, a 1/16 note |
| Microstep | 6 subdivisions of a step, to record timing that is not exactly on the grid |
| Pattern | a sequence of 1 to 256 steps, shown 16 at a time; up to 16 per track |
| Song row | a part of the SONG: which pattern each track plays, for how many bars (a column on the SONG page, ROW n in the FN menu) |
| Scene | one of the four parts of the UI: 0 SETUP, 1 INSTR, 2 SEQ, 3 TAPE |
| SETUP | scene 0: project, files and the settings for every track; hold ESC + GRID column 1 |
| FN menu | what the current screen can do, on the GRID while FN is held (or kept open with a tap) |
| Take | a phrase you played, kept by capture |
| Autosave | the working state, kept in the device's own memory, so the power can go at any time |
| GRID | the 4×4 on the right, matching the 4×4 in the middle of the screen |
| PADS | the 4×8 in the middle, always playing the current track |
| Send | how much goes to the master delay / reverb |
| Master bus | the effects after the four tracks are mixed |

<!-- keywords: glossary, terms, definitions, meaning, vocabulary, 名词 -->

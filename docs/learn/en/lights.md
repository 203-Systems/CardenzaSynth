# Key lights {#lights}

Every key has a single-colour light that tells you what the key can do right now. There are only three states, plus blinking:

| | | |
|---|---|---|
| {{led:off}} | **off** | nothing there |
| {{led:half}} | **half lit** | something there, not chosen |
| {{led:full}} | **fully lit** | chosen, or happening |
| {{led:blink}} | **blinking** | waiting |

<!-- keywords: lights, key lights, led, leds, backlight, 灯 -->

## Off, half lit, fully lit, blinking {#lights-states}

{{shot:seq-selected|SEQ note page with step 5 selected: all four states in one picture.
mark 75,15,14,104 = Selected step
key G2 G4 = Empty step: off
key G1 G3 = Has notes: half lit
key G5 = Selected: fully lit
key r3c1 = This step's note: fully lit
key PADS = Can sound: half lit
}}

| State | Means | Examples |
|---|---|---|
| {{led:off|off}} | nothing there | an empty step, a key that cannot sound (a piano gap, a locked note outside the scale), a function this screen does not have, a send that is off |
| {{led:half|half lit}} | something there, not chosen | a step with notes, a key that sounds, an option, a parameter, an action you can run, an existing pattern |
| {{led:full|fully lit}} | chosen, or happening | the selected step, the focused parameter, the current page / scene / track / pattern, a held or sounding key, the playhead, a switch that is on (mute, solo, sequencer on) |
| {{led:blink|blinking}} | waiting | a queued pattern, a cued song row, OK in a confirmation, PLAY while recording is armed, the STEP view's write cursor |

{{strip
- > seq-step @cell:1,4 | {{led:blink}} STEP view: the write cursor blinks.
- > tape-session-queued @cell:2,2 | {{led:blink}} A queued pattern: waits for the loop to finish.
- > seq-recording @strip:PLAY | {{led:blink}} Record armed: PLAY blinks.
}}

> [!note] Each key has 16 real brightness levels (driven by the device's hardware layer, CardenzaHAL). The eye does not see brightness linearly, so the firmware gamma-corrects every level first; the KEY LED setting dims them all together. The interface uses three: "half lit" is the dimmest glow a key has (1/15 of the time), "fully lit" is 100 %. No in-between levels, and no animation ever lights a key that should be off.

<!-- keywords: light meaning, half lit, fully lit, blinking, dim, bright, what do the lights mean, 灯的意思 -->

## Lights on each screen {#lights-screens}

{{strip
- > seq-notes | **SEQ note page**: empty steps off, steps with notes half lit, selected ones fully lit, the playhead runs across fully lit.
- > seq-pattern | **PATTERN**: existing patterns half lit, the one in use fully lit, a queued one blinking.
- > instr-juno-vcf @cell:1,1 | **INSTR parameter pages**: parameters half lit, the focused parameter fully lit, empty cells off.
- > tape-session | **SESSION**: each track's current pattern fully lit, the others half lit, queued ones blinking.
- > tape-mix | **MIX**: the selected track fully lit; mute / solo / sequencer fully lit when on, half lit when off.
- > map-esc | **Holding ESC / TAB / CTRL**: the current item fully lit, the others half lit; the pads all off.
- > fn-held | **Holding FN**: the menu items on the GRID, the letters of the items and the quick controls half lit; everything else off.
- > setup-keys @cell:1,1 | **SETUP settings pages**: settings half lit, the focused one fully lit.
}}

| Screen | GRID lights |
|---|---|
| SEQ note page | empty steps off, steps with notes half lit, selected ones fully lit, the playhead runs across fully lit; in the STEP view the write cursor blinks |
| SEQ PATTERN | existing patterns half lit, the one in use fully lit, a queued one blinks |
| INSTR parameter pages | parameters and actions half lit, the focused parameter fully lit, empty cells off |
| TAPE SESSION | each track's current pattern fully lit, the others half lit, queued ones blink |
| TAPE SONG | cells with a pattern half lit, `--` off, the selected cell and the playing row fully lit |
| TAPE MIX | the selected track fully lit; mute / solo / sequencer fully lit when on, half lit when off |
| TAPE SENDS · FX | sends that are on half lit, off ones off, the focus fully lit; FX: parameters half lit, the focus fully lit |
| SETUP PROJECT | actions half lit, the focused BPM / SWING fully lit, PLAY fully lit in SONG |
| SETUP FILES | off (choose with ADD / SUB) |
| SETUP KEYS · CHORD · ARP · SYSTEM | settings half lit, the focus fully lit (with KEY LED focused the pads are the brightness ramp) |
| holding ESC | four columns, one per scene: the current scene fully lit, the other three half lit (column 4 is SETUP); the pads all off |
| holding TAB / CTRL | the keys the map accepts: the current one fully lit, the others half lit; the pads all off |
| holding FN / the FN menu | menu items half lit, a switch that is on fully lit, unavailable items and empty cells off |
| text entry | character keys half lit; erase and OK half lit once there is text |

- **The pads** are always the current track's keyboard: keys that sound half lit, held and sounding keys fully lit, keys that cannot sound off (see [[#play-keyboard]]).
- **The four left keys**: ESC, TAB and FN half lit, CTRL half lit when the screen has pages; a held key fully lit; FN fully lit while its menu is kept open (TAB and CTRL are off then, as in every pop-up).
- **Right side**: ADD / SUB half lit when there is something to change, OK half lit when it has an action (fully lit in the SEQ STEP view), PLAY half lit when stopped, fully lit when playing, blinking while recording is armed.

While {{key:FN}} is held the whole keyboard shows what FN can do on this screen:

| Keys | While FN is held |
|---|---|
| GRID | the menu: available items {{led:half|half lit}}, a switch that is on {{led:full|fully lit}}, unavailable items and empty cells {{led:off|off}} |
| PADS | the letters of the available items ({{key:C}} {{key:V}} {{key:X}} {{key:Q}} {{key:D}}, {{key:S}} {{key:T}}) and the quick controls {{key:6}} {{key:7}} {{key:8}} {{key:Y}} {{key:U}} {{key:I}} half lit; every other pad off |
| {{key:ESC}} {{key:TAB}} {{key:CTRL}} | ESC and TAB half lit; CTRL half lit when the scene has more than one page |
| {{key:ADD}} {{key:SUB}} {{key:OK}} {{key:PLAY}} | ADD / SUB half lit; OK half lit where the screen has an {{key:FN+OK}}; PLAY half lit, fully lit while recording is armed |

<!-- keywords: light rules, which keys light, why is it lit, why is it dark, fn lights, 灯光规则 -->

## Screen edges follow the key lights {#lights-edges}

The 4 + 4 strips on the screen's left and right edges **are the lights of the 8 edge keys**: each strip reads its key's light at that moment.

{{shot:tab-hint|Just after pressing TAB: the TAB key is fully lit, and the TAB strip on screen turns solid too.
mark strip:ESC = ESC strip
mark strip:TAB = TAB: fully lit = solid
mark strip:FN = FN strip
mark strip:CTRL = CTRL strip
mark strip:ADD = ADD
mark strip:PLAY = PLAY
key TAB = Fully lit
key ESC FN CTRL = Off
}}

| Key light | Screen strip |
|---|---|
| {{led:off|off}} | very faint |
| {{led:half|half lit}} | 30 % dither |
| {{led:full|fully lit}} | solid (the right-hand glyph turns black) |

Holds, the track hint, maps and blinking always match between screen and keys.

> [!tip] When nobody plays (no key for 4 seconds), every 10 seconds a light runs down both edges: each row's left and right key lights and screen strips rise and fall together, so you see at a glance that the screen's edge strips are the keys beside them.

<!-- keywords: edge, strips, sync, sweep, screen edges, 侧边, 色块 -->

## Brightness and the light test {#lights-ramp}

**KEY LED** (0–15) on SETUP's SYSTEM page is the overall brightness of all key lights.

{{shot:setup-keyled|KEY LED focused: from left to right the pads are a brightness ramp, dark to bright.
mark cell:1,2 = KEY LED brightness
mark cell:1,1 = Master volume
mark cell:1,3 = Screen brightness
key r0c1-r3c1 = Off
key r0c2-r3c2 = Half lit: the dimmest glow
key r0c3-r3c7 = Each brighter than the last
key r0c8-r3c8 = Fully lit
}}

{{strip
hold ESC + G4 > setup-project @8,0,41,11 | Hold {{key:ESC}} and press a key of GRID column 4: {{scene:SETUP}}.
hold CTRL + G6 > setup-system @footer | Hold {{key:CTRL}} and press G6: the SYSTEM page.
G2 > setup-keyled @cell:1,2 | Tap KEY LED: the pads become a brightness ramp. {{key:ADD}} / {{key:SUB}} set the overall brightness.
}}

> [!warn] If every column of the ramp looks equally bright, the light dimming is not working.

<!-- keywords: led brightness, dim lights, brighter, light test, test, ramp, 灯亮度 -->

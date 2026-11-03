# FAQ {#faq}

<!-- keywords: faq, help, problem, trouble, troubleshooting, questions, 常见问题 -->

## No sound {#faq-no-sound}

Check in this order:

{{strip
hold FN + 7 > fn-volume @14,94,212,28 | Master volume: hold {{key:FN}} and turn it up with {{key:7}} (VOLUME pops up at the bottom).
- > tape-mix @cells:2,1-3,4 | {{scene:TAPE}} {{page:MIX}}: is this track muted (MUTE lit), another one soloed (SOLO lit), or the fader too low?
- > instr-juno-fx @cells:1,1-1,3 | {{scene:INSTR}} {{page:FX}}: is FILTER turned to one end, or CRUSH / DRIVE too high?
}}

- A SAMPLER track needs a sample loaded first (see [[#sampler-load]]).
- While a sample is being recorded the speaker is silent (the microphone shares its clock); it comes back when the recording screen closes.
- The web simulator needs a click on the page before it plays sound (see [[#web-audio]]).
- Still nothing: stop all with {{key:FN+ESC}} and try again.

<!-- keywords: no sound, silent, too quiet, cannot hear, muted, speaker, 没声音 -->

## A note keeps sounding {#faq-stuck}

Hold {{key:FN}} and press {{key:ESC}}: stop all stops the sequencer, every ARP (HOLD included) and every sounding note. The HOME side key does nothing in the groovebox.

<!-- keywords: stuck note, hanging note, won't stop, drone, panic, all notes off, latch, hold, 卡音 -->

## Why are the lights like this? {#faq-lights}

The lights have only four meanings: {{led:off|off}} {{led:half|half lit}} {{led:full|fully lit}} {{led:blink|blinking}} (see [[#lights-states]]). While {{key:FN}} is held, only the keys that do something with FN are lit; the others do nothing. If every lit key looks equally bright, select KEY LED on {{scene:SETUP}} {{page:SYSTEM}} and look at the brightness ramp on the pads (see [[#lights-ramp]]): every column should be a different brightness.

<!-- keywords: lights wrong, all lit, too bright, too dim, dark keys, 灯不对 -->

## Where did the FN panel go? {#faq-settings}

{{key:FN}} is now the menu of the current screen (see [[#fn-menu]]); the quick controls {{key:FN}} + {{key:6}} / {{key:7}} / {{key:8}} and the letters work as before. The settings moved:

- Keyboard (layout, scale, root, octave, velocity, lock, SELECT ONE / MULTI) and the device (volume, key lights, screen): scene {{scene:SETUP}}, pages KEYS and SYSTEM. Hold {{key:ESC}} and press a key of GRID column 1, or use SETUP, the bottom-right item of every FN menu (see [[#setup]]).
- BPM, swing and PLAY LOOP / SONG: {{scene:SETUP}} {{page:PROJECT}}.
- Chord and arpeggiator: each track has its own, on the CHORD and ARP pages of {{scene:INSTR}} (not on a drum track; see [[#play-chords]]).
- The length of the pattern: {{scene:SEQ}} {{page:TRACK}} (see [[#seq-track]]).

<!-- keywords: fn panel, where are the settings, settings moved, setup, preferences, fn 1, fn 5, chord, arp, swing, 设置在哪 -->

## Where is my work kept? Can I just pull the power? {#faq-save}

Yes. The working state (patterns, sounds, mix, song, keyboard, chords and ARPs, tempo) is saved by itself in the device's own memory, card or no card, and comes back when you switch on. It is written once you stop pressing keys for a moment and nothing plays; while something keeps playing, at least once a minute. Not kept: capture takes and whether it was playing.

To keep a version as a file, save it: {{key:FN+S}} (a new project is named SONG 01, SONG 02 …), or SAVE AS in the FN menu of {{scene:SETUP}} {{page:PROJECT}} for a name of your own (see [[#project-save]]). Projects go to `Cardenza/Apps/Cardenza/Synth/Projects` on the SD card. Settings save themselves.

> [!warn] A very long project (about 2000 notes) does not fit: the patterns that are not playing stay out, and the screen says TOO LONG TO AUTOSAVE: FN+S TO CARD. Save it to the card. A sample recording without a card lives only in memory and is lost with the power.

<!-- keywords: lost my work, where saved, autosave, power off, pull the plug, battery, switch off, save name, song 01, 丢失, 断电 -->

## Is there undo? {#faq-undo}

There is no undo. That is why clearing a whole pattern, overwriting a file or starting a new project always asks first ({{key:OK}} = yes, {{key:ESC}} = no).

> [!tip] The safest way: save a good version as another project (SAVE AS in the FN menu of SETUP PROJECT) or duplicate the pattern ({{key:FN+D}}) before changing it. A phrase you played well can also be recovered from capture (see [[#capture]]).

<!-- keywords: undo, redo, revert, go back, 撤销 -->

## Can I use MIDI or a computer (USB)? {#faq-midi}

Yes, with one USB cable the device is a MIDI device and a sound card for the computer:

- **MIDI in**: a keyboard or DAW on the computer plays the tracks: channels 1–4 are tracks 1–4, any other channel plays the current track. Controllers (CC) go to that track's engine.
- **MIDI out**: what the device plays (keys and sequencer) goes to the computer, track n on channel n, so you can record it.
- **Audio**: the computer records the device's sound (before the device's volume), and what the computer plays comes out of the device.

There is no MIDI clock sync yet and no MIDI socket. In the web manual the same cable can also mirror the real keys (see [[#web-usb]]).

<!-- keywords: midi, usb midi, usb audio, sound card, audio interface, computer, daw, external keyboard, sync, clock, 外接键盘, 电脑 -->

## How do long patterns work? {#faq-long}

A pattern has 1 to 256 steps; the GRID shows 16 at a time, the page number is at the bottom right (like `2/4`).

- Length: STEPS on {{scene:SEQ}} {{page:TRACK}}, or LEN − / LEN + in the FN menu of the SEQ pages (a whole page at a time).
- Pages: {{key:FN+ADD}} / {{key:FN+SUB}} on the note page, or PAGE − / PAGE + in the FN menu. While playing, the GRID follows the playhead.

Every pattern keeps its own length (see [[#seq-track]]).

<!-- keywords: long pattern, more than 16 steps, 32 steps, 64 steps, pattern length, page, steps, 长 pattern -->

## How do I find a sound by name? {#faq-search}

Open the preset list ({{key:OK}} on {{scene:INSTR}} {{page:VOICE}}, or PRESETS in the FN menu) and press {{key:FN+F}}: type a part of the name and confirm with {{key:OK}}. The pads do not type in the list, so a stray key never starts a search. The sample list works the same way (see [[#instr-presets]]).

<!-- keywords: search, find preset, find sample, filter, by name, 搜索 -->

## How do I switch to another app? {#faq-apps}

The groovebox is one app on the device. Switch apps with the device's own launcher; the working state is kept when you leave and restored when you come back.

<!-- keywords: app, launcher, switch app, other firmware, 应用, 启动器 -->

## Crackles while playing {#faq-crackle}

The audio budget is tight: very dense chords with many effects can exceed it. Play fewer notes at once, turn off an effect or two (especially several choruses and reverb sends), or slow the ARP down. See [[#fx-cpu]].

<!-- keywords: crackle, glitch, pops, distortion, audio dropout, 爆音 -->

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
- The web simulator needs a click on the page before it plays sound (see [[#web-audio]]).
- Still nothing: press the HOME side key (stop all) and try again.

<!-- keywords: no sound, silent, too quiet, cannot hear, muted, 没声音 -->

## A note keeps sounding {#faq-stuck}

Press the HOME side key (or {{key:FN+ESC}}): it stops the sequencer and silences everything, the ARP's LATCH included.

<!-- keywords: stuck note, hanging note, won't stop, drone, 卡音 -->

## Why are the lights like this? {#faq-lights}

The lights have only four meanings: {{led:off|off}} {{led:half|half lit}} {{led:full|fully lit}} {{led:blink|blinking}} (see [[#lights-states]]). If every lit key looks equally bright, select KEY LED on SETUP's SYSTEM page and look at the brightness ramp on the pads (see [[#lights-ramp]]): every column should be a different brightness.

<!-- keywords: lights wrong, all lit, too bright, too dim, 灯不对 -->

## Where did the FN panel go? {#faq-settings}

The settings it had are now in the fourth scene, {{scene:SETUP}}: hold {{key:ESC}} and press a key of GRID column 4 (see [[#setup]]). KEYS, CHORD, ARP and SYSTEM are pages there; BPM, swing and LOOP / SONG are on its PROJECT page, the steps of a track on SEQ TRACK. {{key:FN}} itself is now the menu of the current screen (see [[#fn-menu]]); the quick controls {{key:FN}} + {{key:6}} / {{key:7}} / {{key:8}} and the letters work as before.

<!-- keywords: fn panel, where are the settings, settings moved, setup, preferences, fn 1, fn 5, 设置在哪 -->

## Where is my work kept? How do I not lose it? {#faq-save}

Save projects with {{key:FN+S}} or on {{scene:SETUP}} {{page:PROJECT}} (see [[#project-save]]); they go to `Cardenza/Apps/Cardenza/Synth/Projects` on the SD card. Settings save themselves. The working state before an app switch or power-off is kept automatically (see [[#project-session]]).

<!-- keywords: lost my work, where saved, autosave, 丢失 -->

## Is there undo? {#faq-undo}

There is no undo. That is why clearing a whole pattern, overwriting a file or starting a new project always asks first.

> [!tip] The safest way: save a good version as another project (SAVE AS on SETUP PROJECT) or duplicate the pattern ({{key:FN+D}}) before changing it. A phrase you played well can also be recovered from capture (see [[#capture]]).

<!-- keywords: undo, redo, revert, go back, 撤销 -->

## Can I use a MIDI keyboard or sync other gear? {#faq-midi}

Not yet: there is no external MIDI / USB sync. Everything is played on the device's own keys.

<!-- keywords: midi, external keyboard, sync, usb midi, clock, 外接键盘 -->

## How do I switch to another app? {#faq-apps}

The groovebox is one app on the device. Switch apps with the device's own launcher; the working state is kept when you leave and restored when you come back.

<!-- keywords: app, launcher, switch app, other firmware, 应用, 启动器 -->

## Crackles while playing {#faq-crackle}

The audio budget is tight: very dense chords with many effects can exceed it. Play fewer notes at once, turn off an effect or two (especially several choruses and reverb sends), or slow the ARP down. See [[#fx-cpu]].

<!-- keywords: crackle, glitch, pops, distortion, audio dropout, 爆音 -->

# Web simulator {#web}

{{icon:web}} Without a device, use it right in the browser: [cardenza-synth.203.io](https://cardenza-synth.203.io/) (or [cardenza-synth.vercel.app](https://cardenza-synth.vercel.app/)). It is not a look-alike but the same firmware code (the groovebox interface, the AMY engines, the sequencer, the storage protocol) compiled to WebAssembly: screen, key lights and sound are the same as on the device.

<!-- keywords: simulator, emulator, web, browser, online, try it, no device, 模拟器 -->

## Follow along in the manual {#web-live}

Every screenshot in this manual has **▶ Try it live here** under it:

| | |
|---|---|
| 1 | Press it and the screenshot becomes a running device (the same firmware code), which goes to the screen of the picture by itself (scene, track and page through holding ESC / TAB / CTRL + GRID, just as you would on the device) |
| 2 | Click the device (yellow dashed frame) and your computer keyboard plays it (keys below); click elsewhere on the page and it no longer does |
| 3 | "Go to this screen" goes to this section's screen again; "Close" puts the device away |

- Only one device runs on the page: the section you opened last is the one you operate; the others go back to their screenshots. Moving to another section takes the device there without losing its state.
- Switching between 中文 and English keeps the device at the same picture.

<!-- keywords: try it, try it live, live, follow along, interactive, hands on, 跟着操作, 实机 -->

## Connect your Cardenza (USB) {#web-usb}

{{icon:usb}} Plug the Cardenza into the computer with a USB cable (Chrome or Edge). There is no button to press: when the [guide](./) opens, the browser asks whether the page may use MIDI devices; allow it, and from then on the device connects as soon as it is plugged in.

- In the guide the device follows the lessons: it goes to the lesson's screen, the keys to press blink (the others dim); when you get it right the page says so; try a few more keys, then press Next.
- In this manual every key you press on the real device is pressed on the device running in the manual too: keys go to the device of the section you opened last; if none is open, the section at your reading position opens. The Cardenza itself keeps running as usual.
- Unplug the cable to disconnect; plug it back in and it reconnects.
- It talks to the Cardenza over USB MIDI (Web MIDI, SysEx included): no driver is needed, and USB MIDI and USB audio keep working. If you blocked it before, allow MIDI devices in the site settings (left of the address bar) and reload.

<!-- keywords: usb, connect, device, cable, web serial, hardware, real device, device not found, serial port, midi, web midi, sysex, allow, 连接, 真机 -->

## Which computer key is which {#web-keys}

{{icon:keyboard}} The computer keyboard maps to the device's 4×14 keys by **physical position**; every key cap shows its computer key, and the key on the page lights up when pressed:

{{keymap:The computer keyboard's four rows are the device's four rows
r0c1-r0c12 = Number row 1 … =
r1c1-r1c12 = The Q row
r2c1-r2c12 = The A row
r3c1-r3c12 = The Z row
ESC TAB FN CTRL = Esc · Tab · Caps Lock / left Shift · Ctrl
ADD SUB OK PLAY = Backspace · \ · Enter · Space
}}

| Device | Computer |
|---|---|
| {{key:ESC}} {{key:TAB}} {{key:FN}} {{key:CTRL}} | Esc / Tab / Caps Lock or left Shift / Ctrl |
| {{key:ADD}} {{key:SUB}} {{key:OK}} {{key:PLAY}} | Backspace / `\` / Enter / Space |
| HOME side key | Home |
| the pads without a letter | right Shift (row 3) · left / right Alt (row 4) |

- {{key:ESC}} is also the backquote key (left of 1). On macOS Caps Lock latches {{key:FN}} (press it again to release it); hold left Shift for a {{key:FN}} that lets go with the key.
- You can also click or touch the keys on the page, several at once on a touch screen. A right-click latches a key down (to hold ESC, FN… while you click the GRID); right-click it again to release it.

<!-- keywords: keyboard mapping, computer keyboard, which key, key bindings, shortcuts, caps lock, mac, right click, latch, 键位映射 -->

## Sound {#web-audio}

{{icon:sound}} Browsers only play sound after a click on the page: the first time, click anywhere (or "Start audio"). The sound is synthesized live by the same audio engine; latency depends on the browser and computer.

<!-- keywords: no sound, audio, start audio, sound not working, latency, 没声音 -->

## Storage and samples {#web-storage}

- Settings, projects and sounds are stored in the browser (IndexedDB) in the same folders as on the SD card, and survive a reload. Clearing the site's data deletes them. The simulator and the device in this manual share them.
- The working state that the device keeps by itself ([[#project-session]]) is kept by the browser too: it is saved as you go and is still there after a reload or after closing the page, like pulling the power on the device. To keep it as a project, save with {{key:FN+S}} as usual.
- On the simulator page, "Load project…" copies `.ccproj` files from a Cardenza's card into the browser (never over an existing project, a name already taken gets a number) and opens the first one; the others are in SETUP FILES.
- "Load WAV…" puts audio files from your computer into the shared sample folder `Shared/Samples`, where the SAMPLER's file list finds them. Anything that is not a mono 16-bit WAV (stereo, 24-bit, MP3…) is converted to mono 16-bit 44.1 kHz and cut to 5 seconds.

<!-- keywords: browser storage, autosave, kept after reload, indexeddb, load wav, upload sample, load project, import project, ccproj, where is it saved, reload, 浏览器存储 -->

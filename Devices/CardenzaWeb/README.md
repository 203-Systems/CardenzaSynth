# CardenzaWeb: browser simulator

The groovebox in a web page. This is the real product code (`GrooveApp`, the Groove UI, the synth runtime with AMY, and the `CardenzaSim` device facade) compiled to WebAssembly with Emscripten. The page shows the 240×135 framebuffer the C++ app renders and the 4×14 keyboard with the LED levels the app produces. It plays the audio from `synth::audio::RenderFrames`.

## Build and serve

Install the Emscripten SDK once (official emsdk, latest release):

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest
~/emsdk/emsdk activate latest
```

Then, from the repository root (needs `cmake` on `PATH`, plus the AMY submodule: `git submodule update --init`):

```sh
make web          # = make DEVICE=CardenzaWeb build  ->  build/web/site/
make web-serve    # python3 -m http.server on build/web/site, http://localhost:8000/
```

Without the Makefile:

```sh
source ~/emsdk/emsdk_env.sh
emcmake cmake -S . -B build/web -DDEVICE=CardenzaWeb
cmake --build build/web --target CardenzaWeb
python3 -m http.server 8000 --directory build/web/site
```

Makefile variables: `EMSDK` (default `~/emsdk`, used when `emcmake` is not on `PATH`), `CMAKE`, `WEB_BUILD` (default `build/web`), `WEB_PORT` (default 8000).

`build/web/site/` holds `index.html`, `cardenza-web.js`, `cardenza-web.css`, `cardenza-audio-worklet.js`, `cardenza.js` and `cardenza.wasm`. Any static web server can serve it. It needs no COOP/COEP headers (no threads), so it also works inside an iframe, and it makes no network requests outside its own directory. Opening the page over `file://` does not work (browsers block wasm fetches there). AudioWorklet needs a secure context (https or localhost); on plain http from another host the page falls back to ScriptProcessorNode.

## How it works

- `WebMain.cpp` boots the runtime like the device and `CardenzaSimShot` (`os::system::Begin`, then `os::system::RunFrame` about every 16 ms from `emscripten_set_main_loop`). Key changes go through `CardenzaSim::Input` and run a frame immediately, like the headless runner.
- C API for the page (`EMSCRIPTEN_KEEPALIVE`):

  | Function | Purpose |
  |---|---|
  | `cardenza_web_framebuffer()` | `Device::Display::CurrentFrameBuffer().pixels`: 240×135 RGB565, copied to the canvas each frame |
  | `cardenza_web_leds()` | `Device::LED::Brightness` for all 56 keys, then master, enabled, backlight |
  | `cardenza_web_frame_counter()` | increments after each device frame |
  | `cardenza_web_set_key(row, column, pressed)`, `cardenza_web_set_home(pressed)`, `cardenza_web_release_all()` | input |
  | `cardenza_web_render(frames)` | next mono frames of `synth::audio::RenderFrames` at 44.1 kHz, as floats with the device output volume applied |
  | `cardenza_web_sample_rate()`, `cardenza_web_sample_directory()` | constants for the page |
  | `cardenza_web_flush_storage()` | writes pending settings now (page hidden or closed) |

- **No threads.** The sequencer clock thread is replaced by `SynthSequencer::PumpClock(nowMicros)` (an `__EMSCRIPTEN__`-only addition). `cardenza_web_render` calls it before every 256-frame AMY block with the audio time, so sequencer pulses land on exact block positions however the browser schedules the main thread. Measured 120 BPM quarter notes are 22016 or 22272 frames apart (the ideal is 22050). That is the AMY block quantization the device also has.
- **Audio.** The synth renders on the main thread, where the wasm module lives. An AudioWorklet (`cardenza-audio-worklet.js`) plays the posted frames from a ring buffer and reports how much it consumed every 256 frames. The page keeps about 2048 frames (46 ms) queued and raises the target by 512 frames after an underrun (up to 139 ms). The context is requested at 44.1 kHz; when the browser runs another rate, the worklet resamples linearly. Audio starts on the first click or key press (browser autoplay rules) and the page shows a hint until then. While no output runs, the page still renders at the wall-clock rate and discards the frames, so the sequencer, notes and meters behave the same.
- **AMY** is compiled with `-U__EMSCRIPTEN__` (C sources of `CardenzaAmy` only). Its own web build would switch to 48 kHz, wasm-worker locks and JS hooks, while the groovebox runtime mixes AMY with its 44.1 kHz sampler and effects. So it is built as the plain host build the desktop simulator uses.
- **Storage.** `Device::Storage` is the simulator's (`Devices/CardenzaSim/Storage.cpp`): the card is `/sdcard`, laid out by the storage protocol (`docs/app-storage-protocol.md`): `/sdcard/CardenzaSynth/Settings/{system,synth}.cfg`, `Projects/`, `Presets/`, `Samples/`, `Session/` (the autosave). The page mounts `/sdcard` as IDBFS with `autoPersist` and loads it before `main()` runs, so settings, saved projects, user presets and the working state survive reloads. Every closed file, rename and delete is written back to IndexedDB. Settings are written a second after the last change; when the page is hidden or closed it calls `cardenza_web_flush_storage()` to write them and the autosave at once and then syncs. The card is always present here. If IndexedDB is unavailable the page says "memory only".
- **Samples.** "Load WAV…" copies files into `Samples/`, where the sampler's FILE browser lists them. Mono 16-bit WAVs (8 to 96 kHz, at most 5 s) are copied as they are. Anything else the browser can decode is mixed to mono, converted to 16-bit 44.1 kHz and trimmed to 5 s.

## Keyboard

Computer keys are mapped by physical position (`KeyboardEvent.code`), so the mapping does not depend on the keyboard layout. Each keycap on the page shows its computer key at the bottom, and the status line names the device key for every key you press.

| Device key | Row, column | Computer key |
|---|---|---|
| ESC | 0,0 | Esc, `` ` `` |
| pads `1`–`8` | 0,1–0,8 | 1–8 |
| GRID `9 0 - =` | 0,9–0,12 | 9, 0, -, = |
| ADD (+, keycap `del`) | 0,13 | Backspace (also Delete, numpad +) |
| TAB | 1,0 | Tab |
| pads `Q`–`I` | 1,1–1,8 | Q–I |
| GRID `O P [ ]` | 1,9–1,12 | O, P, [, ] |
| SUB (−, keycap `\`) | 1,13 | \ (also numpad −) |
| FN | 2,0 | Caps Lock, left Shift |
| pad (keycap `shift`) | 2,1 | right Shift |
| pads `A`–`J` | 2,2–2,8 | A–J |
| GRID `K L ; '` | 2,9–2,12 | K, L, ;, ' |
| ENTER | 2,13 | Enter (also numpad Enter) |
| CTRL | 3,0 | Ctrl (left or right) |
| pad (keycap `opt`) | 3,1 | left Alt / ⌥ |
| pad (keycap `alt`) | 3,2 | right Alt / ⌥ |
| pads `Z`–`N` | 3,3–3,8 | Z–N |
| GRID `M , . /` | 3,9–3,12 | M, ,, ., / |
| PLAY (keycap `space`) | 3,13 | Space |
| HOME (side button) | — | Home |

Mapped keys have their browser default actions blocked (Tab focus, Space scrolling, Backspace). Key repeat is ignored because the device repeats ADD / SUB itself. All keys are released when the window loses focus or the page is hidden. Shortcuts with ⌘ (macOS) pass through to the browser. Ctrl+W and Ctrl+T cannot be intercepted, so the page asks before closing while Ctrl is held. On macOS, Caps Lock sends key-down when it turns on and key-up when it turns off, so there it latches FN; hold left Shift for a momentary FN.

Mouse: press and hold a keycap. A right-click latches a key down (for example ESC or FN while you click the GRID with the same mouse); right-click it again to release it. Touch: every finger holds its own key.

## Limitations

- No microphone input: the sampler's REC reports that input is not ready, as on the Linux simulator. Load WAVs instead.
- The sequencer runs as far ahead of what you hear as the audio buffer (about 50–60 ms). Live-recorded notes therefore land that much late, before the sequencer's own quantization.
- `-DCARDENZA_GROOVE_UI=OFF` (the previous UI) builds, but its sequencer is not pumped.
- Projects live in the browser profile (IndexedDB). There is no import or export of project files yet, and clearing site data deletes them.

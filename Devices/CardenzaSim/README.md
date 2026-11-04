# CardenzaSim Desktop Debugging

`CardenzaSimDesktop` now has a scriptable debug mode for driving the simulator and dumping the internal framebuffer directly to disk. This is meant for repeatable UI debugging, regression capture, and agent-driven workflows.

From the repo root, use `make DEVICE=CardenzaSim run` to build and launch the simulator, or `make DEVICE=CardenzaSim build run` if you want the explicit two-step target form.

## CLI options

- `--debug-script=<path>` loads a text script and executes it one command at a time on frame ticks.
- `--debug-output=<dir>` changes the base directory for `screenshot` outputs. Relative paths are resolved from the script directory.
- `--debug-exit-on-complete` closes the simulator when the script finishes.
- `--debug-hide-window` keeps the Win32 window hidden while the script runs.
- `--scale=<1|2|4>` still works as before.

Example:

```powershell
build/CardenzaSim/Devices/CardenzaSim/Debug/CardenzaSimDesktop.exe `
  --debug-script="Devices/CardenzaSim/debug/sample.simdebug" `
  --debug-output="Devices/CardenzaSim/debug/out" `
  --debug-exit-on-complete `
  --debug-hide-window
```

## Script format

One command per line. Blank lines and lines starting with `#` are ignored.

Supported commands:

- `wait <frames>`: wait for a number of simulator frame ticks.
- `tap <key>`: press and release a key.
- `down <key>`: press and hold a key.
- `up <key>`: release a held key.
- `screenshot <path>`: dump the 240x135 device framebuffer as a `.bmp`.
- `quit`: stop the script immediately.

Key targets can be written in either form:

- `row,column`, for example `tap 1,0`
- a visible key label from the desktop keyboard, case-insensitive, for example `tap TRK`, `tap ok`, `tap space`, `tap A`, `tap del`

Example script:

```text
# Open a menu, wait for the UI to settle, then capture it.
tap TRK
wait 4
screenshot home.bmp

# Hold a modifier while tapping another key.
down opt
tap ok
up opt
wait 2
screenshot confirm.bmp

quit
```

## Notes

- `screenshot` writes the device framebuffer only. It does not capture the decorative desktop shell around the screen.
- Relative screenshot paths are resolved under `--debug-output` when provided, otherwise next to the script file.
- If a script line is invalid, the simulator stops and reports the script path plus line number.
- `down` states are automatically released when the script finishes, so a failed script does not leave keys stuck.

## Headless screenshots (macOS, Linux, Windows)

`CardenzaSimShot` replays the same script format without a window and writes PNG files. It boots the production runtime like the device (`os::system::Begin()`, then `os::system::RunFrame()` every 16 ms) and drives input only through `CardenzaSim::Input`, so it works with whatever UI the application renders.

```sh
cmake -S . -B build/CardenzaSim -DDEVICE=CardenzaSim -DCMAKE_BUILD_TYPE=Debug
cmake --build build/CardenzaSim --target CardenzaSimShot
build/CardenzaSim/Devices/CardenzaSim/CardenzaSimShot \
  --script tests/ui-review.script --out build/shots --scale 2 --root build/shots-root
```

- `--script <file>` and `--out <dir>` are required. `--scale N` (1-4) upscales with nearest-neighbour sampling.
- The card is `<cwd>/sdcard`, laid out by the storage protocol (`docs/app-storage-protocol.md`, `Devices/CardenzaSim/Storage.cpp`); as on the device, nothing is kept anywhere else. `CardenzaSim::Input::SetCardInserted(false)` takes it out; out when the system starts, the whole run is one without a card (`CardenzaSimShot --no-card`). `--root <dir>` changes to `<dir>` (created if missing) before boot, so a run can use a scratch sdcard. Device and app settings and the autosave persist in that root between runs, so use a fresh root for reproducible screenshots. Fixtures such as `qafile.wav` go in `<dir>/sdcard/CardenzaSynth/Samples/`. Relative `--script`/`--out` paths are resolved against the caller's directory.
- `screenshot <name>` writes `<out>/<name>.png`; `.png` is appended, and a `.bmp` extension from desktop scripts is replaced. A `<name>.leds.txt` file is written beside it: four lines of fourteen LED levels (0-15), then `brightness <0-15> enabled <0|1>`.
- Key targets are `row,col` or the physical legends, case-insensitive: `` ` ``/`esc 1 2 3 4 5 6 7 8 9 0 - = del`, `tab q w e r t y u i o p [ ] \`, `fn shift a s d f g h j k l ; ' enter`, `ctrl opt alt z x c v b n m , . / space`. The desktop spellings `Aa` and `ok` and the shifted legends (`!`, `{`, `?`, ...) also work. `home` is the separate HOME button (`tap home`, `down home`, `up home`). An unknown target stops before boot with `<script>:<line>:` and exit code 1.
- Timing matches the desktop debug session: each 16 ms tick runs one frame, then one command. `down`/`up` run a frame straight away, and `tap` is down, frame, up, frame. `wait N` idles for N more ticks. A `#` token after a command's argument starts a comment. Held keys are released at the end.
- Hold gestures and animations use wall-clock time, so the run takes real time (about 13 s for `ui-review.script`). The summary line reports late ticks if the host could not keep up.

## Native sequencer render regression (macOS, Linux, Windows)

`CardenzaSequencerRenderTests` drives the public simulated keyboard and HOME button through `os::system::RunFrame()`. It uses the production application, sequencer, MIDI route and 240×135 RGB565 renderer. This target needs no desktop window. It checks point selection, note editing, timed animation, value lanes, playback feedback and LED zoning. `CardenzaLedTests` separately checks PWM levels and complete-frame publication without blank intermediate frames.

```sh
cmake -S . -B build/CardenzaSim -DDEVICE=CardenzaSim -DCMAKE_BUILD_TYPE=Debug
cmake --build build/CardenzaSim --parallel 4
ctest --test-dir build/CardenzaSim --output-on-failure
build/CardenzaSim/tests/CardenzaSequencerRenderTests build/sequencer-review
```

The optional directory argument saves 15 static PPM screenshots, 130 playback frames and 26 entry/edit animation frames, each with a matching LED-state JSON file. With Visual Studio, use the equivalent `tests/Debug/CardenzaSequencerRenderTests.exe` path and pass `-C Debug` to CTest. These captures establish native rendering and input behavior; they do not establish display contrast, physical LED brightness or audio quality on the device.

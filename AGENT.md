# CardenzaSynth Agent Notes

## CardenzaHAL is a read-only dependency (hard rule)

`Devices/Cardenza/cardenza` is the git submodule https://github.com/203-Systems/CardenzaHAL, the shared
Cardenza hardware layer (also used by Cardenza-MPE). We consume it; we do not maintain it.

- **Never change it.** Do not edit, add, delete, format or generate anything under `Devices/Cardenza/cardenza`, do not commit inside it, and never push to `github.com/203-Systems/CardenzaHAL` (no fixes, branches, tags, force pushes or pull requests), even when the account has the rights. Do not copy HAL code into this repository either.
- **Pulling is fine.** `make hal-update` moves the submodule to upstream `main`; then commit only the new submodule pointer here. Never pin it to a commit that is not on upstream `main`.
- **Reuse it.** Use what the HAL provides through its public headers, included from the HAL root (`"cardenzaHAL.h"`, `"keyboard/keyboard.h"`, `"audio/audio.h"`, `"mic/mic.h"`): keyboard scan and key LEDs (`setLED`, `setLEDGlobalBrightness`, `enableLED`), audio TX with mono / stereo `Init` and codec volume, mic RX channel, display panel handles and backlight, SD card mount, Func button, IR. Do not re-implement a driver or a HAL feature in this repository.
- **Missing or wrong in the HAL?** Work above it in `Devices/Cardenza` (the device layer, ours) or model it in shared code (`Devices/LedLight.h` models the LED scan for the simulators). Tell the user what the HAL would need, so its maintainer can change it upstream. Never patch the submodule to get there.
- **Enforced.** `make hal-check` runs before every firmware build and the CMake configure checks too: they fail when the HAL has local changes or a commit that is not on upstream, and they set the submodule's push URL to `no-push://…` so a push cannot leave the machine.
- The HAL keeps its own rules in `Devices/Cardenza/cardenza/AGENT.md` (ESP-IDF only, native handles, no copyleft code); they matter only for reading it.

## Workflow

- Do not start builds from the agent unless the user explicitly asks for one.
- Prefer static validation such as `get_errors`, targeted searches, and diff inspection when editing the Cardenza HAL layer.

## Storage Rules

The spec is `docs/app-storage-protocol.md`; keep it in sync with behaviour changes.

- Apps store data only through the OS storage API (`OS/Storage`: `os::storage::AppFolder`, `AppSettings`, `SharedSamplesDirectory`, `WriteFileAtomic`), in their own app folder `Cardenza/Apps/<Author>/<Name>/` (plus `Shared/` for samples). Never hard-code card paths and never call `Device::Storage` from app code (enforced by ValidateArchitecture).
- `Device::Storage` is the low-level card (root, mount, free space, internal-flash records). Folder knowledge belongs to `OS/Storage`, not to the device layer.
- Settings are `key=value` text read tolerantly (unknown keys kept); change them in RAM and let the system write them (one second after the last change, on suspend). Volume, key LEDs and screen are device settings (`os::storage::Set…`); projects never carry them.
- An app persists its working state in `Suspend()` (its `Session/` folder) and restores it in `Setup()`; the system destroys the app before it creates the next one.
- Groovebox project files stay format 4 (version 5 is reserved for the unreleased development format with a PLAY line); the next format change takes version 6.

## Groovebox UI Rules

The spec is `docs/groovebox-design.md`; keep it in sync with behaviour changes.

- Fixed key roles: left column ESC / TAB / FN / CTRL (no on-screen labels, only four colored dither strips), right column ADD / SUB / ENTER / PLAY, columns 1–8 always play the current track, columns 9–12 mirror the on-screen 4x4 cell grid.
- Holding a left key turns the GRID into a map of what that key switches (ESC scenes by column, TAB tracks laid out like MIX, CTRL every page of the scene, FN the settings of the FN panel page); choices go on the GRID, not on the pads or number keys. Keep each scene at 16 pages or fewer so the CTRL map fits.
- Colors carry meaning: ESC red-orange = scenes / back, TAB cyan = tracks, FN yellow = second functions, CTRL violet = pages, green = playing, red = recording / danger. Selection is terminal inverse (ink background, black text). Area fills use the 4x4 Bayer dither, not flat blocks.
- Held FN shows the FN panel, the same five pages as FN SETUP (`SetupPage`, `UiState::fnPage` / `fnFocus`): FN + 1..5 or FN + CTRL pick the page, FN + GRID the setting, FN + ADD / SUB change it. New global settings go on a panel page, not on their own Fn key. In input screens (`FnYellowMode()`) FN is the yellow keycap layer instead (`; . , /` arrows, text cursor).
- Every Fn shortcut on the pads or the edge keys must appear in `Controller::FnEntries()` so the panel footer documents it.
- Key LEDs are driven by CardenzaHAL (read-only, see the top of this file); it drives each key at 16 linear duties and gamma-corrects only the master level, so `Device::LED` key levels are perceived brightness and the device layer gamma-maps them (gamma 2.6, `Devices/LedLight.h`, also the simulators' model). Every key is off (`groove::kLedOff`, nothing there), half (`kLedHalf` = 6, the dimmest glow, HAL duty 1/15, there but not chosen) or full (`kLedFull` = 15, chosen / held / sounding / playing); Blink is the only effect (waiting). Do not add in-between levels, breathing or press ripples, and never let an animation keep a key lit that the state says is off. The screen's edge strips mirror the edge-key LEDs through `Animator::LedShown`.
- While FN is held no key may do its plain function: a key either has an FN function or does nothing (and its LED is off). Every FN shortcut reports its result with `Controller::ShowHud` (name, value, bar); panel settings show theirs in the panel.
- Settings that apply to every track (keyboard layout, scale, chord, arpeggiator, system) live on the FN panel as `keys.*` / `arp.*` / `proj.*` / `sys.*` slots handled by `SettingValue` / `AdjustSetting`, not on INSTR pages.
- The Learn manual (`docs/learn`, built by `make learn` into `build/web/site/learn`, deployed with the web simulator) documents every key, page and setting in Chinese and English. A change in behaviour updates both `docs/learn/zh/*.md` and `docs/learn/en/*.md` (same section ids; `<!-- keywords -->` and `docs/learn/synonyms.json` keep it searchable). Reference tables and screenshots are generated from the code (`tools/learn/learn_dump.cpp`, the shot script in `tools/learn/build.py`); the build fails on broken links, unknown screenshots or search regressions (`tools/learn/search.test.js`). Pictures follow the product's look (`tools/learn/figures.py`): highlights with numbered markers and a numbered legend (`mark` / `key` lines), key caps as the web simulator draws them, step strips (`{{strip`); no hand-drawn rings or arrows.
- Tracks are identified by color everywhere (`theme::TrackColor`); never color a specific track with a role color.
- Track effects are `fx.*` engine parameters registered by `PagedSynthEngine::RegisterTrackFxParameters()`; the master bus is `Host::MasterFx` (project data). Both are defined in `Library/Synth/Audio/SynthFx.h`.
- Danger (overwrite, delete, discard) always confirms with ENTER = yes, ESC = no.
- The UI must not include ESP-IDF headers or device HAL code (enforced by ValidateArchitecture); use `groove::Host`.
- Check visual changes with `CardenzaSimShot` screenshots before flashing.

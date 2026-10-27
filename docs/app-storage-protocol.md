# App storage protocol (version 2)

Date: 2026-10-27. CardenzaSynth stores settings, projects, presets, samples and saved sessions under
`/CardenzaSynth` on the SD card. The SD card is the only persistent store. Without a writable card,
settings changes remain in RAM and explicit saves fail. Existing NVS data is neither read nor
written nor erased.

This reference defines paths, formats, ownership and compatibility. Code: `OS/Storage/` (API),
`OS/System/` (lifecycle), `Devices/Device.h` (`Device::Storage`, the low-level card).

## 1. Layout on the SD card

```text
<card>/CardenzaSynth/
  Settings/
    system.cfg                 device settings and storage protocol version
    synth.cfg                  app preferences (ROLL selection mode)
  Projects/
    <name>.ccproj               songs
  Presets/
    DX7/user.ccpreset           user preset bank for this engine
    Juno/user.ccpreset
    Sampler/user.ccpreset
    Drums/user.ccpreset
  Samples/
    <name>.wav                  sample files referenced by name
  Session/
    session.ccproj             working project saved on suspend
    session.cfg                session name, pending flag and unsaved-state flag
```

- `<card>` is `/sdcard` on the device, `<working directory>/sdcard` in the simulator (beside the
  executable on Windows), and `/sdcard` backed by IndexedDB in the browser simulator.
- The built-in groovebox's app id remains `Cardenza/Synth`. Its `AppFolder` maps directly to
  `<card>/CardenzaSynth`, with `Settings/synth.cfg` as its settings file. The id is not a disk path.
- Other registered apps use `<card>/CardenzaSynth/Apps/<Author>/<Name>/`, created only when needed.
  Each has `Settings/app.cfg`, `Projects/`, `Presets/` and `Session/`. Both app-id parts are 1–24
  characters of `A–Z a–z 0–9 _ -`. A normal Synth installation does not create an empty `Apps/` tree.
- Only the OS writes `Settings/system.cfg`. Each app writes its own settings and content. Apps may
  share `Samples/`; renaming or deleting a sample affects every project that references its name.
- FAT long file names are required for `CardenzaSynth`, `.ccproj`, `.ccpreset` and transaction files.
  User-facing file names remain ASCII.

## 2. File formats

| Content | Format | Compatibility |
|---|---|---|
| `Settings/*.cfg` | UTF-8-compatible ASCII text, `key=value` lines | Unknown keys survive a read/write cycle |
| `Session/session.cfg` | Same `key=value` syntax | The app regenerates session state; unknown keys are not preserved |
| `Projects/*.ccproj`, `Session/session.ccproj` | Text beginning with `CARDENZA_PROJECT 6` | Reads v1–v6; writes v6 |
| `Presets/<engine>/user.ccpreset` | Text beginning with `CARDENZA_PRESETS 1 "<engine-id>"` | Preset format remains version 1 |
| `Samples/*.wav` | Mono PCM16 WAV | Existing sample file names and project references stay valid |

Storage protocol version 2 describes the directory layout and settings persistence. It does not
change the project, preset or WAV formats. Project version 5 was used only by unreleased development
builds; its `PLAY` line is skipped on load. Format 6 stores keyboard and per-track chord/arpeggiator settings. The next project format change uses version 7.

### Settings syntax

```text
# Cardenza settings: Cardenza/Synth (storage protocol 2)
# key=value per line; unknown keys are kept
keys.select=1
```

- One `key=value` per line. Lines starting with `#` after optional spaces are comments. Spaces around
  the key and value are ignored; CRLF line endings are accepted.
- Keys contain 1–48 characters of `a–z 0–9 _ .` and start with a letter or digit. Use dotted groups
  such as `keys.scale`, `arp.rate` and `sys.volume`.
- Values contain at most 200 printable ASCII characters, with no leading or trailing spaces.
  Numbers are decimal integers. Booleans are written as `1` or `0`; reading also accepts
  `true/false`, `on/off` and `yes/no`. Enumeration indices are appended to, never reordered.
- Malformed lines are skipped. A repeated key keeps its first position and last value. Invalid or
  out-of-range numeric values read as the caller's default. In `Settings/*.cfg`, unknown keys are
  written back unchanged.
- A settings file is at most 16 KB. `Settings/system.cfg` records `protocol=2`.

### File replacement

`WriteFileAtomic` writes and closes `<file>.tmp`, moves an existing file to `<file>.bak`, then
renames the temporary file into place and removes the backup. Readers use `<file>.bak` when an
interrupted replacement left the main file missing. Projects and preset banks use the same
replacement pattern.

Callers must check the save result. This replacement scheme avoids truncating the current file in
place; it does not guarantee recovery from a failing card or arbitrary power loss inside the card.

## 3. Settings persistence and a missing card

Settings live in RAM (`os::storage::Settings`). A change marks its store dirty. The system normally
writes dirty settings one second after the last change, on app suspend and on orderly runtime
shutdown. Apps generally let the system perform these writes.

If the card is absent or a write fails, `Flush()` returns false and changes remain dirty in RAM.
Background retries are spaced five seconds apart; the device also limits failed mount attempts to
once every five seconds. Before flushing settings to a newly available card, the system loads its
settings from the current `Settings/` paths and merges pending RAM edits: locally changed keys win,
and untouched card keys are preserved. This preparation also runs before an app is suspended.
A card inserted after boot can therefore supply settings from the current layout without being
replaced by startup defaults. Older storage locations are not searched or imported.

After successful card recovery, the OS applies device volume, key LED brightness and screen
backlight immediately. The Synth settings cache is also refreshed, but app preferences take effect the next time the app runs `Setup()`; inserting a card does not
replace the live project or its play controls.

There is no NVS fallback and no simulator `internal/` store. Pending changes disappear on reset or
power loss before a successful SD write. Project, preset, sample and session saves require a
writable card; a failed explicit save must not be reported as saved.

## 4. Settings ownership

`Settings/system.cfg`, owned by the OS:

| Key | Meaning |
|---|---|
| `protocol` | storage protocol version of the card: 2 |
| `sys.volume` | output volume 0–100 (perceptual: 0 mute, 1–100 ≈ −42–0 dB), default 85 |
| `sys.led` | key LED master brightness 0–15, default 15 |
| `sys.screen` | screen backlight 10–255, default 180 |
| `boot.app` | last non-system app; started at boot if still registered |

Volume, key LEDs and screen are device settings. The OS applies them, and apps change them through
`os::storage::SetOutputVolume`, `SetKeyLedBrightness` and `SetScreenBacklight`.

`Settings/synth.cfg` stores `keys.select` (0 ONE, 1 MULTI), the ROLL selection preference.
The keyboard, per-track chord and arpeggiator belong to the project: format 6 stores the keyboard
in the UI and KEYS lines, and one PLAYFX line per track. NEW resets these play settings; loading
or restoring a session applies its saved settings. Old preference keys may remain in the text file
and are preserved; they are not rewritten or migrated.

Projects also contain engines and sounds, sample file names, patterns and song arrangement, mixer
levels, mute/solo, tempo, swing and master effects. The UI line's device LED brightness, MASTER
volume and SYSTEM backlight fields remain for reading existing projects but are ignored on load.

## 5. App lifecycle and sessions

One app runs at a time. `os::system::SwitchApplication(id)` switches between frames: call the current
app's `Suspend()`, flush settings, destroy the app, then construct the new app and call `Setup()`.
Keys held across the switch are not delivered to the new app. Boot uses a valid non-system
`boot.app`, otherwise `Cardenza/Synth`. The HOME side button belongs to the running app.

The groovebox's `Suspend()` attempts to write `Session/session.ccproj` and `Session/session.cfg`.
The project uses format 6; the state file records `pending`, `name` and `dirty`. The next `Setup()`
consumes a pending session before applying it, so a session that fails to load is not retried at every
boot. An unsaved sampler take blocks the session save; when the card is writable, `session.cfg`
records `pending=0` and the reason in `skipped`.

Session persistence requires a successful write. Direct power removal or a reboot into the device's
Launcher does not imply that `Suspend()` or `Shutdown()` ran. Save projects explicitly before leaving
when the working state matters.

## 6. Format compatibility and older locations

The runtime uses only the current paths in section 1. Older `/Cardenza/` layouts and the original
singular `/CardenzaSynth/{Project,Preset,Sample}` folders are not automatically discovered,
imported, moved or deleted. Their files, including `.bak` and `.tmp` files, remain untouched.
Existing NVS records are also left untouched.

Format compatibility is separate from directory discovery: supported project versions, preset
format 1 and compatible WAV files can be read at the current paths. Unknown keys in current
settings files survive a read/write cycle. These guarantees do not import files or settings from
older locations.

Any required migration must be discussed with the user before it is implemented or run. There is
no automatic migration or import step at boot, on card insertion or during saving.

## 7. API and compatibility names

Use `OS/Storage/Storage.h` in namespace `os::storage`:

| API | Responsibility |
|---|---|
| `Root()` | `<card>/CardenzaSynth` |
| `SystemDirectory()`, `SystemSettingsPath()` | `Settings/`, `Settings/system.cfg` |
| `SharedDirectory()`, `SharedSamplesDirectory()` | Compatibility names for the root and `Samples/` |
| `AppFolder("Cardenza/Synth")` | Synth's root, `Settings/synth.cfg`, `Projects/`, `Presets/`, `Session/` |
| `AppFolder("Author/Name")` | Isolated folders for other valid app ids |
| `CardPresent()`, `CardCapacityBytes()`, `CardFreeBytes()` | Card availability and capacity |
| `EnsureSystemLayout()`, `EnsureAppLayout(...)` | Create the required directories on a writable card |
| `WriteFileAtomic(...)`, `ReadFileWithBackup(...)` | File replacement and interrupted-write fallback |
| `JoinPath`, `Exists`, `IsDirectory`, `IsFile`, `FileSize`, `ListDirectory` | Portable path and file helpers |

`OS/Storage/Settings.h` exposes `Settings`, `SystemSettings()`, `AppSettings(id)` and
`FlushAllSettings()`. `GetString`, `GetInt` and `GetBool` read with defaults; `SetString`, `SetInt`,
`SetBool` and `Remove` edit RAM. `Dirty()` and `Revision()` drive write-behind. `Flush()` succeeds only
when no pending change remains solely in RAM. `RefreshFromCard()` and
`RefreshAllSettingsFromCard()` preserve pending edits when loading the current card. `Load()` and
`ReloadAllSettings()` explicitly discard pending edits; do not use them for card insertion.

`Device::Storage` only provides the card root, mount state, capacity and free space. It has no
persistent settings-record API. Apps do not call it directly. Existing path helper names remain so
callers can adopt the new layout without duplicating literal paths.

## 8. Simulator and device behavior

- Firmware enables FAT long file names (`CONFIG_FATFS_LFN_HEAP`). Device startup does not initialise,
  read, write or erase NVS for this storage protocol.
- The desktop simulator uses `sdcard/CardenzaSynth/` with the same layout and missing-card behavior.
  `CardenzaSim::Input::SetCardInserted(false)` makes settings remain dirty in RAM.
- The browser simulator mounts `/sdcard` as IDBFS with `autoPersist`. It loads saved files before
  boot and flushes pending settings when the page is hidden. Its simulated card is present; if
  IndexedDB is unavailable, the page reports "memory only" and data does not survive page reloads.

## 9. Adding an app

1. Register a valid `Author/Name` id and factory.
2. Use `AppFolder` and `EnsureAppLayout` for paths; write only to the app's folders and the common
   `SharedSamplesDirectory()` when sharing samples.
3. Put settings in `AppSettings(id)`, use dotted keys with defaults/ranges, and let the system flush.
4. Check all file-write results. Keep data dirty in RAM when saving fails, and report failure for
   explicit saves. Version content formats separately from the storage layout.
5. Save sessions in `Suspend()` and restore them in `Setup()` when appropriate.
6. Change volume, key LEDs and screen only through `os::storage::Set…`.

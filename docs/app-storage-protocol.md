# App storage protocol (version 1)

Date: 2026-10-25. The device runs one app at a time. This firmware has no launcher of its own (choosing
apps belongs to the device's Launcher). Every app, built in or
written by a user, follows this protocol so that its projects and settings are where it expects them
after any app switch, and so that apps never touch each other's files. Code: `OS/Storage/` (API),
`OS/System/` (lifecycle), `Devices/Device.h` (`Device::Storage`, the low-level card).

## 1. Layout on the SD card

```text
<card>/Cardenza/
  System/settings.cfg          device settings, owned by the OS
  Shared/Samples/              audio any app may read and write
  Apps/<Author>/<Name>/        one folder per app id "Author/Name", owned by that app
    settings.cfg               the app's settings
    Projects/                  the app's documents
    Presets/<group>/           preset banks, one folder per group (an engine, an instrument)
    Session/                   working state handed over across an app switch
    <anything else>/           app-specific folders (AppFolder::Folder)
```

- `<card>` is `/sdcard` on the device, `<working directory>/sdcard` in the simulator (beside the
  executable on Windows) and `/sdcard` (IndexedDB) in the browser simulator.
- An app id is `Author/Name`; each part is 1–24 characters of `A–Z a–z 0–9 _ -`. Both parts are folder
  names, so the id is the app's address on the card. The built-in groovebox is `Cardenza/Synth`.
- Folder names of the protocol are 8.3-safe. File names inside an app folder are up to the app (FAT
  long names are enabled on the device; keep them ASCII).
- Ownership: only the OS writes `System/`. An app writes only its own `Apps/<Author>/<Name>/` and
  `Shared/`. Reading another app's folder is not part of the protocol.
- Samples are shared because they are raw material for every app. A file in `Shared/Samples` is
  referenced by its file name, so renaming or deleting one affects every project that names it.

## 2. Settings files

`settings.cfg` files are plain text, so they can be read and fixed on a computer:

```text
# Cardenza settings: Cardenza/Synth (storage protocol 1)
# key=value per line; unknown keys are kept
keys.scale=4
arp.latch=1
```

- One `key=value` per line. Lines starting with `#` (after optional spaces) are comments. Spaces
  around the key and the value are ignored; `\r\n` line ends are accepted.
- Keys: 1–48 characters of `a–z 0–9 _ .`, not starting with `.` or `_`. Use dotted groups
  (`keys.scale`, `arp.rate`, `sys.volume`).
- Values: printable ASCII, at most 200 characters, no leading or trailing space. Numbers are decimal
  integers, booleans `1` / `0` (reading also accepts `true/false`, `on/off`, `yes/no`). Enumerations
  are stored as indices that are only ever appended to.
- Tolerant reading: a malformed line is skipped, a repeated key keeps its first position and its last
  value, a value that is not a number or is out of range reads as the default. Keys the reader does
  not know are kept and written back unchanged, so a newer and an older firmware can share a card.
- Atomic writes: the new text goes to `<file>.tmp` and is closed first; the old file is renamed to
  `<file>.bak` (FAT cannot rename over a file) and the temporary renamed into place; then the backup
  is removed. An interruption leaves the old file, the backup or the new file; readers fall back to
  `<file>.bak` when the file itself is missing. The same scheme is available to apps for their own
  files (`WriteFileAtomic`, `ReadFileWithBackup`) and is what the groovebox's project files use.
- Size: a settings file is at most 16 KB.
- Version: `System/settings.cfg` holds `protocol=1`. A later protocol version that changes the layout
  or the format raises it and migrates (section 6).

### Write-behind and the missing card

Settings live in RAM (`os::storage::Settings`). A change marks the store dirty; the system writes
every dirty store one second after the last change, whenever an app is suspended, and on shutdown.
Apps never need to call `Flush()` themselves, but may.

When the card is missing or cannot be written, the store goes to internal flash instead ("kept on the
device"): NVS on the device (namespace `cardenza`, key `sys` for the system, `app<FNV-1a of the id>`
for apps), files in `<working directory>/internal/` in the simulator. The record holds the whole
settings file. When the card is back, the next flush (the system retries every ten seconds) writes
the card file and deletes the internal record. At start the internal record, when there is one, wins
key by key over the card file, because it only exists while it is newer. On the device a missing
card is probed at most every five seconds.

## 3. System settings

`System/settings.cfg`, owned by the OS:

| Key | Meaning |
|---|---|
| `protocol` | storage protocol version of the card (1) |
| `sys.volume` | output volume 0–100 (perceptual: 0 mute, 1–100 ≈ −42–0 dB), default 85 |
| `sys.led` | key LED master brightness 0–15, default 15 |
| `sys.screen` | screen backlight 10–255, default 180 |
| `boot.app` | last non-system app; started at boot when it is still installed |
| `migrate.legacy` | `done` or `partial`: result of moving the pre-protocol layout (section 6) |
| `migrate.left` | files left in the old folder (name conflicts, failures) |

Volume, key LEDs and screen are device settings: the OS applies them at boot and every app changes
them only through `os::storage::SetOutputVolume / SetKeyLedBrightness / SetScreenBacklight`. Projects
never carry them.

## 4. App lifecycle

- One app runs at a time. `os::system::SwitchApplication(id)` switches between frames: the running
  app's `Suspend()` is called, every settings store is flushed, the app is destroyed (so the next app
  gets the whole heap), then the new app is constructed and `Setup()` runs. Keys held during the
  switch are not delivered to the new app (neither press nor release).
- `Setup()` is the resume point: an app that saved a session in `Suspend()` restores it there and
  marks it used, so a stale session never returns after a power cut.
- Boot starts `boot.app` when it is registered and not a system app, else the default app
  (`Cardenza/Synth`).
- The HOME side button belongs to the running app (the groovebox stops everything on it).

## 5. API

`OS/Storage/Storage.h` (namespace `os::storage`):

```cpp
inline constexpr int kProtocolVersion = 1;
std::string Root(), SystemDirectory(), SystemSettingsPath(), SharedDirectory(), SharedSamplesDirectory(), AppsDirectory();
bool IsValidAppId(std::string_view id);
class AppFolder {                 // AppFolder("Author/Name"); paths are empty for an invalid id
  bool Valid() const; const std::string& Id() const;
  std::string Root() const, SettingsPath() const, Projects() const, Presets() const, Session() const;
  std::string Presets(std::string_view group) const;
  std::string Folder(std::string_view name) const;   // app-specific folder
};
bool CardPresent();               // mounts on demand
std::uint64_t CardCapacityBytes(), CardFreeBytes();
bool EnsureSystemLayout();
bool EnsureAppLayout(const AppFolder& app, const std::vector<std::string>& presetGroups = {});
std::string JoinPath(std::string_view directory, std::string_view name);
bool Exists(const std::string&), IsDirectory(const std::string&), IsFile(const std::string&);
long long FileSize(const std::string&);
bool CreateDirectories(const std::string&), RemoveEmptyDirectory(const std::string&);
std::vector<DirectoryEntry> ListDirectory(const std::string& path);   // sorted, without . and ..
bool WriteFileAtomic(const std::string& path, std::string_view contents);
bool ReadFileWithBackup(const std::string& path, std::string& contents, std::size_t maxBytes);
```

`OS/Storage/Settings.h`:

```cpp
class Settings {                  // one settings file
  void Load(); bool Loaded() const;
  bool Has(key) const;
  std::string GetString(key, fallback = {}) const;
  int GetInt(key, fallback, minimum = INT_MIN, maximum = INT_MAX) const;
  bool GetBool(key, fallback) const;
  bool SetString(key, value); bool SetInt(key, int); bool SetBool(key, bool); bool Remove(key);
  bool Dirty() const; bool PendingCardSync() const; std::uint32_t Revision() const;
  bool Flush();
};
Settings& SystemSettings();
Settings& AppSettings(std::string_view appId);
bool FlushAllSettings();          // the system calls this; apps rarely need to
std::vector<SettingsEntry> ParseSettings(std::string_view text, std::size_t* rejectedLines = nullptr);
std::string FormatSettings(const std::vector<SettingsEntry>& entries, std::string_view header);
```

`OS/Storage/DeviceSettings.h`: `ApplyDeviceSettings()`, `Set/OutputVolume`, `Set/KeyLedBrightness`,
`Set/ScreenBacklight`. `OS/Storage/Migration.h`: `MigrateLegacyLayout(const AppFolder&)`.

`OS/System/System.h` (namespace `os::system`): `Begin`, `RunFrame`, `ActiveApplication`,
`ActiveApplicationId`, `SwitchApplication(id)`, `BootApplicationId()`, `Shutdown()`, `kSettingsWriteDelayMs` (1000).

`Applications/Application.h`: `virtual void Suspend()` (default does nothing) next to `Setup`,
`Tick`, `UpdateLeds`, `RenderFrame` and the key handlers. Register an app with
`app::RegisterApplication({id, name, author, isSystem, factory})`.

`Devices/Device.h` (`Device::Storage`, the device layer only): `RootPath()` (card root), `Mount()`,
`IsMounted()`, `CapacityBytes()`, `FreeBytes()`, and the internal-flash records `ReadInternal`,
`WriteInternal`, `EraseInternal` (keys up to 15 characters of `a–z 0–9 _`, values up to 4000 bytes).
Apps never call these.

## 6. Moving the pre-protocol layout

Firmware before the protocol kept everything in `<card>/CardenzaSynth/`. At every boot, before the
first app starts, `MigrateLegacyLayout(AppFolder("Cardenza/Synth"))` moves what is there:

| Old | New |
|---|---|
| `CardenzaSynth/Project/*` | `Cardenza/Apps/Cardenza/Synth/Projects/` |
| `CardenzaSynth/Preset/<engine>/*` | `Cardenza/Apps/Cardenza/Synth/Presets/<engine>/` |
| `CardenzaSynth/Sample/*` (subfolders too) | `Cardenza/Shared/Samples/` |

- A folder whose target does not exist yet is renamed in one step; otherwise files move one by one by
  rename, or copy, compare and only then remove the original when rename fails.
- Nothing is overwritten. A file whose target already exists is removed only when both are
  byte-identical (an interrupted earlier run); a different file of the same name stays in the old
  folder and is counted in `migrate.left`.
- Sample file names do not change, so projects that name a sample still find it in `Shared/Samples`.
- Emptied folders are removed; `CardenzaSynth/` itself only when nothing else is left in it.
- Running it again only finishes what is left: it is idempotent, and costs one `stat` when the old
  folder is gone. The result is recorded as `migrate.legacy` / `migrate.left`.

## 7. The groovebox (`Cardenza/Synth`)

| What | Where |
|---|---|
| projects `<name>.ccproj` | `Apps/Cardenza/Synth/Projects/` |
| user preset banks `user.ccpreset` | `Apps/Cardenza/Synth/Presets/{DX7,Juno,Sampler,Drums}/` |
| samples (WAV) | `Shared/Samples/` |
| FN SETUP play settings | `Apps/Cardenza/Synth/settings.cfg` |
| session | `Apps/Cardenza/Synth/Session/session.ccproj` + `session.cfg` |
| volume, key LEDs, screen | `System/settings.cfg` (device settings) |

App settings keys (the FN SETUP slots of the same names): `keys.layout` (0 PIANO, 1 SCALE, 2
CHROMA), `keys.scale` (index into the scale table), `keys.root` (0–11), `keys.octave` (−3…3),
`keys.chord`, `keys.voicing`, `keys.lock`, `keys.velocity` (1–127), `arp.mode`, `arp.rate`,
`arp.octaves` (1–4), `arp.gate` (10–100), `arp.latch`. They are restored at start, written a second
after a change and on suspend, and loading or creating a project does not change them.

Projects keep project content only: engines and sounds, samples by file name, patterns and song,
mixer levels / mute / solo and the master effects. The file format stays version 4. Its UI line
(octave, root, scale, layout, LED brightness), `MASTER` and `SYSTEM` (backlight) are still written,
inside the ranges older firmware accepts, so older firmware opens the file; the groovebox ignores
them when it loads. Version 5 was only written by unreleased development builds (a `PLAY` line with
the play settings): such files still load, the `PLAY` line is skipped, and they are saved as
version 4. The next format change takes version 6.

Session: `Suspend()` saves the working project (format 4, atomic) with its name and unsaved state;
the next `Setup()` restores it and marks it used. An unsaved sampler take blocks the session save
exactly as it blocks SAVE: the session is then skipped (`session.cfg` records `pending=0` and the
reason) and the app starts with its play settings but an empty project.

## 8. Device notes

- FAT long file names are enabled (`CONFIG_FATFS_LFN_HEAP`); without them neither `CardenzaSynth`,
  `settings.cfg.bak` nor `<name>.ccproj` can exist on the card.
- NVS is initialised in `Device::Init` (erased and re-initialised when it is full or from a newer
  layout; it only holds settings copies).
- The browser simulator mounts `/sdcard` as IDBFS with `autoPersist`, so every write of this
  protocol reaches IndexedDB; the page also flushes settings when it is hidden. Its card is always
  present.

## 9. Writing an app

1. Pick an id `Author/Name` and register a factory.
2. Keep `AppFolder folder("Author/Name")`; call `EnsureAppLayout(folder, {your preset groups})`
   before writing, and write only below `folder` (and `SharedSamplesDirectory()` for audio).
3. Put settings in `AppSettings("Author/Name")` with dotted keys, read them with defaults and ranges,
   and let the system write them.
4. Save documents with `WriteFileAtomic` (or the same tmp / bak scheme) and read them with
   `ReadFileWithBackup`; version your own formats and keep reading old versions.
5. Implement `Suspend()` to keep the working state in `folder.Session()` and restore it in `Setup()`.
6. Change volume, key LEDs and screen only through `os::storage::Set…` so every app agrees on them.

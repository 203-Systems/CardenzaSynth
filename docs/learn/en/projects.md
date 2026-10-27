# Projects and storage {#projects}

A **project** is one song: the four tracks' engines and sounds, every pattern, the arrangement, the mix, track and master effects, BPM and swing. Projects live on the SD card; they are saved, loaded and created on the PROJECT and FILES pages of {{scene:SETUP}}.

| Kept in the project | Not in the project (saved automatically) |
|---|---|
| Engines, sounds, patterns, arrangement, mix, effects, BPM, swing | Play settings (keyboard, scale, chords, ARP), device settings (volume, lights, screen) |

<!-- keywords: project, file, save, storage, song file, 工程 -->

## Save, save as, new, open {#project-save}

{{strip
FN+S > save-name @footer | {{key:FN+S}} on any screen (or SAVE in the FN menu): saved, SAVED and the name show at the bottom. A project without a name gets the first free one: SONG 01, SONG 02…
hold ESC + G4 > setup-project @cells:1,1-1,4 | {{scene:SETUP}} {{page:PROJECT}}: SAVE, SAVE AS (type your own name), LOAD, NEW.
CTRL > setup-files @content | {{page:FILES}}: the projects on the card; {{key:ADD}} / {{key:SUB}} choose, {{key:OK}} loads.
}}

{{shot:setup-project|The PROJECT page of SETUP.
mark cell:1,1 = SAVE: save now
mark cell:1,2 = SAVE AS: save under a new name
mark cell:1,3 = LOAD: open the FILES list
mark cell:1,4 = NEW: start a new project
mark 108,74,46,10 = UNSAVED: changes not saved yet
}}

- With unsaved changes the PROJECT page says UNSAVED next to the name, and SAVE in the FN menu reads UNSAVED; creating or opening another project asks first ({{key:OK}} = yes, {{key:ESC}} = no).
- The FN menu on PROJECT and FILES has NEW, LOAD and SAVE AS. More in [[#setup-project]] and [[#setup-files]].
- A NEW project has the default four tracks: {{track:1}} {{track:2}} {{track:3}} {{track:4}}.
- Projects are written as format v4 (`.ccproj`); v1–v3 and the v5 of development builds also load. Saving writes a temporary file before replacing the old one. Check for SAVED before removing the card or powering off.

<!-- keywords: save, save project, how to save, save as, new project, open project, load project, unsaved, 保存工程 -->

## Resume a saved session {#project-session}

When the application is suspended or the runtime shuts down, it tries to save the working state (unsaved project changes included) as a session on the SD card. The next start restores a successfully saved session once. This needs a writable card and a saved or discarded sampler take. Cutting power or rebooting into the device's Launcher does not guarantee a session save; save the project first.

> [!tip] With a writable SD card, settings (volume, lights, keyboard, ARP and so on) save about a second after you change them; there is nothing to save by hand.

<!-- keywords: autosave, session, restore, resume, power off, 自动保存 -->

## Folders on the SD card {#storage-layout}

```text
<SD card>/CardenzaSynth/
  Settings/
    system.cfg               volume, lights, screen
    synth.cfg                keyboard, scale, chords, ARP
  Projects/*.ccproj          projects
  Presets/<engine>/          saved sounds (user.ccpreset)
  Samples/*.wav              sample files
  Session/                  saved session (session.ccproj + session.cfg)
```

The Synth uses only the folders shown above. Older `Cardenza/` and singular `CardenzaSynth/{Project,Preset,Sample}` folders stay untouched: their files are not automatically discovered, imported, moved or deleted. Any migration needs to be discussed separately before it is implemented or run.

<!-- keywords: folder, directory, path, sd card layout, where are my files, 目录 -->

## Settings files {#storage-settings}

`Settings/system.cfg` and `Settings/synth.cfg` are plain text, one `key=value` per line, readable and editable on a computer. For example, in `synth.cfg`:

```text
# Cardenza settings: Cardenza/Synth (storage protocol 2)
keys.scale=4
arp.latch=1
```

Reading is forgiving: broken lines are skipped and unknown keys are kept as they are. This preserves settings compatibility within these files; it does not make older storage locations discoverable.

<!-- keywords: settings file, config, cfg, text file, 设置文件 -->

## No card, or a bad card {#storage-card}

{{shot:setup-system|SETUP SYSTEM: the SD card state.
mark 8,66,150,10 = SD card: READY / no card
mark cells:1,1-1,3 = VOLUME · KEY LED · SCREEN
}}

- Without a writable SD card, changed device and app settings stay only in RAM. The system retries saving them when the card becomes available; they are lost if you power off first.
- Projects, sounds and samples need the SD card; saving without one reports an error.

<!-- keywords: no sd card, card missing, storage error, cannot save, sd card not found, 没有卡 -->

# Projects and storage {#projects}

A **project** is one song: the four tracks' engines and sounds, every pattern, the arrangement, the mix, track and master effects, BPM and swing. Projects live on the SD card.

| Kept in the project | Not in the project (saved automatically) |
|---|---|
| Engines, sounds, patterns, arrangement, mix, effects, BPM, swing | Play settings (keyboard, scale, chords, ARP), device settings (volume, lights, screen) |

<!-- keywords: project, file, save, storage, song file, 工程 -->

## Save, save as, new, open {#project-save}

{{strip
FN+S > save-name @8,40,222,30 | {{key:FN+S}}: save. A project without a name asks for one first (it suggests SONG).
OK > save-done @0,123,120,12 | {{key:OK}}: saved, SAVED shows at the bottom.
- > tape-project @cells:1,1-1,4 | {{scene:TAPE}} {{page:PROJECT}}: SAVE, SAVE AS, LOAD, NEW.
}}

{{shot:tape-project|The PROJECT page.
mark cell:1,1 = SAVE: save now
mark cell:1,2 = SAVE AS: save under a new name
mark cell:1,3 = LOAD: open the FILES list
mark cell:1,4 = NEW: start a new project
mark 100,85,48,10 = UNSAVED: changes not saved yet
}}

- With unsaved changes the status line says UNSAVED; creating or opening another project asks first.
- A NEW project has the default four tracks: {{track:1}} {{track:2}} {{track:3}} {{track:4}}.
- Projects are written as format v4 (`.ccproj`); v1–v3 and the v5 of development builds also load. Writing is atomic: a temporary file is written first and then replaces the old one, so a power cut never leaves a broken project.

<!-- keywords: save, save project, how to save, save as, new project, open project, load project, unsaved, 保存工程 -->

## Your work is kept {#project-session}

Before switching to another app or an orderly power-off, the working state (unsaved changes included) is stored as a session and restored just as it was when you come back. A session is used only once, so an old state never returns after a power cut.

> [!tip] Settings (volume, lights, keyboard, ARP and so on) reach the SD card about a second after you change them; there is nothing to save by hand.

<!-- keywords: autosave, session, restore, resume, power off, 自动保存 -->

## Folders on the SD card {#storage-layout}

```text
<SD card>/Cardenza/
  System/settings.cfg        device settings (volume, lights, screen)
  Shared/Samples/            samples shared by every app
  Apps/Cardenza/Synth/
    settings.cfg             this app's settings (keyboard, scale, chords, arp)
    Projects/                projects (.ccproj)
    Presets/<engine>/        sounds you saved
    Session/                 the working state across an app switch
```

Every app follows the same storage protocol: each app writes only its own folder and the shared sample folder. The folders of older firmware (`CardenzaSynth/Sample`, `CardenzaSynth/Project`) are moved to the new place automatically on the first start.

<!-- keywords: folder, directory, path, sd card layout, where are my files, 目录 -->

## Settings files {#storage-settings}

`settings.cfg` is plain text, one `key=value` per line, readable and editable on a computer:

```text
# Cardenza settings: Cardenza/Synth (storage protocol 1)
keys.scale=4
arp.latch=1
```

Reading is forgiving: broken lines are skipped and unknown keys are kept as they are (newer and older firmware can share a card).

<!-- keywords: settings file, config, cfg, text file, 设置文件 -->

## No card, or a bad card {#storage-card}

{{shot:setup-system|FN panel 5 SYSTEM: the SD card state.
mark 8,66,150,10 = SD card: READY / no card
mark cells:1,1-1,3 = VOLUME · KEY LED · SCREEN
}}

- Without an SD card, device and app settings are kept inside the device for now and written to the card automatically once it is in (retried every 10 seconds).
- Projects, sounds and samples need the SD card; saving without one reports an error.

<!-- keywords: no sd card, card missing, storage error, cannot save, sd card not found, 没有卡 -->

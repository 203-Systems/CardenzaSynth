# Sampling {#sampler}

{{icon:sample}} The SAMPLER engine ({{track:4}} by default) plays a WAV recording. Load samples from the SD card or record one with the built-in microphone. All apps share the `Cardenza/Shared/Samples` folder on the card.

Supported files: mono 16-bit PCM WAV, 8–96 kHz sample rate, up to 5 minutes. Short samples (up to about 186 ms, drum hits and the like) are kept in memory; longer ones play straight from the card.

<!-- keywords: sample, sampling, sampler, microphone, record audio, wav, field recording, vocals, 采样 -->

## Loading a sample {#sampler-load}

{{strip
hold TAB + G4, hold CTRL + G4 > instr-sampler-sample @cell:1,1 | In {{scene:INSTR}} switch to {{track:4}} and go to the SAMPLE page (hold {{key:CTRL}}, press G4).
G1 > sampler-files @0,13,240,10 | FILE opens the sample list; {{key:FN+F}} searches it.
OK > instr-sampler-sample @cells:1,3-1,4 | {{key:ADD}} / {{key:SUB}} choose, {{key:OK}} loads, {{key:ESC}} goes back.
}}

On every page of a SAMPLER track the FN menu also has SAMPLES (the list), RECORD and PREVIEW (plays the track's sample at its original pitch). Once loaded, the pads play the sample; ROOT decides which key plays it at its original pitch.

> [!tip] In the web simulator "Load WAV…" puts a WAV from your computer into the shared sample folder (see [[#web-storage]]).

<!-- keywords: load sample, import, open file, browse samples, search samples, wav file, 载入采样 -->

## Recording a sample {#sampler-record}

{{strip
G2 > sampler-rec @8,32,222,34 | REC on the SAMPLE page (or RECORD in the FN menu) opens the recorder; READY means you can start. Up to 60 seconds.
OK > sampler-rec @8,72,222,12 | {{key:OK}} starts, {{key:OK}} again stops. The input level shows while recording.
}}

- With a card the take goes straight to a file on it (`Session/take-<track>.wav` in this app's folder), safe on the card about once a second: a power cut loses at most the last second. A full card stops the recording and keeps what is there (CARD FULL). Without a card the take fits in free memory, only a few seconds.
- The microphone shares a clock pin with the speaker: while recording the speaker is silent; the sequencer keeps running.
- The microphone only runs while the recorder is on screen: leaving it (another scene or track) cancels the recording. {{key:ESC}} while recording cancels it too.
- An unsaved take survives a power cut or a switch to another app: when the app starts again the recorder opens with it (UNSAVED TAKE IS BACK).

> [!warn] An unsaved take is never lost by accident: REC asks RECORD AGAIN? before replacing it, and {{key:ESC}} in the trimmer asks DISCARD TAKE? ({{key:OK}} = yes, {{key:ESC}} = no).

<!-- keywords: record sample, microphone, mic, record audio, vocals, take, unsaved take, power cut, 录采样 -->

## Trimming and previewing {#sampler-trim}

After recording you are in the trimmer:

| Key | Does |
|---|---|
| {{key:G1}} / {{key:G2}} (or {{key:OK}}) | pick the start / end marker (the chosen one fully lit); {{key:ADD}} / {{key:SUB}} move it (hold {{key:FN}} for bigger steps) |
| {{key:G3}} or {{key:PLAY}} | preview |
| {{key:G4}} | SAVE: saves the take at once as the next free `TAKE_nn` and loads it on the track |

Later you can trim again at any time with START / END on the SAMPLE page:

{{shot:instr-sampler-sample|The SAMPLE page: markers and parameters.
mark cell:1,3 = START point
mark cell:1,4 = END point
mark cell:2,1 = ROOT note
mark cell:2,2 = TUNE: fine tune
mark cell:2,3 = LOOP
mark cell:2,4 = GAIN
}}

The TONE page has CUTOFF, RESO, ATTACK, RELEASE and LEVEL:

{{shot:instr-sampler-tone|SAMPLER TONE page.
mark cell:1,1 = CUTOFF
mark cell:1,2 = RESO
mark cell:1,3 = ATTACK
mark cell:1,4 = RELEASE
mark cell:2,1 = LEVEL
}}

<!-- keywords: trim, cut, start, end, marker, loop, preview, 裁剪 -->

## Saving, renaming, deleting {#sampler-save}

- Trimmer cell 4 (SAVE) saves the take as a WAV named `TAKE_01`, `TAKE_02`… (the first free number), without asking for a name.
- In the sample list {{key:FN+OK}} opens RENAME / DELETE for the file under the cursor. DELETE asks first and removes the WAV from the card.
- A sample another track is using cannot be deleted (FILE IN USE).
- Sample files belong to the shared folder, not to a project; a project only remembers which file it uses.

<!-- keywords: save sample, name, rename, delete sample, take name, 保存采样 -->

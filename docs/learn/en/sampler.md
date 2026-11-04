# Sampling {#sampler}

{{icon:sample}} The SAMPLER engine ({{track:4}} by default) plays a WAV recording. Load samples from the SD card or record one with the built-in microphone. All apps share the `Cardenza/Shared/Samples` folder on the card.

Supported files: mono 16-bit PCM WAV, 8–96 kHz sample rate.

<!-- keywords: sample, sampling, sampler, microphone, record audio, wav, field recording, vocals, 采样 -->

## Loading a sample {#sampler-load}

{{strip
hold TAB + G4, CTRL > instr-sampler-sample @cell:1,1 | In {{scene:INSTR}} switch to {{track:4}} and step with {{key:CTRL}} to the SAMPLE page.
G1 > sampler-files @0,13,240,10 | FILE opens the sample list; type to filter.
OK > instr-sampler-sample @cells:1,3-1,4 | {{key:ADD}} / {{key:SUB}} choose, {{key:OK}} loads. {{key:PLAY}} previews.
}}

On every page of a SAMPLER track the FN menu also has SAMPLES (the list), RECORD and PREVIEW. Once loaded, the pads play the sample; ROOT decides which key plays it at its original pitch.

> [!tip] In the web simulator "Load WAV…" puts a WAV from your computer into the shared sample folder (see [[#web-storage]]).

<!-- keywords: load sample, import, open file, browse samples, wav file, 载入采样 -->

## Recording a sample {#sampler-record}

{{strip
G2 > sampler-rec @0,13,240,10 | REC on the SAMPLE page opens the recorder; READY means you can start.
OK > sampler-rec @8,40,222,30 | {{key:OK}} starts, {{key:OK}} again stops. The level shows while recording.
}}

> [!warn] If an unsaved take (a draft) exists, it asks before replacing it, so nothing is lost by accident.

<!-- keywords: record sample, microphone, mic, record audio, vocals, 录采样 -->

## Trimming and previewing {#sampler-trim}

After recording you are in the trimmer:

| GRID | Does |
|---|---|
| {{key:G1}} / {{key:G2}} | pick the start / end marker, {{key:ADD}} / {{key:SUB}} move it (hold {{key:FN}} for bigger steps) |
| {{key:G3}} or {{key:PLAY}} | preview |
| {{key:G4}} | SAVE: name it and save |

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

- Trimmer cell 4 (SAVE) names the draft and saves it as a WAV. If the name exists it asks before overwriting.
- In the sample list {{key:FN+OK}} opens rename / delete.
- A sample another track is using cannot be deleted.
- Sample files belong to the shared folder, not to a project; a project only remembers which file it uses.

<!-- keywords: save sample, name, rename, delete sample, 保存采样 -->

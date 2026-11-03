# 工程与存储 {#projects}

一个**工程**是一首歌：四条轨道的音源和音色、所有 Pattern、编排、混音、每轨效果和总线效果、BPM 和 Swing、键盘设置，以及每条轨道的和弦和琶音。工程存在 SD 卡上，在 {{scene:SETUP}} 的 PROJECT 和 FILES 页保存、载入、新建。手上正在做的东西还会自动存在机器内部（见 [[#project-session]]）。

| 存在工程里 | 不在工程里 |
|---|---|
| 音源、音色、采样（按文件名）、Pattern、编排、混音（音量、静音、独奏、SEQ）、每轨和总线效果、BPM、Swing、键盘（KEYS）、和弦和琶音 | SELECT（应用偏好）、设备设置（音量、按键灯、屏幕）、LOOP / SONG、REC |

<!-- keywords: 工程, 存档, 保存, 文件, 项目, 存了什么, project, file, save, storage -->

## 保存、另存、新建、打开 {#project-save}

{{strip
FN+S > save-done @footer | 任何画面按 {{key:FN+S}}（或 FN 菜单的 SAVE）：马上存好，底部显示 SAVED 和名字。还没有名字的工程自动用第一个空着的名字：SONG 01、SONG 02……
hold ESC + G1 > setup-project @cells:1,1-1,3 | {{scene:SETUP}} {{page:PROJECT}}：SAVE、LOAD、NEW；SAVE AS（自己起名）在这一页的 FN 菜单里。
CTRL > setup-files @content | {{page:FILES}}：卡上的工程；{{key:ADD}} / {{key:SUB}} 选择，{{key:OK}} 载入。
}}

{{shot:setup-project|SETUP 的 PROJECT 页。
mark cell:1,1 = SAVE 保存
mark cell:1,2 = LOAD 打开 FILES 列表
mark cell:1,3 = NEW 新建
mark 8,69,224,18 = 工程名 · UNSAVED：有没保存的改动
}}

- 有未保存的改动时，PROJECT 页工程名旁边写着 UNSAVED，FN 菜单的 SAVE 也写着 UNSAVED；新建或打开别的工程前会先确认（{{key:OK}} 是，{{key:ESC}} 否）。
- PROJECT 和 FILES 页的 FN 菜单有 NEW、LOAD、SAVE AS。详见 [[#setup-project]] 和 [[#setup-files]]。
- NEW 的工程是默认的四轨 {{track:1}} {{track:2}} {{track:3}} {{track:4}}，演奏设置也是默认的（C 大调、没有和弦、琶音关闭）。
- 保存失败时底部会说明原因：SD CARD ERROR（没有卡或写不进去）、SAVE SAMPLE FIRST（有一段录音还没保存）、NO MEMORY。载入失败：INVALID PROJECT、SAMPLE MISSING（工程用到的采样不在 `Shared/Samples` 里）。
- 工程是纯文本文件，格式 7（`.ccproj`）；格式 1–6 也能读（6 之前的文件没有键盘、和弦和琶音，载入时保持现在的设置）。采样按文件名引用，在卡上改名或删除一个采样会影响所有用到它的工程。
- 写文件是原子的：先写临时文件再替换，保存时断电也不会坏掉。

<!-- keywords: 保存, 保存工程, 另存为, 新建工程, 打开工程, 未保存, 保存失败, 卡错误, save project, save as, new project, open project, unsaved, save failed -->

## 工作状态会被保留 {#project-session}

随时可以关机或直接拔电：当前的工作状态会自动存在机器的内部存储里（有没有卡都一样），下次开机原样回来。

- 内容和工程一样，另外还记着工程名和是不是 UNSAVED（所以载入别的工程时照样会先问）。回来时停在 T1。
- 在你停下来的时候写入：最后一次按键 1.5 秒后、没有按住的键、走带停止、没有在录音；如果一直在播放，最迟 60 秒写一次。切换到别的应用时马上写。
- 它不是工程：只有 SAVE 才会把有名字的工程写到卡上。想留下来的东西要保存，载入或新建别的工程之前也先保存。
- 写不进去时屏幕显示 AUTOSAVE FAILED: FN+S TO CARD。特别长的作品（超过约 12 KB）不带没在播放的 Pattern，屏幕显示 TOO LONG TO AUTOSAVE: FN+S TO CARD。两种情况都请用 {{key:FN+S}} 存到卡上。
- 还没保存的录音放在卡上（`Session/take-<轨道>.wav`）；重启后它会回来，屏幕显示 UNSAVED TAKE IS BACK。用到的采样所在的卡没插着时，插卡后自动载入。
- 如果连续三次没能正常启动，下一次启动会跳过一次工作状态。

> [!tip] 设置（SELECT、音量、灯、屏幕）改动后约 1 秒自动写到 SD 卡，不用手动保存。

<!-- keywords: 自动保存, 恢复, 断电, 拔电, 关机, 会话, 作品丢了, 自动保存失败, autosave, session, restore, resume, power off, power cut -->

## SD 卡上的目录 {#storage-layout}

```text
<SD 卡>/Cardenza/
  System/settings.cfg        设备设置（音量、灯、屏幕）
  Shared/Samples/            所有应用共用的采样
  Apps/Cardenza/Synth/
    settings.cfg             本应用的偏好（SELECT）
    Projects/                工程（.ccproj）
    Presets/<音源>/           自己保存的音色
    Session/                 还没保存的录音
```

所有应用都遵守同一套存储协议：每个应用只写自己的目录和共用采样目录。旧版本固件的目录（`CardenzaSynth/Project`、`Preset`、`Sample`）开机时自动搬到新位置。0.1.0 版的工程和采样（`CardenzaSynth/Projects`、`CardenzaSynth/Samples`）留在原处，照样出现在 FILES 和采样列表里；保存这样的工程会在 `Projects/` 里写一份。

<!-- keywords: 目录, 文件夹, 路径, sd 卡结构, folder, directory, path, sd card layout, where are my files -->

## 设置文件 {#storage-settings}

`settings.cfg` 是纯文本，一行一个 `key=value`，在电脑上也能看、能改：

```text
# Cardenza settings: Cardenza/Synth (storage protocol 1)
keys.select=1
```

读的时候很宽容：坏掉的行会跳过，不认识的键原样保留（新旧固件可以共用一张卡）。旧固件存在这里的键盘设置只在开机时读一次，不再写入：键盘现在属于工程。

<!-- keywords: 设置文件, 配置, 文本, settings file, config, cfg -->

## 没插卡或卡坏了 {#storage-card}

{{shot:setup-system|SETUP SYSTEM：SD 卡状态。
mark 8,66,150,10 = SD 卡：READY / NOT FOUND
mark cells:1,1-1,3 = 音量 · 按键灯 · 屏幕
}}

- 没有 SD 卡时，设备设置和应用设置先存在机器内部，插卡后（每 10 秒重试一次）自动写回卡上。工作状态本来就在内部存储里（见 [[#project-session]]）。
- 工程、音色和采样需要 SD 卡；没有卡时保存会提示 SD CARD ERROR。

<!-- keywords: 没有卡, 插卡, 存储卡, 读不到卡, 存不了, no sd card, card missing, storage error, cannot save -->

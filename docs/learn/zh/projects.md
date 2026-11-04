# 工程与存储 {#projects}

一个**工程**是一首歌：四条轨道的音源和音色、所有 Pattern、编排、混音、每轨效果和总线效果、BPM 和 Swing。工程存在 SD 卡上，在 {{scene:SETUP}} 的 PROJECT 和 FILES 页保存、载入、新建。

| 存在工程里 | 不在工程里（自动保存） |
|---|---|
| 音源、音色、Pattern、编排、混音、效果、BPM、Swing | 演奏设置（键盘、音阶、和弦、ARP）、设备设置（音量、灯、屏幕） |

<!-- keywords: 工程, 存档, 保存, 文件, 项目, project, file, save, storage -->

## 保存、另存、新建、打开 {#project-save}

{{strip
FN+S > save-name @footer | 任何画面按 {{key:FN+S}}（或 FN 菜单的 SAVE）：存好了，底部显示 SAVED 和名字。还没有名字的工程自动用第一个空着的名字：SONG 01、SONG 02……
hold ESC + G4 > setup-project @cells:1,1-1,4 | {{scene:SETUP}} {{page:PROJECT}}：SAVE、SAVE AS（自己起名）、LOAD、NEW。
CTRL > setup-files @content | {{page:FILES}}：卡上的工程；{{key:ADD}} / {{key:SUB}} 选择，{{key:OK}} 载入。
}}

{{shot:setup-project|SETUP 的 PROJECT 页。
mark cell:1,1 = SAVE 保存
mark cell:1,2 = SAVE AS 另存为
mark cell:1,3 = LOAD 打开 FILES 列表
mark cell:1,4 = NEW 新建
mark 108,74,46,10 = UNSAVED：有没保存的改动
}}

- 有未保存的改动时，PROJECT 页工程名旁边写着 UNSAVED，FN 菜单的 SAVE 也写着 UNSAVED；新建或打开别的工程前会先确认（{{key:OK}} 是，{{key:ESC}} 否）。
- PROJECT 和 FILES 页的 FN 菜单有 NEW、LOAD、SAVE AS。详见 [[#setup-project]] 和 [[#setup-files]]。
- NEW 的工程是默认的四轨：{{track:1}} {{track:2}} {{track:3}} {{track:4}}。
- 工程文件格式是 v4（`.ccproj`），能读 v1–v3 和开发版写的 v5。写文件是原子的：先写临时文件再替换，中途断电也不会坏掉。

<!-- keywords: 保存, 保存工程, 另存为, 新建工程, 打开工程, 未保存, save project, save as, new project, open project, unsaved -->

## 工作状态会被保留 {#project-session}

切换到别的应用或正常关机前，当前的工作状态（包括还没保存的改动）会存成 session；回来时原样恢复。session 用过一次就作废，断电后不会把很旧的状态带回来。

> [!tip] 设置（音量、灯、键盘、ARP 等）改动后约 1 秒自动写到 SD 卡，不用手动保存。

<!-- keywords: 自动保存, 恢复, 断电, 会话, autosave, session, restore, resume, power off -->

## SD 卡上的目录 {#storage-layout}

```text
<SD 卡>/Cardenza/
  System/settings.cfg        设备设置（音量、灯、屏幕）
  Shared/Samples/            所有应用共用的采样
  Apps/Cardenza/Synth/
    settings.cfg             本应用的设置（键盘、音阶、和弦、ARP）
    Projects/                工程（.ccproj）
    Presets/<音源>/           自己保存的音色
    Session/                 切换应用时的工作状态
```

所有应用都遵守同一套存储协议：每个应用只写自己的目录和共用采样目录。旧版本固件的目录（`CardenzaSynth/Sample`、`CardenzaSynth/Project`）第一次开机时会自动搬到新位置。

<!-- keywords: 目录, 文件夹, 路径, sd 卡结构, folder, directory, path, sd card layout, where are my files -->

## 设置文件 {#storage-settings}

`settings.cfg` 是纯文本，一行一个 `key=value`，在电脑上也能看、能改：

```text
# Cardenza settings: Cardenza/Synth (storage protocol 1)
keys.scale=4
arp.latch=1
```

读的时候很宽容：坏掉的行会跳过，不认识的键原样保留（新旧固件可以共用一张卡）。

<!-- keywords: 设置文件, 配置, 文本, settings file, config, cfg -->

## 没插卡或卡坏了 {#storage-card}

{{shot:setup-system|SETUP SYSTEM：SD 卡状态。
mark 8,66,150,10 = SD 卡：READY / 没有卡
mark cells:1,1-1,3 = 音量 · 按键灯 · 屏幕
}}

- 没有 SD 卡时，设备设置和应用设置先存在机器内部，插卡后（每 10 秒重试一次）自动写回卡上。
- 工程、音色和采样需要 SD 卡；没有卡时保存会提示失败。

<!-- keywords: 没有卡, 插卡, 存储卡, 读不到卡, 存不了, no sd card, card missing, storage error, cannot save -->

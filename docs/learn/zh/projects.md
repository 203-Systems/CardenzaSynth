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
- 工程文件格式是 v4（`.ccproj`），能读 v1–v3 和开发版写的 v5。保存时先写临时文件再替换旧文件；看到 SAVED 后再取卡或关机。

<!-- keywords: 保存, 保存工程, 另存为, 新建工程, 打开工程, 未保存, save project, save as, new project, open project, unsaved -->

## 恢复已保存的工作状态 {#project-session}

应用挂起或运行时正常关闭时，会尝试把当前工作状态（包括还没保存的工程改动）存成 SD 卡上的 session；下次启动恢复成功保存的 session，并只使用一次。这需要可写的 SD 卡，采样录音也要先保存或放弃。直接断电或重启到设备启动器不能保证保存 session，离开前先保存工程。

> [!tip] SD 卡可写时，设置（音量、灯、键盘、ARP 等）改动后约 1 秒自动保存，不用手动保存。

<!-- keywords: 自动保存, 恢复, 断电, 会话, autosave, session, restore, resume, power off -->

## SD 卡上的目录 {#storage-layout}

```text
<SD 卡>/CardenzaSynth/
  Settings/
    system.cfg               音量、灯、屏幕
    synth.cfg                键盘、音阶、和弦、ARP
  Projects/*.ccproj          工程
  Presets/<音源>/            保存的音色（user.ccpreset）
  Samples/*.wav              采样
  Session/                  会话（session.ccproj + session.cfg）
```

Synth 只使用上面列出的目录。旧 `Cardenza/` 和单数目录 `CardenzaSynth/{Project,Preset,Sample}` 保持原样，其中的文件不会被自动发现、导入、移动或删除。任何迁移都需要先单独商量，再决定是否实现或执行。

<!-- keywords: 目录, 文件夹, 路径, sd 卡结构, folder, directory, path, sd card layout, where are my files -->

## 设置文件 {#storage-settings}

`Settings/system.cfg` 和 `Settings/synth.cfg` 都是纯文本，一行一个 `key=value`，在电脑上也能看、能改。例如 `synth.cfg`：

```text
# Cardenza settings: Cardenza/Synth (storage protocol 2)
keys.scale=4
arp.latch=1
```

读的时候很宽容：坏掉的行会跳过，不认识的键原样保留。这保留了这些文件内部的设置兼容性，但不会自动识别旧存储目录。

<!-- keywords: 设置文件, 配置, 文本, settings file, config, cfg -->

## 没插卡或卡坏了 {#storage-card}

{{shot:setup-system|SETUP SYSTEM：SD 卡状态。
mark 8,66,150,10 = SD 卡：READY / 没有卡
mark cells:1,1-1,3 = 音量 · 按键灯 · 屏幕
}}

- 没有可写的 SD 卡时，设备设置和应用设置的改动只保留在 RAM 中。卡可用后会重试保存；在成功写入前断电，这些改动会丢失。
- 工程、音色和采样需要 SD 卡；没有卡时保存会提示失败。

<!-- keywords: 没有卡, 插卡, 存储卡, 读不到卡, 存不了, no sd card, card missing, storage error, cannot save -->

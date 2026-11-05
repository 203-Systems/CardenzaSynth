# 网页模拟器 {#web}

{{icon:web}} 没有机器时，可以直接在浏览器里用：[cardenza-synth.203.io](https://cardenza-synth.203.io/)（或 [cardenza-synth.vercel.app](https://cardenza-synth.vercel.app/)）。它不是仿造的界面，而是同一套固件代码（Groovebox 界面、AMY 音源、音序器、存储协议）编译成 WebAssembly：屏幕、按键灯、声音和机器上一样。

<!-- keywords: 模拟器, 网页版, 浏览器, 在线, 没有机器, simulator, emulator, web, browser, online, try -->

## 在说明书里跟着操作 {#web-live}

说明书里每张截图下面都有 **▶ 在这里跟着操作**：

| | |
|---|---|
| 1 | 点它，那张截图换成一台正在运行的设备（同一套固件代码），并自动走到截图里的画面（用按住 ESC / TAB / CTRL + GRID 切场景、轨道和页面，和你在机器上操作一样） |
| 2 | 点一下设备（黄色虚线框）后，电脑键盘就在操作它（键位见下）；点页面其它地方就不再操作它 |
| 3 | 「跳到这个画面」重新走到这一节的画面；「收起」把设备收起来 |

- 整个页面只有一台设备在运行：你最后点的那一节就是正在操作的那一台，别的节回到截图。换到另一节时设备直接搬过去，里面的状态不会丢。
- 切换中文 / English 时，设备跟着到同一张图的位置。

<!-- keywords: 跟着操作, 跟着做, 实机, 试一试, 在线操作, 交互, try it, live, follow along, interactive, hands on -->

## 连接真机（USB） {#web-usb}

{{icon:usb}} 用 USB 线把鸣片接到电脑上（Chrome 或 Edge）。不用点任何按钮：打开[入门教程](./)时浏览器会问能不能使用 MIDI 设备，选「允许」，之后机器一接上就自动连好。

- 在教程里，机器会跟着课程走：换到这一课的画面，要按的键会闪（其他键变暗）；做对了网页上会出现「学会了」，再多试几下，然后点「下一步」。
- 在这本说明书里，真机上按的每个键都会同时按在说明书里正在运行的那台设备上：键会送到你最后点开的那一节的设备；如果还没有打开，就在当前阅读位置的那一节自动打开。真机本身照常运行。
- 拔掉线就断开，插回去自动连上。
- 它通过 USB MIDI 和机器通信（Web MIDI，包括 SysEx）：不需要装驱动，USB MIDI 和 USB 音频照常可用。之前点了「阻止」的话，在地址栏左边的网站设置里把 MIDI 设备改成允许，再刷新。

<!-- keywords: usb, 连接, 真机, 实体设备, 串口, 数据线, 找不到设备, midi, web midi, sysex, 允许, connect, device, cable, web serial, hardware, real device -->

## 电脑键盘对应哪个键 {#web-keys}

{{icon:keyboard}} 电脑键盘按**物理位置**对应设备的 4×14 键盘，每个键帽上都标着对应的电脑键，按下时屏幕上的键会亮：

{{keymap:电脑键盘的四行就是设备的四行
r0c1-r0c12 = 数字行 1 … =
r1c1-r1c12 = Q 那一行
r2c1-r2c12 = A 那一行
r3c1-r3c12 = Z 那一行
ESC TAB FN CTRL = Esc · Tab · Caps Lock / 左 Shift · Ctrl
ADD SUB OK PLAY = Backspace · \ · Enter · 空格
}}

| 设备 | 电脑 |
|---|---|
| {{key:ESC}} {{key:TAB}} {{key:FN}} {{key:CTRL}} | Esc / Tab / Caps Lock 或左 Shift / Ctrl |
| {{key:ADD}} {{key:SUB}} {{key:OK}} {{key:PLAY}} | Backspace / `\` / Enter / 空格 |
| HOME 侧键 | Home |
| 没有字母的演奏键 | 右 Shift（第 3 行）· 左 / 右 Alt（第 4 行） |

- {{key:ESC}} 也可以用反引号键（1 左边那个）。macOS 上 Caps Lock 会锁住 {{key:FN}}（再按一次松开）；按住左 Shift 则是松手就放开的 {{key:FN}}。
- 也可以直接用鼠标或手指点屏幕上的键，触屏可以多点同时按。右键点一个键会把它锁住按下（方便按住 ESC、FN……再点 GRID），再右键一次松开。

<!-- keywords: 键盘对应, 键位映射, 电脑键盘, 右键, 锁住, keyboard mapping, computer keyboard, which key, key bindings, shortcuts, caps lock, right click -->

## 声音 {#web-audio}

{{icon:sound}} 浏览器要求先点一下页面才能出声，第一次请点任意位置或「Start audio」。声音由同一个音频引擎实时合成，延迟取决于浏览器和电脑。

<!-- keywords: 没声音, 声音, 音频, 开启声音, no sound, audio, start audio, sound not working -->

## 存储与采样 {#web-storage}

- 设置、工程和音色存在浏览器里（IndexedDB），目录结构和 SD 卡一样，刷新页面后还在。清除网站数据会把它们删掉。模拟器和说明书里的设备用的是同一份。
- 机器自动保留的工作状态（[[#project-session]]）浏览器也保留：边做边存，刷新或关闭页面后回来还在，和真机拔电一样。要留成一个工程，仍然用 {{key:FN+S}} 保存。
- 模拟器页面上的「Load project…」把鸣片卡上的 `.ccproj` 文件复制到浏览器里（不会覆盖已有工程，重名的自动加编号），并打开第一个；其余的在 SETUP FILES 里。
- 「Load WAV…」把电脑上的音频文件放进共用采样目录 `Shared/Samples`，之后在 SAMPLER 的文件列表里就能选到。不是单声道 16 位 WAV 的文件（立体声、24 位、MP3……）会转换成单声道 16 位 44.1 kHz，并截到 5 秒。

<!-- keywords: 浏览器存储, 自动保存, 刷新不丢, 导入 wav, 上传采样, 导入工程, 保存在哪, 刷新, browser storage, indexeddb, load wav, upload sample, load project, ccproj -->

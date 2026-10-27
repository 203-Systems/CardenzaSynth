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

{{icon:usb}} 用 USB 线把 Cardenza 接到电脑上，在说明书顶部点 **连接设备 USB**（Chrome 或 Edge），选择设备：之后在真机上按的每个键都会同时按在说明书里正在运行的那台设备上，一边看说明一边用真机按键操作。

- 键会送到你最后点开的那一节的设备；如果还没有打开，就在当前阅读位置的那一节自动打开。
- 真机本身照常运行，说明书里的设备只是跟着按。
- 再点一次按钮断开。只有支持 Web Serial 的浏览器（电脑上的 Chrome、Edge）会显示这个按钮。

<!-- keywords: usb, 连接, 真机, 实体设备, 串口, 数据线, connect, device, cable, web serial, hardware, real device -->

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

也可以直接用鼠标或手指点屏幕上的键。

<!-- keywords: 键盘对应, 键位映射, 电脑键盘, keyboard mapping, computer keyboard, which key, key bindings, shortcuts -->

## 声音 {#web-audio}

{{icon:sound}} 浏览器要求先点一下页面才能出声，第一次请点任意位置或「Start audio」。声音由同一个音频引擎实时合成，延迟取决于浏览器和电脑。

<!-- keywords: 没声音, 声音, 音频, 开启声音, no sound, audio, start audio, sound not working -->

## 存储与采样 {#web-storage}

- 设置、工程和音色存在浏览器里（IndexedDB），目录结构和 SD 卡一样，刷新页面后还在。清除网站数据会把它们删掉。
- 「Load WAV…」把电脑上的 WAV 放进采样目录 `CardenzaSynth/Samples`，之后在 SAMPLER 的文件列表里就能选到。

<!-- keywords: 浏览器存储, 导入 wav, 上传采样, 保存在哪, browser storage, indexeddb, load wav, upload sample -->

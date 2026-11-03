# 常见问题 {#faq}

<!-- keywords: 问题, 故障, 帮助, 排查, faq, help, problem, trouble, troubleshooting -->

## 没有声音 {#faq-no-sound}

按顺序检查：

{{strip
hold FN + 7 > fn-volume @14,94,212,28 | 主音量：按住 {{key:FN}}，用 {{key:7}} 调大（底部弹出 VOLUME）。
- > tape-mix @cells:2,1-3,4 | {{scene:TAPE}} {{page:MIX}}：这一轨是不是被静音（MUTE 亮）、别的轨是不是独奏（SOLO 亮）、推子是不是太低。
- > instr-juno-fx @cells:1,1-1,3 | {{scene:INSTR}} {{page:FX}}：FILTER 是不是拧到了一头，CRUSH / DRIVE 是不是太大。
}}

- SAMPLER 轨要先载入采样（见 [[#sampler-load]]）。
- 录采样的时候喇叭不出声（麦克风和它共用时钟），关掉录音画面就恢复。
- 网页模拟器要先点一下页面才能出声（见 [[#web-audio]]）。
- 还不行就用 {{key:FN+ESC}} 全部停止，再试。

<!-- keywords: 没声音, 声音太小, 听不到, 静音了, 喇叭, no sound, silent, too quiet, cannot hear, mute -->

## 有一个音一直响着停不下来 {#faq-stuck}

按住 {{key:FN}} 再按 {{key:ESC}}：全部停止会停下音序器、所有 ARP（包括 HOLD）和所有正在响的音。HOME 侧键在 Groovebox 里不起作用。

<!-- keywords: 卡音, 一直响, 停不下来, 长音, 全部停止, stuck note, hanging note, won't stop, drone, panic, latch, hold -->

## 灯为什么这样亮 {#faq-lights}

灯只有 {{led:off|灭}} {{led:half|半亮}} {{led:full|全亮}} {{led:blink|闪烁}} 四种意思（见 [[#lights-states]]）。按住 {{key:FN}} 时，只有配合 FN 有功能的键亮，其他键什么也不做。如果你觉得所有亮着的键都一样亮，在 {{scene:SETUP}} {{page:SYSTEM}} 选中 KEY LED，看演奏区的亮度梯（见 [[#lights-ramp]]）：每一列都应该不一样亮。

<!-- keywords: 灯不对, 全亮, 灯太亮, 灯太暗, 键不亮, lights wrong, all lit, too bright, too dim -->

## FN 面板去哪了 {#faq-settings}

{{key:FN}} 现在是当前画面的菜单（见 [[#fn-menu]]）；快捷调节 {{key:FN}} + {{key:6}} / {{key:7}} / {{key:8}} 和字母键照旧。设置搬到了这些地方：

- 键盘（布局、音阶、根音、八度、力度、锁定、SELECT ONE / MULTI）和设备（音量、按键灯、屏幕）：场景 {{scene:SETUP}} 的 KEYS 和 SYSTEM 页。按住 {{key:ESC}} 再按 GRID 第 1 列的任意一个键，或用每个 FN 菜单右下角的 SETUP（见 [[#setup]]）。
- BPM、Swing 和 PLAY LOOP / SONG：{{scene:SETUP}} {{page:PROJECT}}。
- 和弦与琶音器：每条轨道有自己的一套，在 {{scene:INSTR}} 的 CHORD 和 ARP 页（鼓轨没有；见 [[#play-chords]]）。
- Pattern 的长度：{{scene:SEQ}} {{page:TRACK}}（见 [[#seq-track]]）。

<!-- keywords: fn 面板, 设置在哪, 设置去哪了, 找不到设置, 设置, 和弦, 琶音, setup, fn panel, where are the settings, swing -->

## 我的东西存在哪？能直接拔电吗 {#faq-save}

能。工作状态（Pattern、音色、混音、编排、键盘、和弦与 ARP、速度）会自己存在设备内部的存储里，插不插卡都一样，开机时回来。停下按键一会儿、没有在播放时就写入；一直在播放时，至少每分钟写一次。不保留的：无感录制的 take，以及是否在播放。

想把一个版本存成文件就保存：{{key:FN+S}}（新工程自动起名 SONG 01、SONG 02……），或者用 {{scene:SETUP}} {{page:PROJECT}} 的 FN 菜单里的 SAVE AS 自己起名（见 [[#project-save]]）。工程存在 SD 卡的 `Cardenza/Apps/Cardenza/Synth/Projects`。设置会自动保存。

> [!warn] 特别长的工程（大约 2000 个音）放不下：不在播放的 Pattern 不进自动保存，屏幕提示 TOO LONG TO AUTOSAVE: FN+S TO CARD，请存到卡上。没插卡时录的采样只在内存里，断电就没了。

<!-- keywords: 丢失, 保存在哪, 自动保存, 断电, 拔电, 关机, 电池, 工程名, song 01, lost my work, where saved, autosave, power off -->

## 有撤销吗 {#faq-undo}

没有撤销。所以清除整个 Pattern、覆盖文件、新建工程这类操作一定会先确认（{{key:OK}} = 是，{{key:ESC}} = 否）。

> [!tip] 把好的版本存成另一个工程（SETUP PROJECT 的 FN 菜单里的 SAVE AS）或复制一份 Pattern（{{key:FN+D}}）再改，是最稳的做法。弹得好的段落还能从无感录制里找回来（见 [[#capture]]）。

<!-- keywords: 撤销, 后悔, 恢复, undo, redo, revert, go back -->

## 能接 MIDI 或电脑（USB）吗 {#faq-midi}

能。一根 USB 线，设备对电脑来说就是 MIDI 设备和声卡：

- **MIDI 输入**：电脑上的键盘或 DAW 弹各轨：通道 1–4 是轨道 1–4，其他通道弹当前轨。控制器（CC）交给那一轨的音源。
- **MIDI 输出**：设备弹出的音（按键和音序器）发给电脑，轨道 n 走通道 n，可以录下来。
- **音频**：电脑能录到设备的声音（在设备音量之前），电脑播放的声音从设备出来。

还没有 MIDI 时钟同步，也没有 MIDI 插口。在网页说明书里，同一根线还能让真机的按键同步过来（见 [[#web-usb]]）。

<!-- keywords: midi, usb midi, usb 音频, 声卡, 电脑, daw, 外接键盘, 同步, usb audio, external keyboard, sync, clock -->

## 长 Pattern 怎么用 {#faq-long}

一个 Pattern 有 1 到 256 步；GRID 一次显示 16 步，页码在右下角（如 `2/4`）。

- 长度：{{scene:SEQ}} {{page:TRACK}} 的 STEPS，或 SEQ 各页 FN 菜单里的 LEN − / LEN +（一次一整页）。
- 翻页：音符页上 {{key:FN+ADD}} / {{key:FN+SUB}}，或 FN 菜单里的 PAGE − / PAGE +。播放时 GRID 跟着播放头走。

每个 Pattern 记住自己的长度（见 [[#seq-track]]）。

<!-- keywords: 长 pattern, 超过 16 步, 32 步, 64 步, pattern 长度, 翻页, 步数, long pattern, pattern length -->

## 怎么按名字找音色 {#faq-search}

打开音色列表（{{scene:INSTR}} {{page:VOICE}} 上按 {{key:OK}}，或 FN 菜单的 PRESETS），按 {{key:FN+F}}：输入名字的一部分，按 {{key:OK}} 确认。列表里演奏区不打字，所以误按不会开始搜索。采样列表也一样（见 [[#instr-presets]]）。

<!-- keywords: 搜索, 找音色, 找采样, 过滤, 按名字, search, find preset, filter -->

## 怎么切到别的应用 {#faq-apps}

Groovebox 是设备上的一个应用。切换应用用设备自带的启动器；切走前的工作状态会保留，回来时恢复。

<!-- keywords: 应用, 启动器, 切换应用, 其它固件, app, launcher, switch app -->

## 弹的时候有爆音 {#faq-crackle}

音频预算很紧：很密的和弦加上很多效果时可能超出。少按几个音、关掉一两个效果（尤其是多个合唱和混响发送），或者降低 ARP 的速度。见 [[#fx-cpu]]。

<!-- keywords: 爆音, 破音, 卡顿, 杂音, crackle, glitch, pops, distortion, audio dropout -->

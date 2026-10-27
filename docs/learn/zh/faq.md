# 常见问题 {#faq}

<!-- keywords: 问题, 故障, 帮助, 排查, faq, help, problem, trouble, troubleshooting -->

## 没有声音 {#faq-no-sound}

按顺序检查：

{{strip
hold FN + 7 > fn-volume @14,102,212,26 | 主音量：按住 {{key:FN}}，用 {{key:7}} 调大（底部弹出 VOLUME）。
- > tape-mix @cells:2,1-3,4 | {{scene:TAPE}} {{page:MIX}}：这一轨是不是被静音（MUTE 亮）、别的轨是不是独奏（SOLO 亮）、推子是不是太低。
- > instr-juno-fx @cells:1,1-1,3 | {{scene:INSTR}} {{page:FX}}：FILTER 是不是拧到了一头，CRUSH / DRIVE 是不是太大。
}}

- SAMPLER 轨要先载入采样（见 [[#sampler-load]]）。
- 网页模拟器要先点一下页面才能出声（见 [[#web-audio]]）。
- 还不行就按 HOME 侧键（全部停止），再试。

<!-- keywords: 没声音, 声音太小, 听不到, 静音了, no sound, silent, too quiet, cannot hear, mute -->

## 有一个音一直响着停不下来 {#faq-stuck}

按 HOME 侧键（或 {{key:FN+ESC}}）：停止音序器并关掉所有声音，包括 ARP 的 LATCH。

<!-- keywords: 卡音, 一直响, 停不下来, 长音, stuck note, hanging note, won't stop, drone -->

## 灯为什么这样亮 {#faq-lights}

灯只有 {{led:off|灭}} {{led:half|半亮}} {{led:full|全亮}} {{led:blink|闪烁}} 四种意思（见 [[#lights-states]]）。如果你觉得所有亮着的键都一样亮，在 SETUP 的 5 SYSTEM 页选中 KEY LED，看演奏区的亮度梯（见 [[#lights-ramp]]）：每一列都应该不一样亮。

<!-- keywords: 灯不对, 全亮, 灯太亮, 灯太暗, lights wrong, all lit, too bright, too dim -->

## 我的东西存在哪？怎么才不会丢 {#faq-save}

工程要 {{key:FN+S}} 保存，存在 SD 卡的 `Cardenza/Apps/Cardenza/Synth/Projects`。设置会自动保存。切换应用或关机前的工作状态会自动保留（见 [[#project-session]]）。

<!-- keywords: 丢失, 保存在哪, 自动保存, lost my work, where saved, autosave -->

## 有撤销吗 {#faq-undo}

没有撤销。所以清除整个 Pattern、覆盖文件、新建工程这类操作一定会先确认。

> [!tip] 把好的版本存成另一个工程（SAVE AS）或复制一份 Pattern（{{key:FN+D}}）再改，是最稳的做法。弹得好的段落还能从无感录制里找回来（见 [[#capture]]）。

<!-- keywords: 撤销, 后悔, 恢复, undo, redo, revert, go back -->

## 能接 MIDI 键盘或同步别的设备吗 {#faq-midi}

现在还不能：没有外部 MIDI / USB 同步。所有演奏都在机器自己的键盘上。

<!-- keywords: midi, 外接键盘, 同步, usb midi, external keyboard, sync, clock -->

## 怎么切到别的应用 {#faq-apps}

Groovebox 是设备上的一个应用。切换应用用设备自带的启动器；切走前的工作状态会保留，回来时恢复。

<!-- keywords: 应用, 启动器, 切换应用, 其它固件, app, launcher, switch app -->

## 弹的时候有爆音 {#faq-crackle}

音频预算很紧：很密的和弦加上很多效果时可能超出。少按几个音、关掉一两个效果（尤其是多个合唱和混响发送），或者降低 ARP 的速度。见 [[#fx-cpu]]。

<!-- keywords: 爆音, 破音, 卡顿, 杂音, crackle, glitch, pops, distortion, audio dropout -->

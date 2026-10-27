# 1 INSTR 乐器 {#instr}

INSTR 管当前轨的声音：用哪个音源（引擎）、哪个音色、这个音源的所有参数和这一轨的效果。四条轨道各用一个音源，每条轨道记住自己停在哪一页。

| 音源 | 默认轨道 | 页面（{{key:CTRL}} 翻） |
|---|---|---|
| DRUMS 鼓机 | {{track:1}} | VOICE、KIT、FX |
| JUNO 模拟合成器 | {{track:2}} | VOICE、DCO、VCF、ENV、LFO、FX |
| DX7 FM 合成器 | {{track:3}} | VOICE、ALGO、LEVELS、OP1–OP6、LFO、FX |
| SAMPLER 采样器 | {{track:4}} | VOICE、SAMPLE、TONE、FX |

<!-- keywords: 乐器, 音源, 合成器, 音色, 引擎, instrument, synth, synthesizer, engine, sound, patch -->

## VOICE：音源和音色 {#instr-voice}

每个音源的第一页：大字是音色名，下面是实时键盘。

{{shot:instr-voice|INSTR VOICE（T2 Juno）。
mark cell:1,1 = ENGINE 换音源
mark cell:1,2 = PRESET 浏览
mark cell:1,3 = SAVE 保存
mark cell:1,4 = SAVE AS 另存
mark 8,40,200,26 = 音色名
mark 8,66,120,8 = 音源 · 音阶 · 八度
mark 8,75,222,44 = 实时键盘
key ADD SUB = 下一个 / 上一个音色
key OK = 音色浏览
key G1-G4 = ENGINE PRESET SAVE SAVE AS
}}

- {{key:ADD}} / {{key:SUB}}：直接切到下一个 / 上一个音色，马上可以试听。
- {{key:OK}}：打开音色浏览。

换音源会把这条轨道换成新音源的默认音色；同一轨的音序不受影响。

{{strip
G1 > engine-picker @cells:2,1-2,4 | ENGINE：四个音源，当前的写着 NOW。
ADD > instr-voice @8,40,200,26 | {{key:ADD}} / {{key:SUB}} 在 VOICE 页直接换音色。
}}

<!-- keywords: 换音色, 下一个音色, 音源, 引擎, 选择合成器, voice, preset, next sound, change sound, engine picker -->

## 音色浏览与保存 {#instr-presets}

{{strip
G2 > preset-browser @0,13,240,10 | PRESET：音色列表，右上角是「第几个 / 总数」。
B, A, S, S > preset-filtered @0,13,240,10 | 直接在键盘上打字过滤（例如 `BASS`）。
OK > instr-voice @8,40,200,26 | {{key:ADD}} / {{key:SUB}} 移动，{{key:OK}} 载入。
}}

- {{key:ESC}} 先清空过滤，再按一次关闭。
- {{key:FN}} + {{key:;}} {{key:.}} 上下移动，{{key:FN+ADD}} / {{key:FN+SUB}} 一次翻一页。
- SAVE 覆盖保存当前音色（自己的音色），SAVE AS 起个新名字另存。自己的音色存在 SD 卡上本应用的 Presets 目录里。

<!-- keywords: 音色库, 预设, 浏览, 搜索音色, 保存音色, 另存, preset, presets, browse, search, save sound, patch library -->

## 参数页 {#instr-params}

音源的其它页面都是参数页：**GRID 上每个键直接选中一个参数**，{{key:ADD}} / {{key:SUB}} 调值（按住连发），{{key:FN+OK}} 恢复默认。开关和选项型参数再按一次同一个键就切换。

{{shot:instr-juno-vcf|Juno VCF 页：上面两行是参数，下面画着滤波曲线。
mark cell:1,1 = 焦点参数：白底
mark cell:1,2 = RESO
mark cell:1,3 = ENV
mark cell:1,4 = POL
mark cells:3,1-4,4 = 这一组的图：滤波曲线
key G1 = 焦点：全亮
key G2-G6 = 参数：半亮
key G9-G16 = 空格：灭
key ADD SUB = 调值
}}

没有参数的格子画着这一组的图：包络、滤波曲线、算法、波形、示波器……每个参数的范围、默认值和全部选项见下面各音源的参考表。

<!-- keywords: 参数, 调参数, 旋钮, 编辑音色, 恢复默认, parameter, parameters, edit sound, knob, default, reset -->

## FX 页：这一轨的效果 {#instr-fx}

每个音源最后一页都是 FX：

{{shot:instr-juno-fx|FX 页和信号链。
mark cell:1,1 = DRIVE 饱和
mark cell:1,2 = CRUSH 降采样
mark cell:1,3 = FILTER DJ 滤波
mark cell:1,4 = RESO
mark cell:2,1 = CHORUS
mark cell:2,2 = DELAY 发送
mark cell:2,3 = REVERB 发送
mark cells:3,1-4,4 = 信号链：IN → … → OUT
}}

这些是音色的一部分，随音色和工程保存。详见 [[#effects]]。

<!-- keywords: 效果, 失真, 降采样, 滤波, 合唱, 延迟, 混响, fx, effects, drive, distortion, bitcrush, filter, chorus, delay, reverb -->

## DRUMS 鼓机 {#engine-drums}

八个鼓：底鼓、军鼓、闭镲、开镲、拍手、通鼓、牛铃、沙锤，每个在演奏区占一列。KIT 页调每个鼓的音量和总音量。

{{shot:instr-drums-kit|DRUMS KIT 页：八个鼓的音量，右下画着鼓垫。
mark cells:1,1-2,4 = 八个鼓的音量
mark cell:3,1 = MASTER 总音量
mark cells:3,2-4,4 = 鼓垫（按下时亮）
}}

{{ref:engine:DRUMS}}

<!-- keywords: 鼓机, 鼓组, 808, 底鼓音量, drums, drum machine, kit, kick level -->

## JUNO 模拟合成器 {#engine-juno}

经典减法合成：DCO 振荡器 → VCF 滤波器 → VCA 包络，加上 LFO 和 Juno 合唱。

{{strip
CTRL > instr-juno-dco @cells:3,1-4,4 | **DCO**：RANGE、SAW、PULSE、PWM、PW SRC、SUB、NOISE、LFO，附示波器。
CTRL > instr-juno-vcf @cells:3,1-4,4 | **VCF**：CUTOFF、RESO、ENV、POL、LFO、KBD，附滤波曲线。
CTRL > instr-juno-env @cells:3,1-4,4 | **ENV**：ADSR、VCA 模式（ENV / GATE）、LEVEL，附包络图。
CTRL > instr-juno-lfo @cells:3,1-4,4 | **LFO**：RATE、DELAY（延迟渐入），附波形。
}}

Juno 预设自带的 Chorus I / II 映射到 FX 页的 CHORUS。

{{ref:engine:JUNO}}

<!-- keywords: juno, 模拟, 减法合成, 振荡器, 滤波器, 包络, analog, subtractive, oscillator, dco, vcf, envelope, adsr, lfo -->

## DX7 FM 合成器 {#engine-dx7}

六算子 FM 合成，兼容 DX7 音色：

{{strip
CTRL > instr-dx7-algo @cells:3,1-4,4 | **ALGO**：32 种算法、反馈、移调，附算法图。
CTRL > instr-dx7-levels @cells:3,1-4,4 | **LEVELS**：六个算子的输出电平。实心是能听到的载波，点阵是调制器。
CTRL > instr-dx7-op1 @cell:2,3 | **OP1–OP6**：OUT、CRS、FINE、DTN、MODE、AMOD、R1–R4 / L1–L4 包络，CAR / MOD 标记。
CTRL > instr-dx7-lfo @cells:3,1-4,4 | **LFO**：SPEED、WAVE、PMD、AMD、P SENS，附波形。
}}

{{ref:engine:DX7}}

<!-- keywords: dx7, fm, 调频合成, 算子, 算法, 电钢琴, operator, algorithm, frequency modulation, epiano -->

## SAMPLER 采样器 {#engine-sampler}

播放一段 WAV：SAMPLE 页选文件、录音、裁剪、根音、循环、增益（附波形）；TONE 页是滤波、包络和电平。完整流程见 [[#sampler]]。

{{shot:instr-sampler-sample|SAMPLER SAMPLE 页（还没有采样）。
mark cell:1,1 = FILE 载入
mark cell:1,2 = REC 录音
mark cell:1,3 = START
mark cell:1,4 = END
mark cells:2,1-2,4 = ROOT · TUNE · LOOP · GAIN
}}

{{ref:engine:SAMPLER}}

<!-- keywords: 采样器, 采样, wav, 波形, 裁剪, 循环, sampler, sample, waveform, trim, loop -->

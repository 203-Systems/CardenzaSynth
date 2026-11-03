# 效果器 {#effects}

每条轨道有自己的插入效果和两路发送；发送汇到两条总线（延迟、混响），最后经过总线 EQ、压缩和磁带饱和输出。

```text
引擎 → DRIVE → CRUSH → FILTER → CHORUS → 轨道推子 ─┬─→ 输出
                                                  ├─ DELAY 发送 ─→ 总线延迟 ─┐
                                                  └─ REVERB 发送 → 总线混响 ─┴→ EQ → COMP → TAPE → 输出
```

三个地方调效果：

{{strip
- > instr-juno-fx @cells:1,1-2,4 | {{scene:INSTR}} {{page:FX}}：这一轨的插入效果和两路发送。
- > tape-sends @cells:1,1-4,4 | {{scene:TAPE}} {{page:SENDS}}：四轨的 FILTER、CHORUS 和两路发送一屏看完。
- > tape-fx @cells:1,1-3,4 | {{scene:TAPE}} {{page:FX}}：总线延迟、混响、EQ、压缩、磁带。
}}

<!-- keywords: 效果器, 效果链, 信号链, effects, fx, signal chain, routing -->

## 轨道效果（插入） {#fx-inserts}

在每个音源的 FX 页（见 [[#instr-fx]]）；四轨的 FILTER 和 CHORUS 也在 TAPE 的 SENDS 页上：

{{shot:instr-juno-fx|FX 页：上面是效果参数，下面是这一轨的信号链。
mark cell:1,1 = DRIVE：软饱和，越大越脏
mark cell:1,2 = CRUSH：降采样 + 降位深
mark cell:1,3 = FILTER：64 为关，往下低通、往上高通
mark cell:1,4 = RESO：滤波共振
mark cell:2,1 = CHORUS：Juno 式 I / II / I+II
mark cells:2,2-2,3 = DELAY / REVERB 发送
mark cells:3,1-4,4 = 信号链：开着的效果亮起
}}

这些是音色参数（`fx.*`），随音色和工程保存；换音源或换音色时回到默认（Juno 音色带着自己的合唱）。

<!-- keywords: 失真, 饱和, 降采样, 低保真, 滤波, 共振, 合唱, drive, distortion, saturation, bitcrush, lofi, filter, resonance, chorus -->

## 发送：延迟和混响 {#fx-sends}

每轨两个发送量：DELAY 和 REVERB。发送到总线后，总线的返回电平（RETURN）决定整体有多少。

{{shot:tape-sends|SENDS：每列一条轨道。轨道发声且发送打开时，那一格的右边缘会发光。
mark cells:1,1-1,4 = FILTER 滤波（FLT）
mark cells:2,1-2,4 = CHORUS 合唱（CHO）
mark cells:3,1-3,4 = DELAY 发送（DLY）
mark cells:4,1-4,4 = REVERB 发送（REV）
key G1-G16 = 焦点：全亮 · 开着：半亮 · 关着：灭
}}

<!-- keywords: 发送, 延迟, 回声, 混响, 空间感, send, delay, echo, reverb, space, ambience -->

## 总线效果 {#fx-master}

TAPE 的 FX 页，属于工程（工程文件 v4 起保存）：

{{shot:tape-fx|总线效果 FX。
mark cells:1,1-1,4 = DELAY：按 BPM 同步的磁带延迟
mark cells:2,1-2,4 = REVERB：4 线反馈延迟网络，偏暗
mark cell:3,1 = COMP：总线压缩
mark cells:3,2-3,3 = EQ：低 / 高搁架 ±12 dB
mark cell:3,4 = TAPE：磁带饱和
mark cells:4,1-4,4 = 当前速度下的回声和混响衰减
}}

- **DELAY**：TIME 是音符时值，1/32 到 1/2（默认 1/8D），跟 BPM 同步。反馈环里有 TONE 低通和 80 Hz 高通；换时值时像磁带一样滑音。743 毫秒以内的回声用 16 KB 的延迟线；更长的时值换成 32 KB 的线（最长 1.49 秒），放不下就把时值折半直到放得下。
- **REVERB**：带宽约 4.5 kHz，偏暗。
- 两路发送先抽取到 11.025 kHz 再处理，所以延迟和混响听起来偏暖、偏 lo-fi。

<!-- keywords: 总线, 主输出, 磁带延迟, 混响, 均衡, 压缩, master, bus, tape delay, reverb, eq, compressor, saturation -->

## 内存和 CPU {#fx-cpu}

板子没有 PSRAM，效果器的缓冲只在第一次用到时分配：

| 部分 | 内存 |
|---|---|
| 效果核心 | 约 4.6 KB |
| 每条带效果的轨道 / 每个合唱 | 1 KB / 1 KB |
| 延迟 | 16 KB（超过 743 毫秒的回声 32 KB） |
| 混响 | 约 8.8 KB |
| 全开 | 约 35 KB |

- 延迟或混响分配不到内存时那条总线静音，FX 页和信号链图上显示 NO MEM。全部效果关闭时，输出和没有效果器时逐位相同。
- 音频每 256 帧的预算是 5.8 毫秒。很多 DX7 音加上效果就接近上限。超出时丢掉一个声部（先丢已松开的，再丢最老的，最后才丢鼓），节拍不会停。

> [!warn] 很密的和弦叠加很多效果时可能出现爆音：先减少同时发声的音，或关掉一两个效果。

<!-- keywords: 内存, cpu, 性能, 爆音, 卡顿, 没内存, memory, performance, crackle, glitch, no mem, dropout -->

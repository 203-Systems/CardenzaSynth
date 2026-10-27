# 采样 {#sampler}

{{icon:sample}} SAMPLER 音源（默认 {{track:4}}）播放一段 WAV 录音。采样可以从 SD 卡载入，也可以直接用机器上的麦克风录。所有应用共用 SD 卡上的 `Cardenza/Shared/Samples` 目录。

支持的文件：单声道 16 位 PCM WAV，采样率 8–96 kHz。

<!-- keywords: 采样, 录音, 麦克风, 人声, wav, sample, sampling, microphone, record audio, field recording -->

## 载入采样 {#sampler-load}

{{strip
hold TAB + G4, CTRL > instr-sampler-sample @cell:1,1 | {{scene:INSTR}} 切到 {{track:4}}，{{key:CTRL}} 翻到 SAMPLE 页。
G1 > sampler-files @0,13,240,10 | 按 FILE 打开采样列表，可以打字过滤。
OK > instr-sampler-sample @cells:1,3-1,4 | {{key:ADD}} / {{key:SUB}} 选择，{{key:OK}} 载入。{{key:PLAY}} 预听。
}}

载入后演奏区就用这段采样弹，根音（ROOT）决定哪个键是原始音高。

> [!tip] 在网页模拟器里可以用「Load WAV…」把电脑上的 WAV 放进共用采样目录（见 [[#web-storage]]）。

<!-- keywords: 载入采样, 打开文件, 导入, wav 文件, load sample, import, open file, browse samples -->

## 录一段采样 {#sampler-record}

{{strip
G2 > sampler-rec @0,13,240,10 | SAMPLE 页按 REC：打开录音窗口，READY 表示可以录了。
OK > sampler-rec @8,40,222,30 | {{key:OK}} 开始录，再按 {{key:OK}} 停止。录的时候能看到电平。
}}

> [!warn] 如果有一段还没保存的录音（草稿），会先问是否替换，避免误删。

<!-- keywords: 录采样, 录麦克风, 录人声, record sample, microphone, mic, record audio -->

## 裁剪与试听 {#sampler-trim}

录完后进入裁剪：

| GRID | 作用 |
|---|---|
| {{key:G1}} / {{key:G2}} | 选开始 / 结束标记，{{key:ADD}} / {{key:SUB}} 移动（按住 {{key:FN}} 加快） |
| {{key:G3}} 或 {{key:PLAY}} | 试听 |
| {{key:G4}} | SAVE：起名保存 |

之后随时可以在 SAMPLE 页用 START / END 参数再裁剪：

{{shot:instr-sampler-sample|SAMPLE 页：标记和参数。
mark cell:1,3 = START 起点
mark cell:1,4 = END 终点
mark cell:2,1 = ROOT 根音
mark cell:2,2 = TUNE 微调
mark cell:2,3 = LOOP 循环
mark cell:2,4 = GAIN 增益
}}

TONE 页是 CUTOFF、RESO、ATTACK、RELEASE、LEVEL：

{{shot:instr-sampler-tone|SAMPLER TONE 页。
mark cell:1,1 = CUTOFF
mark cell:1,2 = RESO
mark cell:1,3 = ATTACK
mark cell:1,4 = RELEASE
mark cell:2,1 = LEVEL
}}

<!-- keywords: 裁剪, 剪切, 起点, 终点, 循环, 试听, trim, cut, start, end, loop, preview -->

## 保存、改名与删除 {#sampler-save}

- 裁剪窗口第 4 格（SAVE）给草稿起名并保存成 WAV。名字已存在时会问是否覆盖。
- 采样列表里 {{key:FN+OK}} 打开改名 / 删除。
- 被其它轨道正在用的采样不能删除。
- 采样文件属于共用目录，不属于某个工程；工程只记录用的是哪个文件。

<!-- keywords: 保存采样, 命名, 改名, 删除采样, save sample, rename, delete sample -->

# 采样 {#sampler}

{{icon:sample}} SAMPLER 采样器（默认 {{track:4}}）播放一段 WAV 录音。可以从 SD 卡载入，也可以用内置麦克风录。所有应用共用卡上的 `Cardenza/Shared/Samples` 目录。

支持的文件：单声道 16 位 PCM WAV，采样率 8–96 kHz，最长 5 分钟。短采样（约 186 ms 以内，鼓声之类）放在内存里，更长的直接从卡上边读边播。

<!-- keywords: 采样, 采样器, 麦克风, 录音, 人声, 实地录音, sample, sampling, field recording, sampler, microphone, record audio, wav -->

## 载入采样 {#sampler-load}

{{strip
hold TAB + G4, hold CTRL + G4 > instr-sampler-sample @cell:1,1 | 在 {{scene:INSTR}} 切到 {{track:4}}，进 SAMPLE 页（按住 {{key:CTRL}} 按 G4）。
G1 > sampler-files @0,13,240,10 | FILE 打开采样列表；{{key:FN+F}} 搜索。
OK > instr-sampler-sample @cells:1,3-1,4 | {{key:ADD}} / {{key:SUB}} 选择，{{key:OK}} 载入，{{key:ESC}} 返回。
}}

SAMPLER 轨的每一页，FN 菜单里还有 SAMPLES（列表）、RECORD（录音）和 PREVIEW（按原音高试听这一轨的采样）。载入后演奏区就弹这个采样；ROOT 决定哪个键按原音高播放。

> [!tip] 网页模拟器里「Load WAV…」可以把电脑上的 WAV 放进共用采样目录（见 [[#web-storage]]）。

<!-- keywords: 载入采样, 导入, 打开文件, 浏览采样, 搜索采样, wav 文件, load sample, import, open file, browse samples -->

## 录一段采样 {#sampler-record}

{{strip
G2 > sampler-rec @8,32,222,34 | SAMPLE 页的 REC（或 FN 菜单的 RECORD）打开录音界面；显示 READY 就可以开始。最长 60 秒。
OK > sampler-rec @8,72,222,12 | {{key:OK}} 开始，再按 {{key:OK}} 停止。录音时显示输入电平。
}}

- 有卡时直接录进卡上的文件（本应用目录下的 `Session/take-<轨道>.wav`），大约每秒落盘一次：断电最多丢最后一秒。卡满会停止录音并保留已录的部分（CARD FULL）。没有卡时放进剩余内存，只能录几秒。
- 麦克风和喇叭共用一个时钟引脚：录音时喇叭静音，音序器照常走。
- 麦克风只在录音界面开着时工作：离开界面（换场景、换轨）就取消这次录音。录音中按 {{key:ESC}} 也是取消。
- 没保存的录音断电或切到别的应用也不会丢：再打开时录音界面带着它回来（UNSAVED TAKE IS BACK）。

> [!warn] 没保存的录音不会被误丢：REC 会先问 RECORD AGAIN? 再替换，裁剪界面按 {{key:ESC}} 会问 DISCARD TAKE?（{{key:OK}} = 是，{{key:ESC}} = 否）。

<!-- keywords: 录采样, 录麦克风, 录人声, 麦克风, 录音, 人声, 草稿, 未保存的录音, 断电, record sample, microphone, mic, take -->

## 裁剪与试听 {#sampler-trim}

录完就进入裁剪界面：

| 键 | 作用 |
|---|---|
| {{key:G1}} / {{key:G2}}（或 {{key:OK}}） | 选起点 / 终点标记（选中的全亮）；{{key:ADD}} / {{key:SUB}} 移动（按住 {{key:FN}} 大步） |
| {{key:G3}} 或 {{key:PLAY}} | 试听 |
| {{key:G4}} | SAVE：马上存成下一个空着的 `TAKE_nn`，并载入这一轨 |

之后随时可以在 SAMPLE 页用 START / END 重新裁剪：

{{shot:instr-sampler-sample|SAMPLE 页：标记和参数。
mark cell:1,3 = START 起点
mark cell:1,4 = END 终点
mark cell:2,1 = ROOT 根音
mark cell:2,2 = TUNE 微调
mark cell:2,3 = LOOP 循环
mark cell:2,4 = GAIN 增益
}}

TONE 页有 CUTOFF、RESO、ATTACK、RELEASE 和 LEVEL：

{{shot:instr-sampler-tone|SAMPLER TONE 页。
mark cell:1,1 = CUTOFF 截止频率
mark cell:1,2 = RESO 共振
mark cell:1,3 = ATTACK 起音
mark cell:1,4 = RELEASE 释音
mark cell:2,1 = LEVEL 电平
}}

<!-- keywords: 裁剪, 剪切, 起点, 终点, 循环, 试听, trim, cut, start, end, loop, preview -->

## 保存、改名与删除 {#sampler-save}

- 裁剪界面第 4 格（SAVE）把录音存成 WAV，名字是 `TAKE_01`、`TAKE_02`……（第一个空着的编号），不用起名。
- 在采样列表里 {{key:FN+OK}} 打开光标所在文件的 RENAME 改名 / DELETE 删除。DELETE 会先确认，然后从卡上删掉这个 WAV。
- 别的轨道正在用的采样不能删除（FILE IN USE）。
- 采样文件属于共用目录，不属于某个工程；工程只记住它用哪个文件。

<!-- keywords: 保存采样, 命名, 改名, 删除采样, 录音名字, save sample, rename, delete sample -->

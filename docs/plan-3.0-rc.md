# MasterLens 3.0-rc 技术方案与分阶段计划

2026-11-06，基于 `v3.0.0-beta` / `aed9e86`（文档收尾 `7bd434e`）。
目标：把 3.0 线剩下的四项补齐——**频段独听、选区等响、长文件流式播放、编码 A/B**，然后收口 3.0.0。

本文件只描述方案与验收，不含实现。实现顺序以用户确认为准。

## 0. 摘要

| 阶段 | 内容 | 复用基础 | 新增核心 | 估算 |
|---|---|---|---|---|
| A | 频段独听（10 段） | `AuditionEngine` 实时带通、`SpectrumAnalyzer::bandTable` | 4 阶带通 + 频段点击命中 | 1–1.5 人日 |
| B | 选区等响 | 时间线拖选 → `measureRegion`、参考槽位 GAIN | `RegionLoudness`（BS.1770 门限积分） | 2–2.5 人日 |
| C | 长文件流式播放 | `FileAnalyzer` 已按 4096 帧流式读取 | `StreamingPreviewPlayer`（无锁环形缓冲） | 3–4 人日 |
| D | 编码 A/B | `ml::runCodecCheck`（ffmpeg AAC/MP3 往返） | `CodecABJob` + 双源同位置切换 | 2–3 人日 |

推荐顺序：**A → B → C → D**（两项监听能力先行，用户可以立刻验收；C 是 D 的地基；每阶段都能独立出包）。
版本切分建议：`3.0.0-rc.1` = A + B，`3.0.0-rc.2` = C，`3.0.0-rc.3` = D，验证通过后 `3.0.0` 正式版。

估算仅为单人开发、本机 macOS 构建的粗略值，不含用户实机验收时间。

## 1. 现状盘点

### 1.1 可以直接复用的

| 模块 | 能力 | 位置 |
|---|---|---|
| `AuditionEngine` | 实时带通独听（HP+LP 二阶）、单声道折叠、12 s 历史环、跟随宿主播放头 | `Source/audio/AuditionEngine.{h,cpp}`（`startLive` / `start` / `process`） |
| `SpectrumAnalyzer` | 10 段定义表（Sub 20–60 … Ultra 16–20k）、4 段能量占比、长期平均谱 | `Source/dsp/SpectrumAnalyzer.cpp:35` `bandTable`、`getBandInfo` |
| `TimelineStrip` | 拖选区间（≥0.15 s）+ `onSelectionChanged` | `Source/ui/Modules.h:168-195`、`Source/ui/Timeline.cpp:107-132` |
| `measureRegion` | 选区按 100 ms 包络点做能量平均，产出 `RegionMeasurement` | `Source/EditorSeries.cpp:43` |
| `ReferencePlayer` | 参考解码到内存、4 点 Catmull-Rom、`offsetSeconds` / `gainDb`、整轨 LUFS | `Source/audio/ReferencePlayer.{h,cpp}` |
| 参考槽位 | 1–4 条参考、逐槽 OFS/GAIN、随工程状态保存 | `Source/EditorPresets.cpp:552-640` |
| `FileAnalyzer` | 4096 帧分块流式读取，无整段载入；时长无上限 | `Source/file/FileAnalyzer.cpp:161-240` |
| `runCodecCheck` | ffmpeg AAC 256k / MP3 320k 编码 → 解码 float WAV → 重新测量，含 SHA-256 防篡改 | `Source/file/CodecCheck.{h,cpp}` |
| 监听等响 | `monitorMatch`：输入与参考各用独立 trim，参考不二次衰减 | `Source/PluginProcessor.cpp:236-252` |
| 测试基建 | DSP 151 项、CLI 66 项、UI 布局 35 项、RT 零分配审计 | `Tests/`、`scripts/check_ui.py` |

### 1.2 明确的缺口

1. **频段独听**：`AuditionEngine` 只有二阶 HP+LP（12 dB/oct），且只被 6 张信号卡驱动；频谱页 10 段没有点击命中区，`BandBalanceDisplay` 与热力图都不接受鼠标。
2. **选区等响**：`measureRegion` 用的是 400 ms 瞬时值的算术能量平均，**没有 BS.1770 门限**（−70 LUFS 绝对门 + −10 LU 相对门）；参考侧只有 1–4 条轨的合并包络（`ReferenceSpectrum.cpp:188-199`，拼在一起、样本 RMS 而非 LUFS），没有"逐槽逐段"响度。
3. **长文件流式**：`FilePreviewPlayer` 把 `[start,end]` 整段读进内存（`FilePreviewPlayer.cpp:97-118`）；独立版播放传的是 `0 → duration`，等于整首歌进内存。`ReferencePlayer` 有 1 h / 512 MB 硬上限，超限直接失败。
4. **编码 A/B**：`runCodecCheck` 只在 CLI 里用，产物是临时文件且立即清理；GUI 没有入口、没有播放、没有"A/B 同位置切换"。
5. **测试盲区**：PSR / TP 事件 / 谐振 / 门限积分都还没有断言（HANDOFF §7 也记了这一条）。


## 2. 阶段 A：频段独听（Band Solo）

### 2.1 交互

- 频谱页「频谱平衡」条的 10 段、以及频谱图/热力图的频段列，单击 = 独听该段；再点同一段 = 关闭；`Esc` 关闭。
- hover 显示 `250–500 Hz · 独听/停止`，按下时该段描边高亮；底栏固定显示 `实时跟随 DAW · 独听 250–500 Hz · Esc 停止`。
- 只改监听输出，不写任何工程状态；默认旁路不变。

### 2.2 DSP

- `AuditionEngine` 增加一条**专用 4 阶路径**（两级 biquad 级联，24 dB/oct），仅频段独听使用；现有 6 张信号卡的二阶行为保持不变，避免 1.5.1 的听感回归。
- 频点直接取 `SpectrumAnalyzer::bandTable`（单一定义源，不新造阈值）。
- 低频段（20–120 Hz）在 44.1 kHz 下加一档 Q 修正，避免 20–60 Hz 独听时几乎无声；不做等响补偿（避免掩盖真实能量），底栏注明"独听不做等响"。
- 全部在 `prepareToPlay` 预分配；音频线程零分配、零锁（沿用现有 `Biquad` 值语义）。

### 2.3 UI

- `BandBalanceDisplay` / `SpectrogramDisplay`（bands 模式）保存 10 个命中矩形，回调 `onBandClicked(int)`。
- `DetailArea` 复用现有集中式 `mouseDown`（`Source/ui/Detail.cpp:520`）分发，矩形存到 `bandHitBounds`（参考 `signalChipBounds` 的写法）。
- `Modules.h` 加 `onAuditionBand`；`PluginEditor.cpp` 接到 `EditorPresets.cpp` 的新 `auditionBand(int)`，复用 `lastAuditionIndex` 但用负值命名空间（如 `-10-index`）与信号卡区分。

### 2.4 测试与验收

- DSP：三个正弦（100 Hz / 1 kHz / 8 kHz）混音，独听 1 kHz 时其余两个衰减 **≥ 24 dB**；独听 20–60 Hz 时 1 kHz 衰减 ≥ 24 dB；单声道折叠后能量守恒在 ±0.2 dB。
- RT：新增用例纳入 `rtAudit`，回调内 `new/delete` 必须为 0。
- UI：`scripts/check_ui.py` 增加"10 段命中矩形不重叠、不越界、最小窗口仍可点"的断言；截图记录进 3.0-rc 的 screenshots JSON。
- 人工：三种频段独听在最小窗口、英文界面各点一次。

## 3. 阶段 B：选区等响（Selection Match）

### 3.1 交互（定义清楚）

- **选区 = MasterLens 时间线上的拖选**（`TimelineStrip` 已有），不是宿主 loop。VST3/AU 没有跨宿主的 loop 读取 API，这一点写进文档，不假装能读到。
- 选区 ≤ 10 s 时可用 `AuditionEngine` 的 12 s 历史环做循环试听；更长的选区循环由宿主负责，我们只负责等响。
- 参考侧取**当前槽位**文件在 `OFS + 选区起点 → OFS + 选区终点` 的响度。

### 3.2 算法

- 新增 `Source/dsp/RegionLoudness.{h,cpp}`：
  - `gatedLufs(const std::vector<double>& blockMs, double blockSeconds, int from, int to)`：BS.1770-4 门限积分（400 ms 块、75 % 重叠、绝对 −70 LUFS、相对 −10 LU）。
  - `measureFileRange(file, fromSeconds, toSeconds)`：后台线程只解码选区，产出同样的 100 ms 块能量，供参考侧使用。
- `AnalysisEngine` 增加一个 100 ms 的 K 加权均方发布（现在内部已有 400 ms 滑窗与 10 ms 刷新），`RawSeriesPoint` 多存一列 `blockMs`，让选区响度可以按门限积分而不是算术平均。
- 精度目标：与离线参考实现对同段素材的差值 **≤ 0.1 LU**；对 3.0 之前的历史数据（只有 momentary）保留旧算法并标注。

### 3.3 等响策略（不动你的声音）

- 选区等响**只调参考的 GAIN**：`delta = selLufs(工程) − selLufs(参考槽位)`，写入当前槽位的 `referenceGains[slot]`，并立即生效。
- 工程侧不做任何 trim；A/B 时两者在同一响度下比较音色。这与现有 `monitorMatch`（整轨等响）不冲突：选区模式优先，退出后回到整轨模式或关闭。
- 底栏显示：`选区 −14.2 LUFS · 参考段 −11.8 LUFS · Δ −2.4 dB`；按钮再点一次 = 取消等响并恢复原 GAIN。

### 3.4 状态与持久化

- `referenceGains` 已随工程保存（走 `apvts.state`），无需新增格式；只新增"选区等响是否锁定"的 UI 状态（不写工程）。

### 3.5 测试与验收

- DSP：−20 dBFS 997 Hz 正弦（双声道，BS.1770 计入两声道 → −20 LUFS）→ 门限积分 −20.0 LUFS（±0.1）；前后各 5 s 静音 + 10 s 节目 → 静音必须被绝对门剔除；两段不同电平拼接 → 相对门生效（对比手算值）。
- 参考侧：同一文件 `measureFileRange(0, 10)` 与整轨 `Stats.integrated` 在纯稳态素材上差值 ≤ 0.1 LU。
- 边界：选区不足 3 个 100 ms 点、选区超出已播放范围、参考未就绪 → 明确文案，不静默失败。

## 4. 阶段 C：长文件流式播放（Streaming Preview）

### 4.1 设计

新增 `ml::StreamingPreviewPlayer`（`Source/audio/StreamingPreviewPlayer.{h,cpp}`）：

- **消息线程**：`open(file)`、`start(fromSeconds, loop)`、`seek`、`stop`；只做命令投递。
- **读取线程**：保持 3 s 预读水位，`AudioFormatReader::read` 以 8192 帧为单位填入**预分配环形缓冲**（2 ch × 4 s × 192 kHz ≈ 6 MB，`prepare` 时分配）；负责 >2 ch 下混、非法样本检测、EOF/循环环绕。
- **音频线程**：只读环形缓冲 + 原子读写下标；欠载时输出静音并置 `buffering` 标志，**绝不阻塞、绝不分配**；采样率换算沿用 4 点 Catmull-Rom（与 `ReferencePlayer` 一致）。
- **状态**：`isActive / isBuffering / getPositionSeconds / getDurationSeconds / getBufferedSeconds / getStatus`。

### 4.2 接线

- `PluginProcessor` 持有实例；`processBlock` 中"独立版文件作为分析源"的路径改用它（保持现在"先喂分析、再做输出覆盖"的顺序，见 `PluginProcessor.cpp:180-208`）。
- `EditorFiles.cpp::toggleFilePlayback` 不再限制时长；`DetailFiles.cpp` 的传输条增加"缓冲中"状态，进度条按读取位置走。
- `FilePreviewPlayer` 保留，继续服务 ≤ 4 s 的证据片段回听（2.5.0 行为不变）；两条路径共用同一套"消息线程命令 + 无锁资产"模式。
- `ReferencePlayer` 的内存上限**本次不动**，但把"参考过长"的失败文案改成明确提示（参考轨仍走内存解码；逐槽流式参考属于 2.6 的后续项）。

### 4.3 测试与验收

- 新增测试用 `juce::AudioFormatReader` 子类（按需生成扫频/正弦，不落盘），模拟 2 小时文件：
  - 打开、播放 60 s、任意 seek、循环边界、EOF 停止都正确；
  - 环形缓冲欠载时输出静音且状态为 `buffering`，随后自动恢复；
  - `rtAudit`：`render()` 内 `new/delete` 为 0。
- 真实文件：≥ 2 h 的 48 kHz WAV 播放 5 分钟，内存增量 < 20 MB、无爆音、进度条与时间码一致。
- 独立版冒烟：导入长文件 → 播放 → 拖动进度 → 停止，进程不崩、CPU 增量可接受。

## 5. 阶段 D：编码 A/B（Codec A/B）

### 5.1 设计

- 新增 `ml::CodecABJob`（`Source/file/CodecABJob.{h,cpp}`）：把现有 `runCodecCheck` 包成可显示进度、可取消的后台任务，产出 `{codec, bitrate, encodedBytes, encodedPath, decodedWavPath, sourceLufs, decodedLufs, sourceTp, decodedTp, error}`。
- 新增 `ml::ABSwitchPlayer`：持有主文件与解码 WAV 两个 `StreamingPreviewPlayer`，**同位置切换**（同一时间码 + 30 ms 等功率交叉淡化，避免咔嗒），并提供 `setEqualLoudness(bool)`（用两者实测 LUFS 差做补偿，补偿量显示出来）。
- ffmpeg 仍是可选依赖：缺 ffmpeg 时按钮置灰并提示"需要本机 ffmpeg"，与 2.3 CLI 的"明确失败、不伪造结果"保持一致；不做云端编码。
- 临时文件在会话结束或切换文件时清理；编码结果不写回源文件。

### 5.2 UI 与导出

- 文件 QC 页每行加「编码 A/B」菜单（AAC 256k / MP3 320k）；选中文件后传输条出现 `A 原始 · B AAC 256k`，`A/B` 键切换，底栏显示当前侧与 `ΔLUFS / ΔTP / 码率`。
- `EditorCsv.cpp` / `EditorJson.cpp` 增加 `codec`、`codecLufs`、`codecTp`、`codecBytes`、`deltaLufs`、`deltaTp` 字段；源文件 SHA-256 与编码/解码指纹继续沿用 `CodecCheck` 的现有字段。

### 5.3 测试与验收

- 新增控制台目标 `MasterLensCodecAbTest`（或扩展现有 CLI 测试脚本）：
  - 生成 10 s 测试音 → 跑一次 AAC 与一次 MP3 → 解码 WAV 存在、LUFS 与源差 ≤ 0.5 LU、`A/B` 切换后位置连续（误差 ≤ 1 个 block）；
  - 缺 ffmpeg：返回明确错误码与文案，测试跳过并标注 skip（不冒充通过）；
  - 取消：编码中途取消后临时目录必须清空。
- CI：macOS 与 Windows runner 安装 ffmpeg 后跑该用例；未安装的分支只跑"缺失依赖"用例。
- 导出：CSV/JSON 字段与 CLI `--codec` 的报告字段保持一致，避免两套口径。

## 6. 分阶段交付与验证门禁

每个阶段都必须过同一套门禁（沿用 beta 的做法）：

1. `MasterLensDspTest` 全通过（当前 151 项，预计每阶段 +4 ~ +8 项）。
2. `Tests/test_cli.py` 全通过（当前 66 项；阶段 D 增加编码 A/B 用例）。
3. `scripts/check_ui.py` 全通过（当前 35 项，阶段 A/B 增加命中区与按钮状态断言）。
4. `python3 scripts/capture_ui.py` 出图，逐页看数字/单位/按钮不越界；截图 SHA-256 写进 `docs/screenshots-3.0.0-rc.N.json`。
5. macOS Universal 构建 + DMG 校验 + `auval`；Windows x64 / ARM64 CI 构建 + NSIS 安装冒烟。
6. CI 绿 → 发布 pre-release → 下载三个产物、`shasum -a 256 -c SHA256SUMS.txt` 校验 → 把 run id 与三个哈希写进 `docs/performance-3.0.0-rc.N.md`（本次 beta 补的就是这一步）。
7. 版本号改动点：`Source/Version.h`、`.github/workflows/build.yml`（`APP_VERSION` 与 `--notes-file`）、`packaging/installer.nsi`、`packaging/README-DMG.txt`；`CMakeLists.txt` 的 `VERSION 3.0.0` 保持不变，直到 `3.0.0` 正式版。

## 7. 工程约束合规检查

| 约束 | 本方案如何满足 |
|---|---|
| 音频线程零分配 / 零锁 / 零 IO | 新播放器全部在 `prepare` 预分配环形缓冲；读取线程做 IO；音频线程只读原子下标 |
| 不修改音频、零延迟 | 所有新功能只在用户显式开启监听时改写输出；关闭即逐位旁路 |
| 判断可追溯到 BS.1770 / 静态表 / 用户参考 | 门限积分按 BS.1770-4；频段边界取现有 `bandTable`；等响只用实测差值 |
| 不联网、不加载模型 | 编码 A/B 只用本机 ffmpeg 子进程（2.3 已如此），不做云端 |
| 纯 C++ / JUCE | 无新增第三方库；测试用 JUCE 自带 reader |
| 每条检查带时间戳/区间 | 选区等响与频段独听都在底栏显示时间范围与频段边界 |

## 8. 已确认的决策（2026-11-06）

1. **顺序**：实现顺序 A → B → C → D。
2. **发版**：四项合成一个 `3.0.0-rc.1`，不发中间 pre-release；全部完成后统一构建、验证、发布。
3. **选区的定义**：以时间线拖选为准；≤ 10 s 可用 12 s 历史环循环，更长的循环交给宿主。
4. **编码 A/B 的依赖**：GUI 沿用"本机 ffmpeg，缺失即明确不可用"。

### 原始待确认项（保留备查）

1. ~~顺序：是否按 A → B → C → D？~~
2. ~~rc 切分：`rc.1 = A+B`、`rc.2 = C`、`rc.3 = D`，还是合成一个 rc？~~
3. ~~选区的定义：以时间线拖选为准、长选区循环交给宿主？~~
4. ~~编码 A/B 的依赖：本机 ffmpeg，缺失即明确不可用？~~
## 9. 实现状态（2026-11-06）

四项均已在 `3.0.0-rc.1` 落地，验证记录见 `docs/performance-3.0.0-rc.1.md`：

- A 频段独听：`AuditionEngine::startLiveBand`（4 阶、可选补偿），频谱页 10 段 / 三段点击。
- B 选区等响：`RegionLoudness` + `ReferenceRangeMeter`，时间页「选区等响」按钮。
- C 长文件流式：`StreamingPreviewPlayer`，独立版文件传输已切换。
- D 编码 A/B：`CodecABJob` / `createCodecPreview`，文件页「编码 A/B」菜单与 `B` 快捷键。

尚未覆盖：Windows / DAW 实机听感走查；逐槽流式参考（2.6 后续项）。



- UI：时间线选区 + 参考就绪 → 按钮可用；未就绪 → 置灰并有 tooltip。

# MasterLens 3.1.0 验证记录

> 版本：3.1.0 · 2026-11-12 · 对应 `docs/design-3.1.md`
> 本文记录本机 Universal 构建、测试、截图与打包结果。交付范围与已知边界见 [release-3.1.0.md](release-3.1.0.md)。

## 已落地

### 3.1 视觉（Calibrated Plate）

- 顶栏常驻「测量 / 监听」上下文条；快照键 1 / 2；
- 响度页：46px 等宽主读数、目标窗口、四项指标行、覆盖率、平台交付账本、去页卡；
- **两主题共用的深石墨测量窗**（`well / wellEdge / plotInk / plotFaint / plotGrid / plotAccent`），历史图与频谱图共用；
- 页标题 15 / 元信息 10 / 微标签 11 / 等宽数值 15；
- 信号页：平直细线六宫格、方形状态胶囊、22–26px 等宽数值；
- 频谱页：整幅深窗与分频段读数。

### 3.0.4 可信交付

- 新增 `effectiveDelivery`：客户规格优先，其次平台/手动目标；GUI、CLI、CSV、JSON 共用；
- 独立文件播放按**唯一覆盖时间段**统计，部分播放不再报 100%；
- 验收样例：−23 LUFS 母带对 −23 LUFS 客户目标 → 建议增益 0 dB（DspTest 覆盖）。

### oil-ui 视觉对齐

- 调色板改为油墨色板：深色 plate `#141719`、panel `#191d1f`、well `#0b0e0f`、ink `#e9e6dc`、label `#939b95`、accent `#a3bfae`；浅色 plate `#eae6db`、ink `#212822`、accent `#3e6a55`；
- 品牌标记改为设计稿的「环 + 双弧 + 圆点」（32 单位，1.4 描边，0.55/0.9 透明度）；
- 11 个页签图标直接采用设计稿的 16×16 描边路径（SVG 解析缓存、按行着色）；
- 顶栏三个文字按钮改为设计稿的图标方块：太阳 / 信息 / 齿轮，EN 保留文字；
- 1120 宽恢复右侧「平台交付」账本（紧凑档才隐藏）。

### 3.1 紧凑档（≤1024）

- 窗口下限 1000×630；该区间 **1:1 字号**（不缩小中文），画布按真实尺寸布局；
- 左栏收窄到 176，交付账本在窄画布隐藏；顶部、页签、参数条、底部动作在 1000 宽内自适应；
- 0.8× 等缩放窗口仍走 1120 画布缩放；`check_ui` 的 minsize 场景改为 1000×630，46/46 通过；
- 实拍：[compact-3.1.0.png](compact-3.1.0.png)。

### 3.2 参考试听（首批）

- 参考 A/B 片段：单击记录当前位置、双击命名，名称与时间写入 `apvts.state`；
- LOOP：A→B 循环，循环点 64 样本等功率交叉淡化；
- 替换参考：新文件更短时自动收进时长，状态行提示「参考已替换，片段已重新对齐」；
- **有界流式读取**：超过 64 MB 的参考改走 `BufferingAudioReader`（8 s 预读，约 3 MB），不再整段解码；
  96 kHz / 120 s 实测 `isStreaming=1`、内存 < 32 MB、可正常出声；状态显示 `ready (stream)`；
- **流式 Cue 循环**：≤30 s 的 A→B 区间预载进有界缓冲后自由循环（64 样本交叉淡化），长区间回退不循环；
- **流式等响**：无 Cue 时用前 60 s 有界前缀测响度；启用 Cue 时优先用循环区间响度；短文件仍走精确内存路径。

### 3.3 编码对听（首批）

- `measureCodecAlignment`：前 8k 样本归一化互相关，测量编码器延迟与残差；
- 编码 A/B 切换按 `delaySamples` 对齐（A→B 加延迟、B→A 减延迟）；
- 解码文件扫描最高样本峰，菜单「跳至编码后最高峰」直接回听；
- 编码检查 JSON 公开 `delaySamples / tailSamples / residual`；
- 实测 AAC 256k @48 kHz：延迟 4080 样本（85 ms），残差 1.4×10⁻⁶；
- **A−B 差分**：两路对齐后按 `sourceLufs − decodedLufs` 等响匹配再相减，输出 +6 dB 提升；菜单开关、状态行显示匹配量；
- **TP 定位细化**：对全局样本峰做 33-tap 窗函数 sinc 的 4× 插值（0.25/0.5/0.75），定位到编码后真峰值附近；单元测试验证插值峰值高于样本峰；

### 3.4 批量交付（首批）

- `--state` / `--summary` / `--retry`：增量队列、失败重试、交付摘要；
- 摘要含 `previousStatus`、`versionChanged`；实测首跑 2 分析、再跑 2 跳过、改动后只复检 1；
- **连续监视**：`--watch SECONDS` 由父进程监督，每轮以子进程执行单次扫描，状态/摘要跨轮保留；`--watch-iterations` 供测试；
- **GUI 批量摘要**：文件表「导出 QC」的 JSON 现在包含 `summary`（analyzed / passed / review / blocked / errors / target / ceiling），与 CLI 摘要同口径；
- **GUI 监控（逐文件增量）**：2 s 轮询 mtime/size；只有变更文件进入分析，未变更行保留，删除的文件从表中移除；结果按路径去重合并；
- GUI 批量摘要见上。

### 3.0.4 监听导出验证

- 代码顺序：`engine.process()`（测量）→ 参考渲染 / 等响 trim / 独听 / 证据试听（只改输出）→ 导出读 `stats`；
- 单元测试：同一测量数据把 `monitorTrimDb` 从 0 改为 −6 dB，导出的 `requestedGainDb / safeGainDb` 完全不变（`export budget ignores monitor trim`、`export requested gain unchanged`）；
- 独立版文件播放属于「文件即分析源」的设计，证据试听与文件分析同源，不在本项范围；
- 结论：参考、等响、独听、证据试听不会进入导出测量与建议增益；`monitorTrimDb` 仅作为报告里的信息字段。

### 3.1 文件 QC 工作流

- 「下一处待回听」跨文件查找未处理证据并试听；
- 证据行：单击试听、右侧 `[ ]/[x]` 确认、双击弹出备注 / 可忽略；
- `FileFinding` 新增 `ignored` 与 `note`，随证据 JSON 导出；
- 「客户规格」浮层：目标 / 上限 / 容差 / 采样率 / 位深 / 声道 / 命名，实时预览通过与阻断。

## 验证

| 项 | 结果 |
| --- | --- |
| MasterLens / MasterLensCli / MasterLensUiShot 构建 | 通过（3.1.0） |
| `MasterLensDspTest` | **214 passed / 0 failed / 0 skipped**（`MASTERLENS_REQUIRE_RUNTIME=1`） |
| `Tests/test_cli.py` | **86 passed / 0 skipped**（含批量增量与对齐字段） |
| `scripts/check_ui.py` | **46/46 场景通过，0 失败**；`layout-3.1.0.json` 92 条记录 |
| 3.0.4 目标测试 | `spec target overrides`、`spec target zero gain` 通过 |
| 截图 | 9 张 `docs/*-3.1.0.png`（含紧凑档）+ `docs/screenshots-3.1.0.json`（`renderer: MasterLensUiShot 3.1.0`） |

### 截图

- [响度](screenshot-3.1.0.png)
- [信号](signal-3.1.0.png)
- [频谱](spectrum-3.1.0.png)
- [声场](stereo-3.1.0.png)
- [文件 QC](fileqc-3.1.0.png)
- [关于](about-3.1.0.png)
- [编码就绪](codec-ready-3.1.0.png) / [编码失败](codec-failed-3.1.0.png)
- [紧凑档 1000×630](compact-3.1.0.png)

出处文件记录 `rendererSha256` 与 `architecture: universal (x86_64 + arm64)`。

### 本机 DMG

- `dist/MasterLens-3.1.0-macOS.dmg`，约 35 MB，
  SHA-256 `3b0ed6c169ae63a40aa0f096d974269290fb7b51d59fc4386b5b208c04566780`。
- 挂载后 `CFBundleShortVersionString = 3.1.0`；三份插件包内的 `ffmpeg` 均
  `codesign -v` 通过；从 DMG 复制 `MasterLensCli` + `ffmpeg` 到空目录，PATH 无 ffmpeg
  时 `--codec mp3` 成功（`backend=bundled`，延迟 1200 样本）。
- 发布 AU 重新 ad-hoc 签名后 `auval`：`Cocoa Views Available: 1`，
  `AU VALIDATION SUCCEEDED`。

## 复跑时修掉的两个问题

- **编码状态截图相同**：`codec-ready` / `codec-failed` 的截图完全一致。根因有两点：
  File QC 的编码按钮只在独立版文件播放可用时显示，而截图脚本没有传 `standalone`；
  另外页脚只在非工程监听源时显示状态，编码结束/失败后状态被技术行盖掉。现在
  `capture_ui.py` 为编码截图补上 `standalone`，`EditorPaint.cpp` 在
  `hasCodecPreview` 或错误状态时也显示页脚状态。
- **文档数量与事实不一致**：验证记录里的 CLI 82 / DSP 1 skipped 是旧运行结果；
  随包运行时下实测为 **214 / 86 / 0 skipped**，已按本轮结果更新。

## 未完成（后续版本）

- **3.2** 「工程选区对参考」一键动作；
- 文件表与剩余页的细线化。

## 复现

```sh
# 构建
./.tools/cmake-4.4.5-macos-universal/CMake.app/Contents/bin/cmake --build build-universal \
  --target MasterLens MasterLensCli MasterLensUiShot MasterLensDspTest -j 8

# 测试
./build-universal/MasterLensDspTest_artefacts/Release/MasterLensDspTest
python3 Tests/test_cli.py build-universal/MasterLensCli_artefacts/Release/MasterLensCli
python3 scripts/check_ui.py --build-dir build-universal --jobs 6

# 截图
python3 scripts/capture_ui.py --build-dir build-universal
```

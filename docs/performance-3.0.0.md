# MasterLens 3.0.0：验证记录

2026-11-06，基于 `v3.0.0-rc.1` / `070ff48`。3.0.0 正式版在 rc.1 的基础上只做三件事：修好三处读数对齐、修掉响度曲线右上角被裁掉的单位、把布局回归接进 CI；四项监听能力的测量口径与 rc.1 完全一致。

## 改动

### 四项监听能力（沿用 rc.1）

- **频段独听**：`AuditionEngine` 新增 4 阶 Butterworth 实时带通（Q 0.5412 / 1.3066，24 dB/oct），`compensate=false` 时不做电平补偿；频谱页 10 段热力图与低 / 中 / 高三分段都可点击，`Esc` 停止。
- **选区等响**：`RegionLoudness`（BS.1770-4 门限积分：400 ms 窗、100 ms hop、−70 LUFS 绝对门、−10 LU 相对门）与 `ReferenceRangeMeter`（后台只解码选区）；时间页「选区等响」把差值写进参考槽位 GAIN，工程侧不动。
- **长文件流式播放**：`StreamingPreviewPlayer`——读取线程 8192 帧预读、无锁环形缓冲（4 s × 192 kHz，约 8 MB）、4 点 Catmull-Rom；独立版文件传输改用它，欠载输出静音并显示「缓冲中」。
- **编码 A/B**：`CodecABJob` / `createCodecPreview`（本机 ffmpeg AAC 256k / MP3 320k → float WAV）与处理器内的 A/B 双流切换；文件 QC 显示 ΔLUFS / ΔTP，CSV / JSON 增加 `codecAB` 字段。

### 正式版修掉的排版

- `Source/ui/Theme.h` 新增 `namespace text`：`centredBaseline()` / `drawRun()` / `drawValueUnit()`，让数值与单位共用一条基线。
- `Source/ui/Panels.cpp`：`ReadoutBand::paint` 的行高改由 `valueFont.getAscent() + getDescent()` 决定，单位不再用 `withTrimmedTop(8)` 硬推；`MetricGrid::paintCard` 去掉 `withTrimmedTop(16)`，两处都改走 `drawValueUnit()`。
- `Source/ui/Logo.cpp`：`paintBerryLockup` 不再把字标塞进顶部 14 px 条带，字标占满整行，与页脚其余两行同中线。
- `Source/ui/Settings.cpp`：页脚先在 `settingsFooterParts()` 里把整组对齐到 4 px 网格，再交给 `grid::alignChildren()`。此前只有真实按钮被 30→28 px、y 555→552 上移约 4 px，文字与按钮并不在同一条中线。
- `Tests/RealtimeChecks.h`：参考播放器的 ready 断言改为等 `isReady()`；原实现等 `isBusy()` 变 false，线程尚未启动时会提前断言，在 Universal / 慢构建上会偶发失败。
- `Source/ui/History.cpp`：曲线右上角的当前值不再用固定的 92 / 34 px 列宽，改为按字体实测宽度排版；`LUFS` 之前在 14 px 下宽约 37 px，被截成 `LU…`。

### 工程

- `scripts/check_ui.py` 接入 `build.yml` 的 macOS job（`Editor layout regression` 步骤 + 报告 artifact），构建目标补上 `MasterLensUiShot`。
- `check_ui.py` 现在每个场景跑两次：先 `layoutcheck`（真实组件边界 / 字体 / 全部页面与缩放断言），再渲染；`layoutcheck` 被移到场景开关之后，settings / about / 频段热力 / 选区 / 编码状态都会在断言前生效。
- `UiShot` 增加 `selectionset` / `codecstate` 两个快照入口，走真实的 `measureRegion()` 与 `applyCodecResult()` 路径，CI 不再只验证“能画出来”。
- `Source/ui/About.cpp` 增加「选区等响需先播放该段 · 编码 A/B 需本机 ffmpeg」；README 与 DMG 使用说明同步这两条前提（设置页保持原有四行说明，未加行以免挤掉页脚）。

## 验证

- DSP / 可靠性断言：**178 项通过**。
  - 频段独听：通带增益 −0.03 dB（容差 ±1.5 dB），相邻两倍频程泄漏 **≤ −24 dB**；回调内 `new/delete` 为 0。
  - 选区等响：稳态 −23.0 LUFS 门限积分不变；−95 LUFS 窗口被绝对门剔除；−50 LUFS 段按相对门剔除后回到 −20.0 LUFS；文件选段测量 −20.0 LUFS（±0.2），窗口数 37；短于 0.4 s 的选段明确拒绝。
  - 长文件流式：2 小时虚拟文件可打开、播放、seek 到 3600.01 s、EOF 停止；回调内 `new/delete` 为 0。
  - 编码 A/B：AAC 往返成功，解码 WAV 可测量，与源的 LUFS 差 ≤ 0.6 LU；无 ffmpeg 时用例显式跳过并打印 `encode: ffmpeg unavailable`。
- CLI 集成检查：**66 项通过**。
- UI 布局：**44/44 场景通过**（每个场景先跑 `layoutcheck` 边界断言，再渲染；含 80%–200% 缩放、浅色深色、最小窗口、settings / about、频段热力、选区与编码状态）。
- 三处对齐逐像素复核（深色 / 浅色，100% 与 200%）：侧栏与频谱页 5 组读数的数字/单位底线差 ≤ 1 px；设置页页脚字标 1132–1148、开发者 1131–1149、关闭按钮文字 1131–1149（中心均为 1140）；曲线右上角 `-17.7 LUFS` 完整显示。
- 六张 README 截图（2240×1400）重新渲染，记录在 `docs/screenshots-3.0.0.json`，SHA-256 复核一致。
- macOS Universal 构建（x86_64 + arm64）、DMG 校验、`auval -v aufx Maln OrcA` 通过；本地 DMG SHA-256 `18d727dec9c33af94cffed5259f3be084e88fb97af57b3cda73f91fde5a6fee1`。

## 未验证 / 限制

- 频段独听、选区等响、编码 A/B 的真实听感与宿主行为仍需实机走查；本轮只验证测量、线程安全与界面布局。
- 「选区」是 MasterLens 时间线拖选，不是宿主 loop；长选区循环交给宿主，≤ 10 s 可用 12 s 历史环。
- 参考轨仍整段解码，超过 1 小时或 512 MB 会明确拒绝；逐槽流式参考不在本版。
- 编码 A/B 依赖本机 ffmpeg；Windows 实机需自行安装，macOS 继续 ad-hoc 签名、未公证。
- Windows ARM64 仍为交叉构建，未在 Windows on ARM 实机验证。

# MasterLens 3.1.0

> 版本：3.1.0 · 状态：**稳定版** · 2026-11-12
> 本版把 3.0.4–3.4 的能力与 Calibrated Plate 界面并入同一版本；验证证据见 [performance-3.1.md](performance-3.1.md)。

## 一、本版做了什么

### 1. 界面（Calibrated Plate，三页完成）

- **oil-ui 对齐**：油墨调色板（plate/panel/well/ink/label/accent）、环+双弧品牌标记、设计稿 16×16 页签图标、顶栏太阳/信息/齿轮图标方块；
- 1120 宽显示右侧平台交付账本，1000×630 紧凑档隐藏账本并收栏；

- 顶栏「测量 / 监听」上下文条；快照键 1 / 2；
- 响度页：46px 等宽主读数、目标窗口、指标行、覆盖率、平台交付账本、去页卡；
- **两主题共用的深石墨测量窗**（历史图 / 频谱图同一组窗内墨色）；
- 频谱页整幅深窗；信号页平直细线六宫格、方形状态胶囊、等宽数值；
- 页标题 15 / 元信息 10 / 微标签 11 / 数值 15。

### 2. 3.0.4 可信交付

- `effectiveDelivery` 统一 GUI / CLI / CSV / JSON 的有效目标（客户规格优先）；
- 独立文件播放按**唯一覆盖时间段**统计，部分播放不再报 100%；
- 验收样例：−23 LUFS 母带对 −23 LUFS 客户目标 → 建议增益 0 dB（DspTest 覆盖）。

### 3. 3.1 文件 QC 工作流

- 「下一处待回听」跨文件查找未处理证据并试听；
- 单击试听、右侧 `[ ]/[x]` 确认、双击弹出备注 / 可忽略；
- `FileFinding.ignored / note` 随证据 JSON 导出；
- 「客户规格」浮层：目标 / 上限 / 容差 / 采样率 / 位深 / 声道 / 命名，实时预览通过与阻断。

### 4. 3.2 参考试听（首批）

- 参考 A/B 片段 + 命名 + LOOP（A→B 循环，64 样本等功率交叉淡化）；
- 片段与循环写入 `apvts.state`，重开工程恢复；
- 替换参考后自动收进新时长，状态行提示重新对齐；
- **有界流式读取**：>64 MB 的参考使用 8 s 预读缓冲（约 3 MB），实测 96 kHz/120 s 内存 <32 MB；
  流式模式支持 ≤30 s 的 Cue 循环（预载 + 交叉淡化）；无 Cue 时用前 60 s 的有界前缀测响度，等响监听可用。

### 5. 3.3 编码对听（首批）

- `measureCodecAlignment`：前 8k 样本互相关，测编码延迟与残差；
- 编码 A/B 切换按延迟对齐（A→B 加、B→A 减）；
- 解码文件最高峰定位，菜单「跳至编码后最高峰」回听；
- 编码检查 JSON 公开 `delaySamples / tailSamples / residual`；
- 实测 AAC 256k @48 kHz：延迟 4080 样本（85 ms），残差 1.4×10⁻⁶；
- **A−B 差分**：按源/解码 LUFS 等响匹配后相减，+6 dB 提升便于听伪影；
- **TP 定位**：全局样本峰用 33-tap 窗函数 sinc 做 4× 插值细化（0.25/0.5/0.75），定位到真峰值附近。

### 6. 3.4 批量交付（首批）

- `--state / --summary / --retry`：增量队列、失败重试、交付摘要；
- 摘要含 `previousStatus`、`versionChanged`；
- 实测：首跑 2 文件分析；再跑 0 分析 2 跳过；改动 1 个后只复检 1 个；
- **连续监视**：`--watch SECONDS` + `--watch-iterations`（父进程监督子进程扫描，状态/摘要跨轮保留）；
- **GUI 批量摘要**：导出 QC 的 JSON 增加 `summary` 计数块；
- **GUI 监控**：项目菜单「监控文件夹…」按文件 mtime/size 轮询，**只复检变更文件**，未变更行保留、删除文件移出。

## 二、验证

| 项 | 结果 |
| --- | --- |
| 构建（MasterLens / CLI / UiShot / DspTest） | 通过，3.1.0（Universal） |
| `MasterLensDspTest` | **214 passed / 0 failed / 0 skipped** |
| `Tests/test_cli.py` | **86 passed / 0 skipped** |
| `scripts/check_ui.py` | **46/46 场景**；`layout-3.1.0.json` 92 条记录 0 失败 |
| 截图 | 9 张 `docs/*-3.1.0.png`（含紧凑档）+ `screenshots-3.1.0.json` |
| 真实 AAC 对齐 | 4080 样本 / 残差 1.4×10⁻⁶；`auval` PASS |

## 三、安装包

- `MasterLens-3.1.0-macOS.dmg`（Universal，VST3 / AU / Standalone / CLI，含随包 FFmpeg）
- `MasterLens-3.1.0-Windows-x64.exe`
- `MasterLens-3.1.0-Windows-ARM64.exe`
- `SHA256SUMS.txt`

macOS 仍为 ad-hoc 签名、未公证；Windows ARM64 仍为交叉构建，未在 ARM64 实机验证。

## 四、已知边界

- **3.2 流式**：有界预读 + 内存峰值 + ≤30 s Cue 循环 + 前缀响度均已落地；

- **3.4**：CLI `--watch`、GUI 逐文件增量监控与导出摘要均已就绪；
- **紧凑档（≤1024）**：已实现 1000×630 紧凑档，1:1 字号、收栏、隐藏账本，实拍 `docs/compact-3.1.0.png`；
- 3.0.4 监听导出已验证：参考 / 等响 / 独听 / 证据试听只改输出，不进入导出测量与建议增益（含单元测试）；
- 文件表与部分页面的细线化仍为 3.0.3 形态。

## 五、升级与迁移

- 版本号 3.1.0；工程 / 会话新增 `refCueA / refCueB / refLoop / refCueAName / refCueBName` 属性，旧工程缺少时按默认值处理；
- 文件证据 JSON 新增 `ignored / note / delaySamples / tailSamples / residual` 字段，旧报告仍可读；
- CLI 新增 `--state / --summary / --retry`，与 `analyze` 兼容。

## 六、后续计划

1. 「工程选区对参考」一键动作；
2. 文件表与剩余页的细线化；
3. 真实宿主与 ≥2 小时素材的实机验收。

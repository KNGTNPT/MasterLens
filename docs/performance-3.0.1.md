# MasterLens 3.0.1：编码后端与发布门禁记录

2026-11-08，基于 `v3.0.0` / `f50742b`（文档收尾 `a5d601a`）。3.0.1 不改测量算法与监听功能，只统一编码后端、补齐失败分类与发布门禁。预发布标签：`v3.0.1-rc.1`。

## 一、审计结论（静态审查 + 运行时复现）

| 发现 | 位置 | 处理 |
|---|---|---|
| GUI 与 CLI 各自实现 FFmpeg 查找、启动、超时 | `CodecPreview.cpp`、`CodecCheck.cpp` | 合并为 `FFmpegRuntime`，只保留一份解析 / 探测 / 进程管理 |
| 查找顺序依赖 PATH，打包后不可控 | 同上 | 随包优先；显式覆盖（`MASTERLENS_FFMPEG`）必须可用；PATH 只在开发标志下启用 |
| 「无法启动」和「编码失败」都只报 exit code | 同上 | 区分 unavailable / launch / failed / timeout / cancelled / invalid output |
| 短寿命子进程探测存在竞态（脚本已退出但输出未读） | 探测路径 | 等待退出后同步排空；实测 CRLF 脚本曾导致 255，已改为显式 LF |
| 编码失败可被测试当成 `skipped` | `Tests/DspTest.cpp` | 只对「运行时缺失」跳过；`MASTERLENS_REQUIRE_RUNTIME=1` 时缺失即失败 |
| 中文 / 空格路径在 CLI 变乱码 | `Source/cli/main.cpp` | UTF-8 / Windows 宽字符 argv；单测覆盖 |
| 没有取消 / 进度 / 20 s 无输出检测 | `CodecABJob`、`FFmpegRuntime` | 取消标志、`-progress pipe:1` 解析、超时与停滞检测 |
| 发布包与运行时没有绑定检查 | 打包 / CI | 安装器、DMG、CI 都在缺少运行时时明确失败（prerelease 除外） |

## 二、实现

### 运行时

- `Source/file/FFmpegRuntime.{h,cpp}`：随包路径（macOS `Contents/Resources/ffmpeg`；Windows `Contents\Resources\ffmpeg\ffmpeg.exe` / `$INSTDIR\ffmpeg\ffmpeg.exe`）、显式覆盖、开发 PATH 回退；`-version` 与 `-encoders` 探测；LGPL / GPL 标记；有界输出、进度、超时、取消、退出码分类。
- `CodecPreview` / `CodecCheck` / `CodecABJob` / CLI 全部改用运行时；结果记录 `backend`、`ffmpegVersion`。
- CLI 帮助与 `Codec backend:` 行说明随包运行时与开发覆盖方式；JSON `codecChecks[]` 增加后端字段。

### 测试

- DSP：AAC（必需）、MP3（能力存在时）、后端记录、中文/空格路径、损坏输入、显式无效运行时、超时（探测通过但编码挂起的假运行时）。
- CLI：缺失运行时、超时、损坏输入、Unicode/空格/单声道/44.1 kHz、后端字段；`passed / skipped` 分开计数。
- 失败分类验证：无运行时 → 跳过并计入 skipped；`MASTERLENS_REQUIRE_RUNTIME=1` → 失败；无效路径 → `ffmpeg unavailable`；挂起 → `timeout`；损坏输入 → 失败。

### 打包与 CI

- `scripts/fetch-ffmpeg.sh`：固定 URL + SHA-256，拒绝 GPL/nonfree，要求 AAC（MP3 可选），复制许可证。
- `scripts/verify-runtime.sh`：版本、许可配置、编码器、macOS 双架构校验。
- `scripts/ci-prepare-ffmpeg.sh`：有运行时→`REQUIRE`；无运行时→开发/PR 用系统 ffmpeg；稳定 tag 缺失→失败。
- macOS DMG / Windows NSIS 在缺少运行时且未显式允许时构建失败；安装后冒烟检查运行时可执行。
- 新增 `.github/workflows/ci.yml`（push/PR：构建 + DSP/CLI/布局）；`build.yml` 发布门禁会读取 `runtime-*.json`，稳定版缺失运行时直接失败。

## 三、验证结果

| 项目 | 结果 |
|---|---|
| DSP / 可靠性 | **178 项通过**（0 skipped，使用显式系统运行时）；无运行时 → 178 通过 + 1 skipped；`REQUIRE` → 失败 |
| CLI | **74 项通过**（0 skipped）；无运行时 → 58 通过 + 1 skipped；`REQUIRE` → 失败 |
| UI 布局 | **46/46 场景**（每场景先跑 `layoutcheck` 断言再渲染；新增编码中/失败状态） |
| 编码分类 | 缺失 / 无效 / 超时 / 损坏输入 / Unicode 路径均有断言 |
| macOS | Universal 构建、DMG 3.0.1、`auval` PASS（aufx Maln OrcA）；本地 DMG SHA-256 `dfb5afb0ca2b927ab9a37f3d1effecfeb8b6ef798c1394282fbbe69e1b846644` |
| 截图 | 8 张（6 主图 + 编码就绪/失败），`docs/screenshots-3.0.1.json` 记录版本、尺寸、参数与 SHA-256 |
| CI（tag `v3.0.1-rc.1`） | run `42405381902`：macOS / Windows x64 / ARM64 / publish 全绿；`SHA256SUMS.txt` 三资产校验通过 |
| CI（push main） | run `42405374496`：macOS + Windows 快速门禁全绿 |
| 运行时验证 | `verify-runtime.sh` 正确拒绝 Homebrew 的 `--enable-gpl` 构建 |

### 本环境无法完成的项

- **随包运行时二进制**：无网络、无交叉工具链，且本机仅有 GPL 构建。本 prerelease 不含 FFmpeg；打包/门禁代码已完成。
- 真实宿主矩阵（Logic / REAPER / Cubase）与 ≥2 小时真实文件持续播放：未执行。
- Windows ARM64 实机、Apple 公证：未执行。

## 四、指导文档 §十 状态表

| 必须完成的事项 | 状态 | 证据与缺口 |
|---|---|---|
| 真实宿主与长时间运行验收 | 未完成 | 缺宿主环境；未执行听感/波形/2 小时持续播放 |
| 修复“测试失败却绿灯” | 完成 | 缺失/失败/超时/损坏输入断言；`REQUIRE` 失败路径；skipped 单独计数 |
| push / PR 与正式发布自动门禁 | 完成 | push 门禁 run `42405374496` 全绿；发布门禁 run `42405381902` 全绿并校验三资产 SHA-256；稳定版仍会因缺少随包运行时被拒绝（设计如此） |
| 当前版本文档与全部截图 | 完成 | 3.0.1 文档、截图清单与哈希、链接检查（见下） |

## 五、范围与限制

- 本版不改变响度、真峰值、文件 QC、参考播放与默认旁路行为。
- 编码仅本地执行，不覆盖源音频；临时文件在成功、失败、取消后清理。
- 编码 A/B 的时间对齐仍是「同一时间码起播 + 5 ms 淡入」，未验证编码器延迟补偿；这是 3.0.2 的待办。
- macOS ad-hoc 签名、未公证；Windows ARM64 交叉构建未实机验证。

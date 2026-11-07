# MasterLens 3.0.1（rc.1）

编码后端统一，发布门禁补齐。

3.0.1 是编码链路的基础版本：GUI、文件 QC 后台任务和 CLI 现在共用同一个 `FFmpegRuntime`，默认只认随包运行时，不再依赖 Homebrew 或 PATH；编码器能力、版本和许可在启动时探测并显示。包装、CI 与测试也按「发布包必须带运行时」重写。

## 用户可见变化

- **默认随包运行时**：解析顺序为 `MASTERLENS_FFMPEG`（显式覆盖）→ 插件/独立版包内 `ffmpeg` → 仅开发构建允许 PATH（`MASTERLENS_ALLOW_SYSTEM_FFMPEG=1`）。显式路径无效时会明确报错，不再静默回退。
- **后端可见**：编码菜单、就绪提示、CLI `Codec backend:` 行和 JSON `backend` / `ffmpegVersion` 都显示实际运行时与版本；GPL/nonfree 构建会被标记「do not redistribute」。
- **失败分类**：运行时缺失、无法启动、编码/解码失败、超时、取消、无效输出分别报错；超时按文件长度放宽，并检测 20 s 无输出。
- **Unicode 路径**：CLI 改用 UTF-8 / Windows 宽字符 argv，中文与空格路径可正常编码。
- **不伪造结果**：开发环境没有运行时只跳过并计入 `skipped`；`MASTERLENS_REQUIRE_RUNTIME=1`（发布门禁）下缺失运行时直接失败。

## 发布门禁与打包

- 新增 `scripts/fetch-ffmpeg.sh`（固定版本 / 校验值 / LGPL 检查）与 `scripts/verify-runtime.sh`（拒绝 `--enable-gpl` / `--enable-nonfree`，要求 native AAC）。
- macOS：DMG 内 VST3、AU、Standalone 均带 `Contents/Resources/ffmpeg`，先签内部再签外层；Windows：安装器写入 `Program Files\MasterLens\ffmpeg` 和 VST3 `Contents\Resources\ffmpeg`，并与卸载器一起清理。
- CI：push/PR 快速门禁（构建 + DSP/CLI/布局）；发布门禁要求运行时存在，缺少时**稳定版失败**，prerelease 会在说明中标注。
- `packaging/THIRD-PARTY-NOTICES.md` 记录许可、源码获取与替换方式。

## 本 prerelease 的运行时缺口

本次构建环境**没有网络，也没有 Windows / FFmpeg 交叉工具链**，无法产出可再分发的 LGPL 运行时；Homebrew 的 ffmpeg 是 `--enable-gpl --enable-version3` 构建，明确不能随包。因此：

- 本 prerelease 的安装包**不包含** FFmpeg 二进制，编码 A/B 仍需要本机 ffmpeg（或自行设置 `MASTERLENS_FFMPEG`）。
- 随包结构、验证脚本、安装器与 CI 门禁已经完成；补齐运行时后即可产出满足指南的稳定版。
- 详细证据与未验证项见 `docs/performance-3.0.1.md`。

macOS 继续 ad-hoc 签名、未公证；Windows ARM64 仍为交叉构建。详细验证见 `docs/performance-3.0.1.md`。

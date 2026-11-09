# MasterLens 3.0.2

发布日期：2026-11-09  
版本：3.0.2（稳定版，自带 FFmpeg 运行时）

## 这一版做了什么

3.0.1 把编码链路的抽象、失败分类、打包结构与发布门禁做好了，但没有可再分发的
FFmpeg 二进制。3.0.2 补齐这块：**安装包直接包含精简的 LGPL FFmpeg**，安装后无需
Homebrew、PATH 或手动配置即可使用 AAC 256k / MP3 320k 编码 A/B 与 CLI 编码复测。

- **运行时版本**：FFmpeg 9.0.2（`n9.0.2` 源码），MP3 编码器 LAME 3.100。
- **许可**：LGPL-2.1-or-later；`scripts/verify-runtime.sh` 拒绝 `--enable-gpl` /
  `--enable-nonfree` 构建。许可证、源码地址与 SHA-256 随二进制分发。
- **裁剪范围**：只保留 PCM / FLAC / MP3 / AAC / ALAC / Vorbis / Opus 解码，
  AAC / libmp3lame / `pcm_f32le` 编码，WAV / AIFF / FLAC / MP3 / MOV / OGG /
  ADTS 解封装，MP3 / MP4 / iPod / WAV 封装，`file` / `pipe` 协议与
  `aresample` / `aformat` / `anull` 滤镜；无网络、无视频、无 ffplay / ffprobe。
- **体积**：macOS Universal 约 6.6 MB；Windows x64 约 3.4 MB，ARM64 约 2.8 MB。DMG 约 37 MB。
- **可复现构建**：`scripts/build-ffmpeg-runtime.sh` 固定源码 URL 与 SHA-256、
  LAME 补丁和 configure 参数；`third_party/ffmpeg/<平台>/build-info.txt` 记录
  实际构建参数。
- **门禁**：发布与 PR 构建直接校验仓库内运行时；存在运行时时测试以
  `MASTERLENS_REQUIRE_RUNTIME=1` 运行，不再出现“缺依赖就算跳过”。

## 用户可见变化

- 首次安装后即可在文件 QC 里生成 AAC / MP3 编码副本并做 A/B，不需要额外软件。
- 编码菜单与关于页显示随包 FFmpeg 的实际版本与后端来源。
- 安装包增大约 9 MB（运行时随三份插件包与安装根目录各放一份）；其余界面、测量与默认旁路行为不变。

## 安装包

- `MasterLens-3.0.2-macOS.dmg`（Universal，VST3 / AU / Standalone / CLI）
- `MasterLens-3.0.2-Windows-x64.exe`
- `MasterLens-3.0.2-Windows-ARM64.exe`
- `SHA256SUMS.txt`

macOS 仍为 ad-hoc 签名、未公证；首次打开如被拦截，请右键 → 打开。Windows ARM64
仍为交叉构建，未在 ARM64 实机验证。

## 验证

完整记录见 [performance-3.0.2.md](performance-3.0.2.md)。摘要：

- DSP / 可靠性断言 **178 passed / 0 failed / 0 skipped**（`REQUIRE_RUNTIME=1`）。
- CLI 集成 **76 passed**（随包运行时）。
- UI 布局 **46/46 场景**。
- `auval` PASS；DMG 内三份插件包各自携带运行时，签名顺序为内部运行时 → 插件包 → DMG。
- 安装包内运行时通过 `scripts/verify-runtime.sh` 的版本、LGPL、AAC 与
  Universal 架构检查，并在移除 PATH 的环境里完成一次 MP3 / AAC 编解码往返。

## 已知限制

- 真实宿主（Logic / REAPER / Cubase）与 ≥2 小时长文件持续播放仍未验收。
- macOS 未做 Developer ID 签名与公证；Windows 未做代码签名。
- Windows ARM64 产物由 llvm-mingw 交叉构建，CI 仅能在 x64 上解包检查，未实机运行。
- 编码 A/B 的编码器延迟补偿仍未实现，切换点以编码文件时间轴为准。

## 升级说明

覆盖安装即可。3.0.1 及更早版本若通过 `MASTERLENS_FFMPEG` 指向自定义运行时，3.0.2
仍然尊重该覆盖；移除覆盖后会自动使用随包版本。旧的系统 ffmpeg 不再需要。

# MasterLens 3.0.2：随包 FFmpeg 与回归记录

日期：2026-11-09  
构建：Universal（arm64 + x86_64）、Release  
运行时：FFmpeg 9.0.2（LGPL-2.1-or-later）+ LAME 3.100

## 一、结果摘要

| 项目 | 结果 |
| --- | --- |
| DSP / 可靠性断言 | **178 passed / 0 failed / 0 skipped**（`MASTERLENS_REQUIRE_RUNTIME=1`） |
| CLI 集成 | **76 passed / 0 failed / 0 skipped**（随包运行时，PATH 中无 ffmpeg） |
| UI 布局 | **46/46 场景**，`docs/layout-3.0.2.json` 共 92 条记录，0 失败 |
| `auval` | **PASS**（aufx Maln OrcA，DMG 内 AU 复制后 ad-hoc 重签） |
| DMG | 约 35 MB，三份插件包与安装根目录各带 `ffmpeg`，内部签名有效 |
| DMG 内 CLI | 无需 `MASTERLENS_FFMPEG` / PATH，AAC 编码成功，`backend=bundled`，版本 9.0.2 |

## 二、随包运行时

- **来源**：`n9.0.2` 源码归档
  `https://codeload.github.com/FFmpeg/FFmpeg/tar.gz/refs/tags/n9.0.2`
  （SHA-256 `6e374ed621e48faa40639307dff48ba6fe574a509977956d2cce9669b7cc27e9`）。
- **MP3 编码器**：LAME 3.100（SHA-256 `ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e`），
  应用 `packaging/patches/lame-3.100-c-only-xrpow-init.patch` 以在无 NASM 对象时保持可链接。
- **构建脚本**：`scripts/build-ffmpeg-runtime.sh`。macOS 分别以 `-arch arm64` / `-arch x86_64`
  构建后 `lipo`；Windows 使用 llvm-mingw 20261104（SHA-256 `66cbd517…6bf3c`）交叉构建
  x64 与 ARM64。
- **裁剪**：`--disable-everything` 后只启用 file / pipe 协议；WAV / AIFF / FLAC / MP3 /
  MOV / OGG / ADTS 解封装；PCM / FLAC / MP3 / AAC / ALAC / Vorbis / Opus 解码；
  AAC / libmp3lame / `pcm_f32le` 编码；MP3 / MP4 / iPod / WAV 封装；
  `aresample` / `aformat` / `anull` 滤镜。无网络、视频、ffplay、ffprobe。
- **许可证**：configure 输出为 `License: LGPL version 2.1 or later`；
  `scripts/verify-runtime.sh` 在 CI 中再次拒绝 GPL / nonfree。
- **体积**：macOS Universal 6,566,088 字节；Windows x64 3,351,040 字节；
  Windows ARM64 2,830,848 字节。
- **依赖**：macOS 仅链接 `/usr/lib/libSystem` 与系统框架；Windows 仅依赖
  KERNEL32 / SHELL32 / bcrypt 与 UCRT API set，无第三方 DLL。
- **定位**：插件包内 `Contents/Resources/ffmpeg`；DMG 根目录 `ffmpeg` 与 `MasterLensCli`
  同级；Windows 为 `$INSTDIR\ffmpeg\ffmpeg.exe` 与 VST3 `Contents\Resources\ffmpeg`。
  3.0.2 补上 CLI 同级运行时解析（此前仅覆盖插件包路径）。

## 三、验证过程

命令（macOS，Universal 构建目录 `build-universal`）：

```bash
export MASTERLENS_FFMPEG=$PWD/third_party/ffmpeg/macos-universal/ffmpeg
export MASTERLENS_REQUIRE_RUNTIME=1

./build-universal/MasterLensDspTest_artefacts/Release/MasterLensDspTest
python3 Tests/test_cli.py ./build-universal/MasterLensCli_artefacts/Release/MasterLensCli
python3 scripts/check_ui.py --build-dir build-universal
DEVELOPER_DIR=/Library/Developer/CommandLineTools bash scripts/make_dmg.sh
```

- 为证明不依赖开发机，DSP / CLI 在 `env -i PATH=/usr/bin:/bin`（CLI 另加一个只含
  `python3` 的目录）下运行，PATH 中没有 ffmpeg。
- CLI 新增用例：把 `MasterLensCli` 与 `ffmpeg` 复制到同一临时目录、移除
  `MASTERLENS_FFMPEG` 后运行 `--codec aac`，要求 `ok=true` 且 `backend=bundled`。
- DMG 挂载后检查三份插件包的 `Contents/Resources/ffmpeg` 均 `codesign -v` 通过；
  从 DMG 复制 CLI 与 `ffmpeg` 到新目录，在无 PATH 情况下完成 AAC 编码并读取
  `ffmpegVersion=9.0.2`。
- `auval`：将 DMG 内 AU 复制到 `~/Library/Audio/Plug-Ins/Components` 后
  `codesign --force --deep -s -` 重签，`auval -v aufx Maln OrcA` 输出
  `AU VALIDATION SUCCEEDED`。
- DMG SHA-256：`31f2a7effa225f8b38348f7fccebdd15a4618460fca3f31a312f15d9cce1171e`。

### 一个测试稳定性修正

DSP 的 “stream reports buffering” 在并发构建时偶发失败：原用例在第一次
`render()` 之后才采样 `isBuffering()`，缓存文件读得极快时状态已回到 false。
`StreamingPreviewPlayer::start()` 会同步置位缓冲标志，测试改为在 `start()` 返回后
立即锁存，再继续原有的音频/分配断言；三次连续运行均为 0 failed。

## 四、CI 与发布门禁

- 仓库内 `third_party/ffmpeg/<平台>/` 直接提供运行时，`scripts/ci-prepare-ffmpeg.sh`
  优先校验并设置 `MASTERLENS_FFMPEG` / `MASTERLENS_REQUIRE_RUNTIME=1`，不再依赖
  `FFMPEG_*_URL` 仓库变量。
- macOS 发布任务校验 Universal 双架构；Windows 任务在原生 x64 上执行
  `ffmpeg.exe -encoders`，缺失 AAC 会直接失败；ARM64 安装包仍为交叉构建。
- 稳定 tag 缺少运行时仍会被拒绝（`ci-prepare-ffmpeg.sh` 的 tag 分支）。
- 本次 tag 的实际运行与产物校验结果在发布后补记于本节。

## 五、未完成项与限制

- 真实宿主（Logic / REAPER / Cubase）与 ≥2 小时真实文件持续播放：**未执行**。
- macOS 未做 Developer ID 签名与公证；Windows 未做代码签名。
- Windows ARM64 未在实机运行；CI 只能检查 PE 架构与安装包内容。
- 编码 A/B 的编码器延迟补偿仍未实现。
- 运行时按需裁剪，只覆盖本产品支持的输入格式；不承诺作为通用 ffmpeg 使用。

## 六、复现

```bash
# 重新生成三个平台的运行时（需要网络与 llvm-mingw）
scripts/build-ffmpeg-runtime.sh macos-universal
scripts/build-ffmpeg-runtime.sh windows-x64
scripts/build-ffmpeg-runtime.sh windows-arm64

# 校验
scripts/verify-runtime.sh third_party/ffmpeg/macos-universal/ffmpeg macos-universal
```

平台的源码 URL、SHA-256、补丁与 configure 行见
`third_party/ffmpeg/<平台>/build-info.txt`；许可证随二进制分发。

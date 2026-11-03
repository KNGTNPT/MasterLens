# MasterLens 2.3.2：可靠性与验证

2026-11-03，基于 `v2.3.1` / `e03e9612b2934f799b5b7c9fd79131e0defa1ffb`。本轮优先修复测量、失败退出、复核绑定与实时对象生命周期。以下是有边界的工程验证，不是 EBU 认证或全部 DAW 兼容承诺。

## 测量误差与标准素材

完整逐文件数据、源文件指纹、探针 / 脚本指纹及 FFmpeg 版本见 [measurement-2.3.2.json](measurement-2.3.2.json)。共 **89 个文件**，每个分别使用固定 256 样本块及两种确定性随机 1–8192 样本块回放；I/M/S/LRA/TP、时长和峰值时间的块大小最大漂移为 **0**。

独立生成的 25 个立体声向量覆盖 **32 / 44.1 / 48 / 96 / 192 kHz**：997 Hz 标定、动态电平和静音段、非整窗文件尾、最后一个样本的脉冲、近奈奎斯特信号、全静音。I/M/S/LRA 对照独立 FFmpeg `ebur128`；TP 另对完整文件补零后以 double 精度四倍重采样取峰，避免独立工具的 100 ms 输出帧漏掉文件尾部。补零仅用于峰值对照，不计入响度或文件时长。

| 指标 | 生成向量相对独立实现最大绝对误差 | 本轮对照容限 | EBU 指定指标最大绝对误差 | EBU 验收项 |
| --- | ---: | ---: | ---: | ---: |
| I | 0.04136 LU | 0.10 LU | 0.02062 LU | 7 |
| M max | 0.04979 LU | 0.15 LU | 0.00641 LU | 20 |
| S max | 0.04386 LU | 0.15 LU | 0.06626 LU | 20 |
| LRA | 0.10000 LU | 0.50 LU | 0.00001 LU | 6 |
| TP | 0.05990 dB | 0.30 dB | 0.16863 dB | 9 |

**EBU v5 已实际运行**：选取包内 64 个单声道 / 立体声 `seq-3341-*` / `seq-3342-*` 文件；其中 60 个文件有 62 项指定指标验收，全部通过。I/M/S 容限 ±0.1 LU、LRA ±1 LU、TP −0.4/+0.2 dB。另 4 个序列（9 / 11 / 12 / 14）保留独立实现结果与随机缓冲区检查，不将其登记为本轮指定指标通过。多声道文件不在产品支持和本轮范围内。

每个 EBU 文件同时记录所有可测指标，但验收使用该序列指定的指标。例如 TP 校验序列上的 FFmpeg LRA 初始化差异也完整保留，不能把这些辅助输出计入“LRA 标准测试通过”或拿来替代指定 LRA 序列。M max 使用 **400 ms 精确滑动窗 / 10 ms 更新**；I 仍用完整 400 ms 门控窗与 100 ms 步长，S 为完整 3 s，LRA 以 100 ms 采样。标准序列曾暴露旧 M 更新节奏漏峰最高约 0.45 LU，修复后重新运行，并未放宽指定指标容限。

素材来源：[EBU Loudness Test Set](https://tech.ebu.ch/publications/ebu_loudness_test_set)，**© EBU**，仅内部研发验证。官方站下载返回 403，最终由南京大学 Gentoo 镜像获取；SHA-512 与 [Gentoo Manifest](https://github.com/gentoo/gentoo/blob/master/media-libs/libebur128/Manifest) 完全一致。音频不提交仓库、不随安装包或 Actions 报告分发。

- archive SHA-256: `9cc500b4df83f7c21855c74dce795ef5209a752bf884253ae57d0ce512efb062`
- archive SHA-512: `60d022fdac47ad0be2688411be9daecbff85da994d6fa4921bba6cffab841b081d8b15d9ce284ad2253efb686463450a84a0d19cb0bad7a934546cc52dd73771`
- 算法与序列依据：[EBU Tech 3341](https://tech.ebu.ch/publications/tech3341)、[Tech 3342](https://tech.ebu.ch/publications/tech3342)；指定值映射亦与 [libebur128 tests.c](https://github.com/jiixyj/libebur128/blob/master/test/tests.c) 交叉核对。

文件结束丢弃不完整的响度门控切片，真峰值滤波器单独补零排空延迟；尾部最后一个样本不会再被预听器跳过。实时遇到 NaN / Inf 会标记测量无效，分析端以零代替异常样本以保护后续滤波状态，默认旁路原音频不被修改。

## 结果与证据

- **144 项 DSP / 可靠性断言通过**。覆盖分数区间并集、重复循环、真实空洞、未知曲长、容量溢出、项目隔离、独立项目 ID、原样恢复快照、复核持久化、新源文件拒绝旧复核、未知 / 未完整覆盖拒绝 PASS、异常采样、错位短脉冲取峰及既有 DSP / QC 回归。
- **66 项 macOS CLI 集成检查通过**，新增真实 `--spec` 和 AAC / MP3 `--codec` 端到端路径、缺失编码器、编码器超时、报告不能覆盖输入、无效规格及失败 JSON / 退出码。Windows x64 执行同套测试，Unix 专用的超时模拟两项不在 Windows 执行。
- 更正旧记录的范围：2.3.1 的 **41 项 CLI 检查并没有覆盖新增 `--spec` / `--codec` 端到端流程**；当时关于工作流覆盖的措辞过宽。本轮才加入实际编码包、解码与测量断言。
- 覆盖度改为固定容量 4096 个宿主时间区间的精确并集。未知位置 / 曲长或区间容量溢出返回未知；不把循环时长累加为整首歌。未完整覆盖不通过，临近 100% 的浮点舍入不能冒充完整。
- 项目由每个处理器实例拥有，默认不再共享全局 `album.json`。项目打开 / 宿主状态恢复保留原快照的哈希、版本和时间；项目文件原子写入成功才更新当前快照。
- 复核 ID 绑定源 SHA-256、分析版本、事件代码、起点、长度和声道，复核状态随项目保存。旧版本 / 新内容需要重新复核；同内容搬迁可继续识别来源。采集后的文件分析前后均校验 SHA-256；编码往返也校验输入内容与原始分析一致。
- AAC → M4A、MP3 → MP3 实际编码后分别解码为 float WAV，再用正常文件分析器测量；报告保存源 / 编码 / 解码指纹与包体大小。失败退出 1，规格 / 峰值门禁不合格退出 2，不会显示成功。临时音频自动清理。
- 证明显示 PASS / INCOMPLETE / BLOCKED。报告 / 规格 / 项目导出检查写入结果，主要单文件使用临时文件替换；多格式成组导出若部分失败会明确提示，尚不是整组文件事务。

证明是**当时已分析内容的快照**；它不是对磁盘当前文件的持续认证。改变源文件后请重新分析，报告内的源指纹用于核对当时内容。

## 实时安全与生命周期

参考轨与证据预听改为单音频读者 / 单工作线程发布的 hazard-pointer 交换：工作线程保有所有权并回收旧缓冲，音频线程只持有短期非 owning 指针。发布竞争时跳过该块，不在音频回调重试、等待或释放 owning 引用。元数据锁仅用于工作线程与 UI。

- 20,000 次并发素材发布未读到损坏数据；参考加载 / 切换 / 停止、预听与引擎处理的受测回调 **C++ new/delete 为 0**。这是线程局部 C++ 分配探针，不宣称截获了系统全部 malloc 或驱动分配。
- processBlock 及其实时分析 / 试听路径无 FFT、new、Path、锁或 vector 扩容。M 窗储存只在 prepare 分配。GUI / 离线工作中的 FFT 与容器不属于音频回调。
- 参考 A/B 使用 5 ms 交叉淡变，输入与参考分别等响，输出保留 float 余量。当前参考仍是四点 Catmull-Rom，证据预听仍是线性重采样；不宣称已经完成高规格 sinc 重采样、局部精确等响或所有循环边界无缝处理。
- Debug 的 48 个 DynamicObject 退出生命周期断言已经消除，根因是全局静态 Session 保留 var 树；改为实例所有权，未关闭断言。UI 文件选择回调加入组件生命周期检查。
- 本机 Release 60 秒立体声 DSP 基准约 0.160 秒 / 单核心 0.27%，这是独立引擎基准，不是所有宿主的总 CPU 或长时间稳定性保证。

## UI、插件验证与发行边界

浅色频谱采用深石墨测量面，主曲线提高不透明度；点击释放的光晕与阻尼回弹增强，命中区域不变。14 / 16 / 26px 字体与整张等比缩放继续保留。35 组实际页面 / 缩放场景见 [layout-2.3.2.json](layout-2.3.2.json)，真实 Release 截图与指纹见 [screenshots-2.3.2.json](screenshots-2.3.2.json)。

关于页 `Resources/brand/Berrystudio.svg` **未重新绘制、字节不变**，SHA-256：`321f972b9e0336adedb0f015a159d7516be8ab69a09ed43e55b83dbd107283ae`。

- macOS Universal AU / VST3 / Standalone / CLI 构建和本地 DMG 打包通过；Debug arm64 保持断言。
- Apple Silicon 上 **pluginval 1.0.4 Level 5**（默认 44.1/48/96 kHz、64–1024 样本块，包含 GUI）通过：[日志](validation-2.3.2/pluginval.txt)。
- **auval `aufx Maln OrcA`** 对临时安装的 2.3.2 AU 通过：[日志](validation-2.3.2/auval.txt)。验证后已恢复本机原先安装的 2.3.1，避免把测试安装冒充用户主动升级。
- 正常发行包含 macOS Universal、Windows x64、Windows ARM64 三个安装包；Actions 的原生 x64 执行 DSP / CLI，ARM64 交叉编译。
- **手头没有 Windows on ARM 设备，尚未经 Windows on ARM 环境实机测试。** Intel Mac、完整 DAW 兼容矩阵、长时间音频设备压力、旧工程大规模升级回归仍需补测。
- macOS 继续 ad-hoc 签名，当前没有 Developer ID，未公证；Windows 未做商业代码签名。这些发行覆盖限制不会因自动测试通过而消失。

## 复现

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MasterLensDspTest MasterLensCli MasterLensUiShot -j4
build/MasterLensDspTest_artefacts/Release/MasterLensDspTest
python3 Tests/test_cli.py build/MasterLensCli_artefacts/Release/MasterLensCli
python3 Tests/fetch_ebu.py .tools/ebu
python3 Tests/validate_measurements.py build/MasterLensDspTest_artefacts/Release/MasterLensDspTest --ebu-dir .tools/ebu
python3 scripts/check_ui.py --build-dir build
```

需要 FFmpeg、JUCE 8.0.15 和相应平台构建依赖。EBU 音频只作内部验证；下载不可用时脚本失败并明确未运行，不会自动把官方素材验收写成通过。仅使用生成向量时去掉 `--ebu-dir`，报告会标明没有 EBU 素材。

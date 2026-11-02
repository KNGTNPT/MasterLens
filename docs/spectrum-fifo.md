# 主输出频谱 FIFO

`Source/dsp/SpectrumFifo.h` 新增独立输出采集通道。`processBlock()` 最后将已完成试听、参考轨替换及等响 trim 的主输出送入 FIFO：立体声取 `(L + R) / 2`，单声道原样采集。既有输入频谱、响度、真峰值及 M/S 测量代码保持原样。

## 线程与数据契约

- `juce::AbstractFifo` 管理单生产者 / 单消费者的完整帧队列。固定 8192 点、2048 点 hop（75% 重叠），最多排队 4 帧。
- 音频线程仅混单声道、写入预分配历史缓冲、复制并发布完整帧；不分配内存、不加锁、不做 FFT 或加窗。
- 队列满时丢弃新显示帧，继续积累最新历史，音频输出不受影响。UI 恢复消费后释放旧队列，下一个可发布帧继续来自最新音频。不会由生产者覆盖仍在读取的帧。
- UI 定时器消费全部已发布帧，仅缓存其中最新一帧。`SpectrumFifo::Reader` 在 UI 侧应用预先计算的三项 Blackman 窗（alpha 0.16，不做窗口归一化）。
- `getFrame()` 为原始混单声道数据；`getWindowedSamples()` 为加窗结果。后续未归一化 FFT 可用 `getFftMagnitudeScale()`（`2 / sum(window)`）校正 bin-centred 正弦的幅度。
- `endSample` 是当前 prepare 周期内帧尾的排他样本编号，帧范围为 `[endSample - 8192, endSample)`；`sampleRate` 与 `generation` 随帧发布。它不是宿主绝对时间码。
- prepare 时不跨线程 reset AbstractFifo。旧周期帧通过 generation 拒绝；新周期收满一帧后恢复有效数据。
- 仅由编辑器消费 FIFO。调试工具读取 UI 自有缓存，不能成为第二个消费者。`getOutputSpectrumForDebug()` 仅在 Debug 构建中开放。

本阶段接通采样传输和 UI 缓存，现有输入频谱图继续使用原来的分析器；不把旧分析器切换到试听后的输出。UI 的 Blackman 缓存可供后续输出频谱绘制使用。

## Debug 验收

使用仓库的 CMake 可执行文件（或本机 CMake）配置：

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-debug --target MasterLensUiShot MasterLens_VST3 MasterLens_AU MasterLens_Standalone MasterLensDspTest -j 4
./build-debug/MasterLensUiShot_artefacts/Debug/MasterLensUiShot.app/Contents/MacOS/MasterLensUiShot spectrumfifo
./build-debug/MasterLensDspTest_artefacts/Debug/MasterLensDspTest
```

`spectrumfifo` 模式执行以下检查：

1. 输出侧 +6dB trim 已进入 FIFO，而测量峰值仍来自原始输入。
2. 单声道、反相抵消、超大宿主块、队列满、环绕、溢出后恢复及采样率周期切换。
3. 独立音频线程以 48kHz、127 / 511 / 64 / 1024 可变块长调用真实 `processBlock()`；真实编辑器定时器持续读取。输入在一秒后改变频率，并根据帧样本编号逐点核对原始数据和 Blackman 结果。
4. 默认输出逐样本不变、零延迟；同步检查不从第二条线程直接消费 FIFO。

2026-11-02，Apple Silicon macOS / JUCE 8.0.15 的本地结果：

- Debug VST3、AU、Standalone、UiShot 与 DSP 测试目标全部编译通过。
- FIFO 前置检查通过；UI 读取 46 帧，原始样本最大误差 0，Blackman 最大误差 `6.15e-8`，无队列溢出，默认旁路通过。
- 帧尾相对于已处理输出样本的最大落后为 35.17ms。该值不包含分析窗跨度；8192 点窗在 48kHz 下覆盖约 170.67ms，首次完整帧需等待同样时长。这不是插件添加的音频延迟。
- 原有 108 项 DSP / 证据检查全部通过。

Debug UI 进程返回 0，但退出时 JUCE 仍报告 CoreTextTypeface / Typeface / Font、DynamicObject 和 MemoryBlock 泄漏断言。现有 `Theme.cpp` 明确保留字体缓存以规避卸载顺序问题；本次未修复退出阶段的对象生命周期，因此不把此次同步检查描述为「无断言的 Debug 退出验收」。

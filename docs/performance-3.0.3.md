# MasterLens 3.0.3：界面规范与验证记录

日期：2026-11-12  
构建：Universal（arm64 + x86_64）、Release  
范围：只改界面与设计规范；DSP、监听、随包 FFmpeg 与 CLI 行为与 3.0.2 相同

## 一、这版改了什么

3.0.3 来自一轮 UI 评审。重点不是换风格，而是把“能读”变成“稳定地读”：字号层级、浅色对比度、数字与分隔符、中文页面里的英文残留、浮层构图和最小缩放。

- **四档排印**：Label 14 / Value 18 / Title 20 / Critical 30（JUCE 字体高度，落屏约 11 / 14 / 16 / 24 px）。新增 `getTitleFont()`；页面标题用 Title，元信息与页脚回到 Label；Critical 仍只给 LUFS / 真峰值，同屏最多两处。
- **标签不再机械加字距**：`labelFontFor()` 只对全大写拉丁文案使用 0.04em tracking；中文与句子式正文保持 0 字距。`Layout::alignChildren()` 与按钮字体也走同一判定。
- **浅色主题状态色**：`good #0d6b47`、`warn #8a4f00`、`bad #b3261e`、`cyan #355b68`、`violet #5a4bd3`，均按承载 14 px 文字校准；新增 `accentInk`，状态胶囊改为中性底 + 1px 语义色描边，不再铺 14% 同色底。
- **频谱轴**：格线与曲线共用 `spectrumDbToT()`，范围 +6…−90 dB；补页面标题行，轴标签使用 U+2212。
- **数字与分隔符**：U+2212 负号、en dash 范围、负零归零、`7.8%` / `97/100` 紧贴、时间区间两侧留空；`theme::sep()` 统一为 `·`，不再混 `|`。
- **构图**：关于浮层 350 px、设置 430 px；平台页无参考时不画空卡；信号卡风险条移入文字流；空量表轨道使用 `theme::track`。
- **缩放**：`minScale` 0.8 → 1.0，档位 100 / 125 / 150 / 175 / 200%；宿主约束 1120×700 – 2240×1400，`setBoundsConstrained(800×500)` 被拒绝。

规范同步重写：[`DESIGN.md`](../DESIGN.md) 与 [`FRONTEND-DESIGN.md`](../FRONTEND-DESIGN.md)。

## 二、验证结果

| 项目 | 结果 |
| --- | --- |
| UI 布局 | **46/46 场景通过**；`docs/layout-3.0.3.json` 92 条记录，0 失败 |
| 字号断言 | 14 / 18 / 20 / 30；标签与数值差 ≥ 4；Title/Value/Critical 顺序正确 |
| 宿主 resize | 最小 1120×700、最大 2240×1400；过小/过大被约束 |
| DSP / 可靠性 | **178 passed / 0 failed / 0 skipped**（`MASTERLENS_REQUIRE_RUNTIME=1`） |
| CLI 集成 | **76 passed / 0 skipped**（随包运行时，PATH 无 ffmpeg） |
| `auval` | **PASS**，`Cocoa Views Available: 1` |
| 截图 | 8 张，`docs/screenshots-3.0.3.json` 记录参数、尺寸、渲染器与 SHA-256；`architecture` 由 `lipo -info` 得出 `universal (x86_64 + arm64)` |

复跑命令：

```bash
export MASTERLENS_FFMPEG=$PWD/third_party/ffmpeg/macos-universal/ffmpeg
export MASTERLENS_REQUIRE_RUNTIME=1

./build-universal/MasterLensDspTest_artefacts/Release/MasterLensDspTest
python3 Tests/test_cli.py ./build-universal/MasterLensCli_artefacts/Release/MasterLensCli
python3 scripts/check_ui.py --build-dir build-universal
```

## 三、发布状态与限制

- 3.0.3 目前**只有源码与本地构建**，没有 tag / Release；安装包仍以 3.0.2 为准。
- 真实宿主（Logic / REAPER / Cubase）与 ≥2 小时持续播放未验收。
- macOS 仍为 ad-hoc 签名、未公证；Windows ARM64 仍为交叉构建，未实机运行。
- 编码 A/B 的编码器延迟补偿仍未实现（与 3.0.2 相同）。

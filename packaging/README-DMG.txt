MasterLens 2.0.1 —— 界面重做 · 质感与性能预算（macOS · VST3 / AU / 独立版）
by BerryStudio · 开发者 KNGTNPT
=====================================================================

【安装】
  1. 双击「安装 Install.command」。如系统提示“无法打开，因为来自身份不明的开发者”，
     请右键该文件 → 打开 → 仍要打开。
  2. 脚本会复制：
        ~/Library/Audio/Plug-Ins/VST3/MasterLens.vst3
        ~/Library/Audio/Plug-Ins/Components/MasterLens.component
     独立版复制到 ~/Applications/MasterLens.app
  3. 重启 DAW，在厂商 BerryStudio 下搜索 MasterLens（AU 名称亦为 BerryStudio: MasterLens）。

【工作方式】
  默认零延迟、默认旁路，只测量不改变音频；只有点击信号卡片、问题时间戳、开启
  「等响监听」或「参考 A/B」时，输出才会临时改变。信号卡片实时跟随 DAW；
  问题时间戳循环指定片段。Esc 停止。

【2.0.1 新增：界面重做与性能预算】
  · 暖石墨底 + 单一铜色强调；三层表面、无卡片描边、4 / 8 px 圆角。
  · 左栏加宽到 208 px，短标签不再截断；顶部设置条压到 36 px，参考响度与
    真峰值上限可直接键入。
  · 顶部常驻输出状态：BYPASS / MATCH / REFERENCE / AUDITION。
  · 文件 QC 表格可滚动、表头排序、点击行看详情、导出 CSV；文件 / 专辑页
    自动隐藏直播时间线并显示 OFFLINE。
  · 立体声页改为 4 px 轨道 + 峰值保持；三段分频改为三列文本。
  · 历史曲线标记叠在曲线上，hover 读数，点击送进试听。
  · 性能：频谱页约 2.4% 单核，响度页约 1.4%，paint p95 8.6 ms；详见
    docs/performance-2.0.1.md。

【2.0 新增：频谱图 / 选区 / M/S / PDF / CLI】
  · 频谱页新增「频谱 / 频谱图 / 频段热力」三种视图。
  · 拖动底部整曲时间线选择一段；时间线页显示该段 LUFS / M / S / TP / PLR /
    相关性 / 宽度，并写入报告。
  · 立体声页新增 M / S 独立综合响度；报告与 CLI 同步输出。
  · 导出报告额外生成 .pdf 与 -timeseries.csv（100 ms 原始序列）。
  · DMG 内含 MasterLensCli：MasterLensCli analyze <文件或文件夹> --json out.json。
  · 参考 A/B 新增「粗略对齐」，用短时响度包络找参考段落；非采样级同步。

【2.0 新增：文件 QC / 等响 A/B / 宿主时间线】
  · 「文件 QC」页（快捷键 F）：拖入 WAV / AIFF / FLAC 或专辑文件夹，后台按
    文件原生采样率分析，自动写入专辑表与门禁；12 首 × 4 分钟不再实时等。
  · 底栏「等响监听」：按目标 / 参考 LUFS 自动 trim；频谱页「参考 A/B」：
    在插件内按宿主 playhead 播放参考文件，默认关闭、关闭时逐位旁路。
  · 事件时间戳改用宿主 playhead；专辑页可填「总长 s」，覆盖度 <95% 阻断门禁。
  · 专辑快照随工程保存，并保留 album.json 与 Album_<name>.json sidecar。
  · 100% 界面重排：字号更清楚、行高更紧、统计页填满；立体声页新增
    L / R / Side 真峰值与单声道折叠卡片。
  · 越界问题时间戳不再静默播最近片段，会明确提示从该位置重播。

【1.5.1 新增：实时 Solo / 问题区域 / 曲风目标】
  · 信号健康六张卡片：点击后从当前总线输入实时滤波，跟随 DAW 播放头；
    暂停 / 停止时立即静音，不再循环历史片段。Esc 或再次点击停止。
  · 卡片下方「问题区域」：检出越线时自动记录带时间戳的 2 s 区域；
    点击时间戳循环该段，按对应频段滤波并做响度匹配。
  · 「曲风目标 / 交付」页：选择曲风，拖动“响度战争参与度”滑块
    （50% = 曲风标准；0% = 最安静的发行观测值；100% = 最响的观测值），
    查看目标 LUFS / 建议 LRA / 真峰值上限；点「应用为目标」写入手动参考。
  · 同页平台交付表：Spotify / Apple Music / YouTube / Tidal / Amazon /
    SoundCloud / 播客 / 俱乐部，含 LUFS、dBTP、编解码器、归一化行为与
    Apple Digital Masters 曲尾 ≥1 s 无波形 / 数字静音检查（测到尾部静音才判定）。
  · 窗口最小 880×560，默认 1120×700；小屏会自动降低过大的旧缩放，避免窗口
    超出 MacBook Air 默认 DPI 的可用区域。



【1.5 的核心变化：不再用固定曲风响度当合格线】
  · 顶部下拉选择主基准：
      平台归一化后果 / 参考轨 / 专辑中位 / 手动参考
  · 平台模式直接显示归一化后的衰减、服务端限幅和被丢弃的响度；
    -4 LUFS 这类超高响度会明确显示“归一化后感知响度不变，动态余量已损失多少”。
  · 参考轨 / 专辑中位模式显示 LUFS、LRA、PLR 与基线的差；偏差只作为信息，
    不自动判不合格。频谱平衡也只对照参考轨或专辑中位，不再内置绝对目标曲线。
  · 真峰值上限仍用于编码风险与专辑门禁；它不再与曲风响度绑定。

【频谱 60 fps】
  频谱页分析路径按 60 Hz 更新（8192 FFT、分数倍频程平滑、峰值保持）；
  其他页面保持低负载刷新（约 30 Hz），HOLD 时停止 FFT。

【专辑统一 / 交付门禁】
  1. 在 DAW 中逐首播放，输入曲目名，点「采集本曲」。采集写入共享专辑文件，
     同一台机器的其他 MasterLens 实例会看到同一份专辑。
  2. 至少采集 2 首后生成专辑中位；表格并列 LUFS / LRA / PLR / TP / Δ中位 / 状态。
  3. 右侧按严重度排序整张专辑的风险：平台归一化、编码 / TP、信号缺陷、
     单声道折叠、与专辑中位的偏离；并显示新增 / 恶化 / 改善的版本 diff。
  4. 底部五个规则开关控制门禁：平台限幅、真峰值、信号缺陷、单声道、中位偏离。
     阈值与规则可通过导入/导出的专辑 JSON 自定义并团队共享。
  5. 「导出证明」输出 JSON + TXT：包含 schema/版本/修订号、每首快照、中位、
     规则、阻断/放行结论、风险排序、版本 diff 与 SHA-256 快照/证据哈希。

【问题联动试听】
  · 信号健康页六张卡片：点击后实时跟随 DAW，滤波所选频段：谐波 2–12k、
    混叠 12–20k、超低 20–60、全频瞬态、单声道 60–250（M 折叠）、掩蔽 200–800。
    停止 / 暂停 DAW 时立即静音，不读取历史循环。
  · 卡片下方「问题区域」时间戳：点击后循环该 2 s 片段，适合反复检查特定问题。
  · 专辑页右侧风险行仍按时间点循环对应频段；试听窗口取最近 1.5 s，
    若风险时间仍在 12 s 历史内则使用该时间点。
  · 试听采用 5 ms 淡入淡出并做峰值保护；默认旁路时输出逐位不变。

【DAW 标记与自动化】
  · 时间线页可「导出标记 CSV / 导入标记 CSV」，与 DAW 的标记表互相转换
    （timecode / seconds / severity / text）。VST3/AU 没有跨宿主标记写回 API，
    因此本版提供 CSV 交换，而不是声称直接写回每家 DAW 的标记轨。
  · 宿主自动化参数：Auto reset on transport start、Auto capture album on stop、
    Auto export proof on stop、Automation command
    （None / Reset / Capture track / Export proof / Export markers / Clear album）。
    重置在音频线程直接生效；采集 / 证明 / 标记需要编辑器打开以访问频谱与文件。

【平台归一化（仅作后果模拟，不是曲风目标）】
  Spotify / Apple Music / YouTube / Amazon / Tidal / SoundCloud / Podcast / EBU R128
  的响度归一化、预测真峰值、服务端限幅与被丢弃响度；不同平台可能不同。

【导出报告】
  右上角「导出报告」输出 TXT / JSON / CSV；报告包含基准模式、平台后果、
  专辑门禁、风险与版本 diff。专辑证明在专辑页单独导出。

【性能参考】
  频谱页约 3.1% 单核（60 fps）；响度页约 1.4%，信号页约 1.5%，
  曲风目标 / 交付页约 1.5%。DSP 约 0.28%。
  数值随机器与系统调度波动；用 MasterLensUiShot bench pageN 复测。

【卸载】
  双击「卸载 Uninstall.command」。

【说明】
  · 通用二进制（Apple Silicon + Intel），最低 macOS 11。
  · 采用 ad-hoc 签名（未做 Apple 公证）。若 DAW 拒绝加载：
        xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/MasterLens.vst3
  · 专辑文件：~/Library/Application Support/BerryStudio/MasterLens/album.json
    （可用 MASTERLENS_ALBUM_FILE 环境变量覆盖，便于测试 / 团队共享快照）。

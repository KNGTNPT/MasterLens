#include "Modules.h"

namespace ml
{
namespace
{
    struct CardSpec
    {
        juce::String title;
        juce::String primary;
        juce::String unit;
        juce::String note;
        float risk = 0.0f;
        bool active = false;
        std::array<std::pair<juce::String, juce::String>, 3> rows;
    };

    juce::Colour riskColour (float risk)
    {
        return risk >= 65.0f ? theme::bad : (risk >= 35.0f ? theme::warn : theme::good);
    }

    void paintCard (juce::Graphics& g, juce::Rectangle<int> area, const CardSpec& c)
    {
        const auto r = area.toFloat().reduced (0.5f);
        const bool compact = area.getHeight() < 96;

        g.setColour (c.active ? theme::accent.withAlpha (0.18f) : theme::cardAlt);
        g.fillRoundedRectangle (r, tokens::radiusCard);
        g.setColour (c.active ? theme::accent : theme::border);
        g.drawRoundedRectangle (r, tokens::radiusCard, c.active ? 1.5f : 1.0f);

        if (c.active)
        {
            g.setColour (theme::accent);
            g.fillRoundedRectangle ((float) area.getX() + 1.0f, (float) area.getY() + 12.0f,
                                    3.0f, (float) area.getHeight() - 24.0f, 1.5f);
        }

        auto inner = area.reduced (14, compact ? 8 : 11);
        auto titleRow = inner.removeFromTop (compact ? 16 : 18);

        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawText (c.title, titleRow.withTrimmedRight (78), juce::Justification::centredLeft, false);

        const auto colour = riskColour (c.risk);
        const auto chip = juce::Rectangle<int> (titleRow.getRight() - 72, titleRow.getY() - 2, 72, 18);
        const auto status = c.risk >= 65.0f ? T ("需修复", "FIX")
                          : (c.risk >= 35.0f ? T ("建议回听", "REVIEW") : T ("通过", "PASS"));

        g.setColour (colour.withAlpha (0.14f));
        g.fillRoundedRectangle (chip.toFloat(), 9.0f);
        g.setColour (colour);
        g.setFont (getValueFont());
        g.drawText (status, chip, juce::Justification::centred);

        auto valueRow = inner.removeFromTop (compact ? 24 : juce::jmin (38, inner.getHeight() / 2));

        g.setColour (colour);
        g.setFont (getValueFont());
        g.drawText (c.primary, valueRow, juce::Justification::centredLeft, false);

        if (c.unit.isNotEmpty())
        {
            const auto font = getLabelFont();
            const int valueWidth = juce::roundToInt (
                juce::GlyphArrangement::getStringWidth (getValueFont(), c.primary));

            g.setColour (theme::textFaint);
            g.setFont (font);
            g.drawText (c.unit, valueRow.withTrimmedLeft (valueWidth + 6).withTrimmedTop (6),
                        juce::Justification::centredLeft, false);
        }

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (c.note + T (" · 实时试听", " · live audition"),
                    inner.removeFromTop (compact ? 14 : 16), juce::Justification::centredLeft, false);

        if (! compact)
        {
            inner.removeFromTop (4);

            for (const auto& row : c.rows)
            {
                if (inner.getHeight() < 18)
                    break;

                auto line = inner.removeFromTop (20);

                g.setColour (theme::textFaint);
                g.setFont (getLabelFont());
                g.drawText (row.first, line.removeFromLeft (96), juce::Justification::centredLeft, false);

                g.setColour (theme::text);
                g.setFont (getValueFont());
                g.drawText (row.second, line, juce::Justification::centredRight, false);
            }
        }

        const auto track = juce::Rectangle<float> ((float) area.getX() + 14.0f,
                                                    (float) area.getBottom() - 9.0f,
                                                    (float) area.getWidth() - 28.0f, 3.0f);
        g.setColour (theme::borderSoft);
        g.fillRoundedRectangle (track, 1.5f);
        g.setColour (colour);
        g.fillRoundedRectangle (track.withWidth (track.getWidth()
                                    * juce::jlimit (0.0f, 1.0f, c.risk / 100.0f)), 1.5f);
    }
} // namespace

juce::String signalAuditionLabel (int index)
{
    static const char* zh[] { "谐波 / 互调 2–12 kHz", "混叠 12–20 kHz", "超低 20–60 Hz",
                              "全频瞬态", "单声道 60–250 Hz", "掩蔽 200–800 Hz" };
    static const char* en[] { "harmonics / IMD 2-12 kHz", "aliasing 12-20 kHz", "sub 20-60 Hz",
                              "full-band transient", "mono 60-250 Hz", "masking 200-800 Hz" };
    index = juce::jlimit (0, 5, index);
    return T (zh[index], en[index]);
}

//==============================================================================
void DetailArea::paintSignal (juce::Graphics& g)
{
    auto area = contentArea().withTrimmedTop (32);
    const auto& d = state.defects;
    const auto& s = state.stats;

    // ---- verdict strip ------------------------------------------------------
    {
        auto strip = area.removeFromTop (40);
        area.removeFromTop (8);

        const float worst = d.worstRisk();

        const auto colour = ! d.ready ? theme::textFaint : riskColour (worst);
        const auto headline = ! d.ready ? T ("正在积累频谱…", "Accumulating spectrum...")
                            : worst >= 65.0f ? T ("信号已出现可测量损伤", "Measurable signal damage")
                            : worst >= 35.0f ? T ("信号处于边缘，建议逐层核对", "Signal is borderline")
                                             : T ("信号本身完整", "Signal is intact");

        g.setColour (colour.withAlpha (0.10f));
        g.fillRoundedRectangle (strip.toFloat(), tokens::radiusCard);
        g.setColour (colour.withAlpha (0.35f));
        g.drawRoundedRectangle (strip.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

        g.setColour (colour);
        g.fillRoundedRectangle ((float) strip.getX() + 12.0f, (float) strip.getY() + 12.0f,
                                3.0f, (float) strip.getHeight() - 24.0f, 1.5f);

        g.setColour (theme::text);
        g.setFont (getLabelFont());
        g.drawText (headline, strip.getX() + 24, strip.getY() + 6, strip.getWidth() / 2,
                    strip.getHeight() - 12, juce::Justification::centredLeft, false);

        g.setColour (theme::textFaint);
        g.setFont (getValueFont());
        g.drawText (T ("越线层 ", "over ") + juce::String (d.attributionCount)
                        + ml::sep() + T ("最高 ", "max ") + juce::String (juce::roundToInt (d.worstRisk()))
                        + ml::sep() + T ("平顶 ", "flat-top ") + fmt1 (d.flatTopPct) + "%",
                    strip.getX() + 24, strip.getY() + 6, strip.getWidth() - 40,
                    strip.getHeight() - 12, juce::Justification::centredRight);
    }

    // ---- six cards, 3 x 2 ---------------------------------------------------
    auto grid = signalGridBounds;
    auto attribution = signalAttributionBounds;
    const int gap = tokens::gap;
    const int w = (grid.getWidth() - gap * 2) / 3;
    const int h = (grid.getHeight() - gap) / 2;

    const auto cell = [&] (int col, int row)
    {
        return juce::Rectangle<int> (grid.getX() + col * (w + gap),
                                     grid.getY() + row * (h + gap), w, h);
    };

    const auto dbText = [&] (float v) { return sgn (v) + " dB"; };

    CardSpec c0;
    c0.title = T ("谐波 / 互调失真", "HARMONIC / IMD");
    c0.primary = num (d.harmonicExcessDb);
    c0.unit = "dB";
    c0.note = T ("相对局部谱底 2–5 次谐波", "2nd-5th harmonics over local floor");
    c0.risk = d.distortionRisk;
    c0.rows = {{ { T ("互调边带", "IMD sidebands"), dbText (d.imdSidebandDb) },
                 { T ("平顶样本", "Flat tops"), fmt1 (d.flatTopPct) + "%" },
                 { T ("峰响比", "PLR"), fmt1 (s.getPlr()) } }};

    CardSpec c1;
    c1.title = T ("混叠 / 高频完整性", "ALIASING / TOP");
    c1.primary = num (d.hfTiltDb);
    c1.unit = "dB";
    c1.note = T ("16–20k 相对 10–16k 的高频斜率", "16-20k vs 10-16k high-frequency tilt");
    c1.risk = d.aliasRisk;
    c1.rows = {{ { T ("滚降余量", "Roll-off margin"), dbText (d.aliasExcessDb) },
                 { T ("过采样", "Oversampling"), juce::String ("4") + times() },
                 { T ("上限", "Ceiling"), num (state.ceiling) + " dBTP" } }};

    CardSpec c2;
    c2.title = T ("超低频堆积", "SUB BUILD-UP");
    c2.primary = num (d.subSharePct);
    c2.unit = "%";
    c2.note = T ("40 Hz 以下占总能量", "below 40 Hz of total energy");
    c2.risk = d.subRisk;
    c2.rows = {{ { T ("20–40 / 40–80", "20-40 / 40-80"), dbText (d.subTiltDb) },
                 { T ("单声道低频", "Mono low end"), fmt1 (d.monoLowRetainPct) + "%" },
                 { T ("瞬态得分", "Transient"), juce::String (juce::roundToInt (d.transientScore)) } }};

    CardSpec c3;
    c3.title = T ("瞬态完整性", "TRANSIENT INTEGRITY");
    c3.primary = juce::String (juce::roundToInt (d.transientScore));
    c3.unit = "/100";
    c3.note = T ("攻击与峰响比保留程度", "attack and crest preservation");
    c3.risk = 100.0f - d.transientScore;
    c3.rows = {{ { "PLR", fmt1 (s.getPlr()) + " dB" },
                 { "PSR", fmt1 (s.psr) + " dB" },
                 { T ("峰响损失", "Crest loss"), dbText (d.crestLossDb) } }};

    CardSpec c4;
    c4.title = T ("单声道折叠损失", "MONO FOLD LOSS");
    c4.primary = num (d.monoFoldLossDb);
    c4.unit = "dB";
    c4.note = T ("M=(L+R)/2 后损失的电平", "level lost after M=(L+R)/2");
    c4.risk = d.monoRisk;
    c4.rows = {{ { T ("保留", "Retained"), fmt1 (d.monoRetainPct) + "%" },
                 { T ("低频保留", "Low retained"), fmt1 (d.monoLowRetainPct) + "%" },
                 { T ("相关性", "Correlation"), sgn (s.correlation, 2) } }};

    CardSpec c5;
    c5.title = T ("中频掩蔽风险", "MID MASKING");
    c5.primary = num (d.maskingDb);
    c5.unit = "dB";
    c5.note = T ("200–800 Hz 相对 1–4 kHz", "200-800 Hz over 1-4 kHz");
    c5.risk = d.maskingRisk;
    c5.rows = {{ { T ("低中密度", "Low-mid"), dbText (d.maskingDb) },
                 { "LRA", fmt1 (s.lra) + " LU" },
                 { T ("低频侧向", "Low side"), fmt1 (s.lowSidePct) + "%" } }};

    c0.active = state.signalAuditionIndex == 0;
    c1.active = state.signalAuditionIndex == 1;
    c2.active = state.signalAuditionIndex == 2;
    c3.active = state.signalAuditionIndex == 3;
    c4.active = state.signalAuditionIndex == 4;
    c5.active = state.signalAuditionIndex == 5;

    paintCard (g, cell (0, 0), c0);
    paintCard (g, cell (1, 0), c1);
    paintCard (g, cell (2, 0), c2);
    paintCard (g, cell (0, 1), c3);
    paintCard (g, cell (1, 1), c4);
    paintCard (g, cell (2, 1), c5);

    // ---- timestamped problem regions ---------------------------------------
    {
        auto row = signalProblemRowBounds;

        if (! row.isEmpty())
        {
            auto label = row.removeFromLeft (juce::jmin (78, row.getWidth() / 5));

            g.setColour (theme::textDim);
            g.setFont (getLabelFont());
            g.drawText (T ("问题区域", "REGIONS"), label, juce::Justification::centredLeft, false);

            if (state.signalProblems.empty())
            {
                g.setColour (theme::textFaint);
                g.setFont (getLabelFont());
                g.drawText (T ("检出越线时自动生成时间戳；点击卡片实时跟随 DAW，点击时间戳循环该段",
                               "Timestamped regions appear when a layer crosses its limit; click a card for live DAW solo, or a timestamp to loop that region"),
                            row, juce::Justification::centredLeft, true);
            }
            else
            {
                for (int i = 0; i < 4 && i < (int) state.signalProblems.size(); ++i)
                {
                    const auto& p = state.signalProblems[(size_t) i];
                    auto chip = signalChipBounds[(size_t) i];
                    const auto colour = p.severity >= 2 ? theme::bad : theme::warn;

                    g.setColour (theme::cardAlt);
                    g.fillRoundedRectangle (chip.toFloat(), tokens::radiusControl);
                    g.setColour (colour.withAlpha (0.55f));
                    g.drawRoundedRectangle (chip.toFloat().reduced (0.5f), tokens::radiusControl, 1.0f);

                    auto inner = chip.reduced (6, 0);

                    g.setColour (colour);
                    g.setFont (getValueFont());
                    g.drawText (fmtTime (p.start), inner.removeFromLeft (44),
                                juce::Justification::centredLeft, false);

                    g.setColour (theme::textDim);
                    g.setFont (getLabelFont());
                    g.drawText (p.label, inner, juce::Justification::centredLeft, false);
                }
            }
        }
    }

    // ---- attribution layer --------------------------------------------------
    if (attribution.getHeight() >= 24)
    {
        g.setColour (theme::cardAlt);
        g.fillRoundedRectangle (attribution.toFloat(), tokens::radiusCard);
        g.setColour (theme::border);
        g.drawRoundedRectangle (attribution.toFloat().reduced (0.5f), tokens::radiusCard, 1.0f);

        auto inner = attribution.reduced (14, 9);

        g.setColour (theme::accent);
        g.setFont (getLabelFont());
        g.drawText (T ("联动归因层  CORRELATED EVENTS", "CORRELATED EVENTS"),
                    inner.removeFromTop (16), juce::Justification::centredLeft, false);

        juce::StringArray lines;

        if (d.distortionRisk >= 55.0f && s.truePeakDb > state.ceiling - 0.5f)
            lines.add (T ("限幅级联动：真峰值贴近上限 + 平顶样本 + 谐波堆积 → 末级限幅/削波正在塑形信号，"
                          "先降 0.5–1 dB 输出再看失真是否下降。",
                          "Limiter chain: TP at the ceiling, flat tops and harmonic build-up - the final "
                          "limiter/clipper is shaping the signal. Drop 0.5-1 dB and re-check."));
        else if (d.distortionRisk >= 55.0f)
            lines.add (T ("失真同源：谐波与互调边带同时升高，检查饱和级、过载插件与过采样设置。",
                          "Distortion: harmonics and IMD sidebands rise together - check saturation, "
                          "driven plug-ins and oversampling."));

        if (d.subRisk >= 50.0f && d.monoRisk >= 35.0f)
            lines.add (T ("低频联动：40 Hz 以下堆积 + 单声道折叠损失 → 20–40 Hz 的侧向内容过多，"
                          "低频需要收紧并做单声道化。",
                          "Low-end link: sub build-up plus mono fold loss - too much side content below "
                          "40 Hz. Tighten and mono the low end."));

        if (d.maskingRisk >= 55.0f && d.transientScore < 55.0f)
            lines.add (T ("掩蔽联动：低中能量过高 + 瞬态被压 → 人声与鼓点互相掩蔽；先减 200–500 Hz，再复查 PLR。",
                          "Masking link: excessive low-mids plus squashed transients - vocal and drums mask "
                          "each other. Cut 200-500 Hz first, then re-check PLR."));

        if (d.aliasRisk >= 55.0f && d.distortionRisk >= 45.0f)
            lines.add (T ("混叠联动：16–20 kHz 异常抬升与失真同时出现，通常来自未过采样的削波/限幅。",
                          "Aliasing link: a lifted 16-20 kHz band together with distortion usually comes "
                          "from clipping without oversampling."));

        if (lines.isEmpty())
            lines.add (T ("未发现同时出现的多层异常；各项读数彼此独立。",
                          "No correlated layers found - the readings are independent."));

        lines.add (T ("单项越线按各自建议处理；联动层只在同一时间 / 同一根因下才合并归因。",
                      "Handle single-layer items individually; the attribution layer only merges "
                      "anomalies that share a time position or a root cause."));

        g.setColour (theme::textDim);
        g.setFont (getLabelFont());

        for (int i = 0; i < juce::jmin (3, lines.size()); ++i)
            if (inner.getHeight() >= 18)
                g.drawText (U8 ("· ") + lines[i], inner.removeFromTop (20),
                            juce::Justification::centredLeft, false);
    }
}
} // namespace ml



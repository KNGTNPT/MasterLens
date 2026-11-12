#include "Modules.h"
#include "../Delivery.h"

namespace ml
{
void DetailArea::ensureBackground()
{
    if (! backgroundDirty && ! backgroundCache.isNull()
        && cachedBackgroundRevision == theme::revision()
        && backgroundCache.getWidth() == juce::roundToInt (getWidth() * juce::jmax (1.0f, getDesktopScaleFactor()))
        && backgroundCache.getHeight() == juce::roundToInt (getHeight() * juce::jmax (1.0f, getDesktopScaleFactor())))
        return;

    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const int scale = juce::roundToInt (juce::jmax (1.0f, getDesktopScaleFactor()));
    backgroundCache = juce::Image (juce::Image::ARGB, getWidth() * scale, getHeight() * scale, true);

    juce::Graphics bg (backgroundCache);
    bg.addTransform (juce::AffineTransform::scale ((float) scale));
    // 3.1: pages sit on the plate; the measurement well is the only raised
    // surface. No rounded page card.
    bg.fillAll (theme::background);

    backgroundDirty = false;
    cachedBackgroundRevision = theme::revision();
}

static juce::String pageTitle (PageId p)
{
    switch (p)
    {
        case PageId::loudness:    return T ("响度与动态", "Loudness & dynamics");
        case PageId::truePeak:    return T ("真峰值", "True peak");
        case PageId::spectrum:    return T ("频谱平衡", "Spectral balance");
        case PageId::stereo:      return T ("立体声 / 相位", "Stereo / phase");
        case PageId::delivery:    return T ("平台归一化", "Platform normalisation");
        case PageId::album:       return T ("专辑统一 / 交付门禁", "Album consistency / release gate");
        case PageId::signal:      return T ("信号健康", "Signal health");
        case PageId::diagnostics: return T ("混音诊断", "Mix diagnostics");
        case PageId::timeline:    return T ("时间线", "Timeline");
        case PageId::target:      return T ("曲风响度目标 / 平台交付要求", "Genre loudness target / delivery requirements");
        case PageId::files:       return T ("文件 QC / 离线批量分析", "File QC / offline batch analysis");
    }

    return {};
}

void DetailArea::paintPageHeader (juce::Graphics& g, const juce::String& meta)
{
    auto header = contentArea().removeFromTop (32);

    g.setColour (theme::text);
    g.setFont (getPageTitleFont());
    g.drawText (pageTitle (page), header.removeFromLeft (juce::jmin (320, header.getWidth() * 2 / 3)),
                juce::Justification::centredLeft);

    if (state.liveAuditionLabel.isNotEmpty())
    {
        const auto badgeW = juce::jmin (250, juce::jmax (150, header.getWidth() / 2));
        auto badge = header.removeFromRight (badgeW);

        g.setColour (theme::card);
        g.fillRoundedRectangle (badge.toFloat().reduced (0.0f, 4.0f), 9.0f);
        g.setColour (theme::accent.withAlpha (0.55f));
        g.drawRoundedRectangle (badge.toFloat().reduced (0.0f, 4.0f), 9.0f, 1.0f);
        g.setColour (theme::accent);
        g.setFont (getLabelFont());
        g.drawText (T ("实时试听 · ", "LIVE · ") + state.liveAuditionLabel,
                    badge.reduced (8, 0), juce::Justification::centredRight, true);
    }

    g.setColour (theme::textFaint);
    g.setFont (getMetaFont());
    g.drawText (meta, header, juce::Justification::centredRight);

    if (page == PageId::spectrum)
    {
        // The mode buttons keep a small group label; the page title now lives
        // in the shared header row above the graph.
        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (grid::controlText (T ("视图", "View")),
                    getLocalBounds().withTrimmedTop (spectrumSplit + 8)
                        .withHeight (24).reduced (16, 0).withWidth (64),
                    juce::Justification::centredLeft, false);
    }
}

void DetailArea::paint (juce::Graphics& g)
{
    ensureBackground();
    g.drawImage (backgroundCache, getLocalBounds().toFloat());

    juce::String meta;

    switch (page)
    {
        case PageId::loudness:
            meta = juce::String ("M 400 ms") + ml::sep() + "S 3 s" + ml::sep() + "I " + T ("门限", "gated")
                 + ml::sep() + T ("基准 ", "basis ") + state.basis.label();

            if (state.snapshotSlot >= 0)
                meta += ml::sep() + (state.snapshotSlot == 0 ? "A" : "B") + T (" 快照", " snapshot");
            break;

        case PageId::truePeak:
            meta = juce::String ("4") + ml::times() + " " + T ("过采样", "oversampling") + ml::sep()
                 + "OVERS " + juce::String (state.stats.oversCount);
            break;

        case PageId::spectrum:
            meta = (state.basis.mode == ml::BasisMode::album && state.basis.hasAlbum
                        ? T ("专辑中位: ", "Album median: ") + state.basis.albumLabel
                        : (state.hasReference ? T ("参考: ", "Ref: ") + state.referenceStatus
                                              : T ("无参考 / 专辑基准", "no reference / album basis")))
                 + ml::sep() + T ("谐振 ", "res ") + juce::String ((int) state.resonances.size())
                 + ml::sep() + (state.analyzer != nullptr
                                   ? T ("频谱 / 频谱图 / 频段热力", "spectrum / waterfall / band heat")
                                   : T ("8192 FFT · 60 fps", "8192 FFT · 60 fps"));
            break;

        case PageId::stereo:
            meta = (state.stats.stereo ? T ("三段分频", "three bands")
                                       : T ("单声道", "mono") + ml::sep() + T ("三段分频", "three bands"))
                 + ml::sep() + "L/R";
            break;

        case PageId::delivery:
            meta = juce::String (numPlatforms) + T (" 个平台", " platforms") + ml::sep()
                 + T ("归一化后果", "normalisation outcome");
            break;

        case PageId::signal:
            meta = T ("失真 / 混叠 / 超低 / 瞬态 / 单声道 / 掩蔽", "THD-IMD / alias / sub / transient / mono / masking")
                 + ml::sep() + T ("最高风险 ", "max risk ")
                 + juce::String (juce::roundToInt (state.defects.worstRisk()));
            break;

        case PageId::album:
            meta = state.album.albumName + ml::sep() + "rev " + juce::String (state.album.revision)
                 + ml::sep() + juce::String ((int) state.album.tracks.size()) + T (" 首", " tracks")
                 + ml::sep() + (! state.albumGate.passed() ? T ("未通过", "NOT PASSED") : T ("放行", "PASS"));
            break;

        case PageId::diagnostics:
            meta = juce::String ((int) state.markers.size()) + T (" 标记", " markers");
            break;

        case PageId::timeline:
            meta = T ("双击时间条添加标记", "double-click to add a marker");
            break;

        case PageId::target:
            meta = T ("曲风标准 50% · 滑块调整参与度", "genre reference at 50% · slider sets participation")
                 + ml::sep() + T ("平台交付要求", "platform delivery requirements");
            break;

        case PageId::files:
            meta = T ("WAV / AIFF / FLAC · 后台线程", "WAV / AIFF / FLAC · background thread")
                 + ml::sep() + T ("完成后自动写入专辑", "auto-added to album");
            break;
    }

    paintPageHeader (g, meta);

    if (emptyMode && page != PageId::files)
    {
        paintEmptyState (g, contentArea().withTrimmedTop (32));
        return;
    }

    switch (page)
    {
        case PageId::loudness:    paintLoudness (g);       break;
        case PageId::truePeak:    paintTruePeak (g);       break;
        case PageId::spectrum:    paintSpectrumExtras (g); break;
        case PageId::stereo:      paintStereoExtras (g);   break;
        case PageId::delivery:    paintDelivery (g);       break;
        case PageId::album:       paintAlbum (g);          break;
        case PageId::signal:      paintSignal (g);         break;
        case PageId::diagnostics: paintDiagnostics (g);    break;
        case PageId::timeline:    paintTimeline (g);       break;
        case PageId::target:      paintTarget (g);         break;
        case PageId::files:       paintFiles (g);          break;
    }
}

//==============================================================================
void DetailArea::paintDeliveryLedger (juce::Graphics& g)
{
    if (deliveryArea.isEmpty() || deliveryArea.getWidth() < 160)
        return;

    auto area = deliveryArea;
    auto head = area.removeFromTop (18);
    g.setColour (theme::textFaint);
    g.setFont (getMicroFont());
    g.drawText (T ("平台交付", "DELIVERY"), head, juce::Justification::centredLeft, false);

    const bool live = state.stats.integrated > -70.0f;
    int limited = 0;

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto r = ml::evaluate (state.stats, p);
        const bool warn = r.state != DeliveryState::ok;

        if (warn && live) ++limited;

        auto row = area.removeFromTop (27);
        g.setColour (theme::text.withAlpha (0.08f));
        g.fillRect ((float) row.getX(), (float) row.getBottom() - 1.0f, (float) row.getWidth(), 1.0f);

        const auto colour = ! live ? theme::textFaint
                          : r.state == DeliveryState::over ? theme::bad
                          : r.state == DeliveryState::warning ? theme::warn
                                                              : theme::accent;
        g.setColour (colour);
        g.fillEllipse ((float) row.getX(), (float) row.getCentreY() - 2.5f, 5.0f, 5.0f);

        g.setColour (theme::textDim);
        g.setFont (getMicroFont());
        g.drawText (T (p.nameZh, p.nameEn), row.withTrimmedLeft (12).withTrimmedRight (108),
                    juce::Justification::centredLeft, false);

        g.setColour (theme::textFaint);
        g.setFont (getMonoFont (10.0f));
        g.drawText (num (p.targetLufs), row.withTrimmedRight (58).removeFromRight (50),
                    juce::Justification::centredRight, false);

        const float delta = state.stats.integrated - p.targetLufs;
        const bool near = ! live || std::abs (delta) < 0.05f;
        g.setColour (near ? theme::textFaint : colour);
        g.drawText (near ? juce::String::fromUTF8 ("\xe2\x80\x94") : sgn (delta) + " LU",
                    row.removeFromRight (52), juce::Justification::centredRight, false);
    }

    auto foot = area.removeFromTop (20);
    g.setColour (theme::textFaint);
    g.setFont (getMicroFont());
    const auto footText = live ? juce::String (limited) + T (" 个平台受限", " platforms limited")
                               : T ("等待信号", "WAITING FOR SIGNAL");
    g.drawText (footText, foot, juce::Justification::centredLeft, false);
}

void DetailArea::paintLoudness (juce::Graphics& g)
{
    paintDeliveryLedger (g);

    if (loudnessSideArea.isEmpty())
        return;

    const bool compact = loudnessSideArea.getHeight() < 150;
    auto area = loudnessSideArea;

    const auto drawMetric = [&] (juce::Rectangle<int> cell, const juce::String& label,
                                 const juce::String& value, float normalised,
                                 juce::Colour colour)
    {
        auto inner = cell;

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText (label, inner.removeFromTop (16), juce::Justification::centredLeft, false);

        g.setColour (theme::text);
        g.setFont (getValueFont());
        g.drawText (value, inner.removeFromTop (20), juce::Justification::centredLeft, false);

        inner.removeFromTop (4);
        auto track = inner.removeFromTop (4).toFloat();
        g.setColour (theme::borderSoft);
        g.fillRect (track);
        g.setColour (colour);
        g.fillRect (track.withWidth (juce::jmax (2.0f, track.getWidth()
                                                  * juce::jlimit (0.0f, 1.0f, normalised))));
    };

    const auto corr = state.stats.stereo ? state.stats.correlation : 1.0f;
    const auto corrNorm = juce::jlimit (0.0f, 1.0f, (corr + 1.0f) * 0.5f);
    const auto widthNorm = juce::jlimit (0.0f, 1.0f, state.stats.widthPct / 100.0f);
    const auto lowNorm = juce::jlimit (0.0f, 1.0f, state.stats.lowSidePct / 100.0f);

    if (! compact)
    {
        auto top = area.removeFromTop (24);
        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawText ("02", top.removeFromLeft (24), juce::Justification::centredLeft, false);
        g.drawText (T ("相位与宽度", "PHASE & WIDTH"), top, juce::Justification::centredLeft, false);
        surface::hairlineH (g, (float) area.getX(), (float) area.getRight(), (float) area.getY() - 8.0f);

        const int rowH = juce::jlimit (44, 68, area.getHeight() / 5);

        drawMetric (area.removeFromTop (rowH), T ("相关性", "CORRELATION"),
                    sgn (corr, 2), corrNorm,
                    corr < 0.0f ? theme::bad : (corr < 0.15f ? theme::warn : theme::good));
        drawMetric (area.removeFromTop (rowH), T ("立体声宽度", "STEREO WIDTH"),
                    num (state.stats.widthPct) + "%", widthNorm, theme::accent);
        drawMetric (area.removeFromTop (rowH), T ("低频侧向", "LOW SIDE"),
                    num (state.stats.lowSidePct) + "%", lowNorm,
                    state.stats.lowSidePct > 35.0f ? theme::warn : theme::good);
    }
    else
    {
        const int w = area.getWidth() / 3;
        drawMetric (area.removeFromLeft (w), T ("相关性", "CORR"),
                    sgn (corr, 2), corrNorm,
                    corr < 0.0f ? theme::bad : (corr < 0.15f ? theme::warn : theme::good));
        drawMetric (area.removeFromLeft (w), T ("宽度", "WIDTH"),
                    num (state.stats.widthPct) + "%", widthNorm, theme::accent);
        drawMetric (area, T ("低频侧向", "LOW SIDE"),
                    num (state.stats.lowSidePct) + "%", lowNorm,
                    state.stats.lowSidePct > 35.0f ? theme::warn : theme::good);
    }
}
} // namespace ml

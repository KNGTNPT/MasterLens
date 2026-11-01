#include "Modules.h"
#include "../Delivery.h"
#include "../GenreWar.h"

#include <utility>

namespace ml
{
//==============================================================================
DetailArea::DetailArea()
{
    addChildComponent (tiles);
    addChildComponent (levelBars);
    addChildComponent (history);

    history.onSeek = [this] (double age)
    {
        if (onHistorySeek != nullptr)
            onHistorySeek (age);
    };
    addChildComponent (spectrum);
    addChildComponent (spectrogram);
    addChildComponent (bands);

    for (int i = 0; i < (int) spectrumViewButtons.size(); ++i)
    {
        auto& b = spectrumViewButtons[(size_t) i];
        b.setClickingTogglesState (true);
        b.setRadioGroupId (8811);
        b.onClick = [this, i]
        {
            setSpectrumView (i);
        };
        addChildComponent (b);
    }
    addChildComponent (meters);
    addChildComponent (issues);
    addChildComponent (refImport);
    addChildComponent (refClear);
    addChildComponent (refPlay);
    addChildComponent (refAlign);
    addChildComponent (reportButton);
    addChildComponent (markButton);
    addChildComponent (markerExport);
    addChildComponent (markerImport);
    addChildComponent (albumCapture);
    addChildComponent (albumProof);
    addChildComponent (albumImport);
    addChildComponent (albumClearButton);
    addChildComponent (albumExportMarkers);
    addChildComponent (albumImportMarkers);
    addChildComponent (albumTrackName);
    addChildComponent (albumNameEditor);
    addChildComponent (albumTrackLength);

    albumTrackLength.setInputRestrictions (7, "0123456789.");
    albumTrackLength.onTextChange = [this]
    {
        if (onTrackLengthChanged != nullptr)
            onTrackLengthChanged (albumTrackLength.getText().getDoubleValue());
    };

    addChildComponent (genreBox);
    addChildComponent (warSlider);
    addChildComponent (applyTarget);

    genreBox.onChange = [this]
    {
        if (onTargetGenreChanged != nullptr)
            onTargetGenreChanged (genreBox.getSelectedId() - 1);
    };

    warSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    warSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 24);
    warSlider.setRange (0.0, 100.0, 1.0);
    warSlider.setDoubleClickReturnValue (true, 50.0);
    warSlider.setTextValueSuffix (" %");
    warSlider.onValueChange = [this]
    {
        if (onTargetWarChanged != nullptr)
            onTargetWarChanged ((float) warSlider.getValue());
    };

    applyTarget.onClick = [this]
    {
        if (onApplyTargetPlan != nullptr)
            onApplyTargetPlan();
    };

    for (auto* b : { &fileChoose, &fileFolder, &fileAddAlbum, &fileClear })
        addChildComponent (*b);

    fileChoose.onClick = [this] { if (onChooseFiles != nullptr) onChooseFiles(); };
    fileFolder.onClick = [this] { if (onChooseFolder != nullptr) onChooseFolder(); };
    fileAddAlbum.onClick = [this] { if (onAddFileResultsToAlbum != nullptr) onAddFileResultsToAlbum(); };
    fileClear.onClick = [this] { if (onClearFileResults != nullptr) onClearFileResults(); };
    fileExport.onClick = [this] { if (onExportFileTable != nullptr) onExportFileTable(); };
    addChildComponent (fileExport);

    for (auto& b : albumRuleButtons)
        addChildComponent (b);

    refImport.onClick = [this] { if (onImportReference != nullptr) onImportReference(); };
    refClear.onClick = [this] { if (onClearReference != nullptr) onClearReference(); };
    refPlay.onClick = [this] { if (onToggleReferencePlayback != nullptr) onToggleReferencePlayback(); };
    refAlign.onClick = [this] { if (onAutoAlignReference != nullptr) onAutoAlignReference(); };
    reportButton.onClick = [this] { if (onExportReport != nullptr) onExportReport(); };
    markButton.onClick = [this] { if (onAddMarkerNow != nullptr) onAddMarkerNow(); };
    markerExport.onClick = [this] { if (onExportMarkers != nullptr) onExportMarkers(); };
    markerImport.onClick = [this] { if (onImportMarkers != nullptr) onImportMarkers(); };

    albumCapture.onClick = [this] { if (onAlbumCapture != nullptr) onAlbumCapture(); };
    albumProof.onClick = [this] { if (onAlbumExportProof != nullptr) onAlbumExportProof(); };
    albumImport.onClick = [this] { if (onAlbumImport != nullptr) onAlbumImport(); };
    albumClearButton.onClick = [this] { if (onAlbumClear != nullptr) onAlbumClear(); };

    for (int i = 0; i < (int) albumRuleButtons.size(); ++i)
    {
        auto& b = albumRuleButtons[(size_t) i];
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, theme::borderSoft.withAlpha (0.55f));
        b.setColour (juce::TextButton::buttonOnColourId, theme::bad.withAlpha (0.55f));
        b.setColour (juce::TextButton::textColourOffId, theme::textDim);
        b.setColour (juce::TextButton::textColourOnId, theme::text);
        b.onClick = [this, i]
        {
            if (onAlbumRuleChanged != nullptr)
                onAlbumRuleChanged (i, albumRuleButtons[(size_t) i].getToggleState());
        };
    }

    for (auto* e : { &albumTrackName, &albumNameEditor })
    {
        e->setFont (uiFont (tokens::label));
        e->setColour (juce::TextEditor::backgroundColourId, theme::cardAlt);
        e->setColour (juce::TextEditor::outlineColourId, theme::border);
        e->setColour (juce::TextEditor::textColourId, theme::text);
        e->setColour (juce::TextEditor::highlightColourId, theme::cyan.withAlpha (0.25f));
    }

    albumNameEditor.setTextToShowWhenEmpty (T ("专辑名", "Album name"), theme::textFaint);
    albumTrackName.setTextToShowWhenEmpty (T ("曲目名", "Track name"), theme::textFaint);
    albumTrackLength.setTextToShowWhenEmpty (T ("总长 s", "Length s"), theme::textFaint);
    albumTrackLength.setTooltip (T ("宿主没有统一的曲目长度 API；在这里填整首时长，用于覆盖度门禁",
                                    "Hosts do not expose a universal track length; enter it here for coverage"));

    refreshTexts();
}

void DetailArea::refreshTexts()
{
    refImport.setButtonText (T ("导入参考轨", "Import reference"));
    refClear.setButtonText (T ("清除", "Clear"));
    refPlay.setButtonText (refPlaying ? T ("参考 A/B 开", "Ref A/B on")
                                      : T ("参考 A/B", "Ref A/B"));
    refPlay.setTooltip (T ("在插件内按宿主播放头播放参考轨；与等响监听一起构成 A/B",
                            "Play the reference inside the plug-in at the host playhead; pair with loudness matching"));
    refAlign.setButtonText (T ("粗略对齐", "Auto align"));
    refAlign.setTooltip (T ("用最近 30 s 短时响度包络做粗略自动对齐；不是采样级同步",
                             "Coarse envelope alignment over the last 30 s; not sample-accurate"));

    spectrumViewButtons[0].setButtonText (T ("频谱", "Spectrum"));
    spectrumViewButtons[1].setButtonText (T ("频谱图", "Waterfall"));
    spectrumViewButtons[2].setButtonText (T ("频段热力", "Band heat"));
    spectrumViewButtons[(size_t) spectrumView].setToggleState (true, juce::dontSendNotification);

    reportButton.setButtonText (T ("导出报告", "Export report"));
    markButton.setButtonText (T ("+ 打标记", "+ Marker"));
    markerExport.setButtonText (T ("导出标记 CSV", "Export markers"));
    markerImport.setButtonText (T ("导入标记 CSV", "Import markers"));
    albumCapture.setButtonText (T ("采集本曲", "Capture track"));
    albumProof.setButtonText (T ("导出证明", "Proof"));
    albumImport.setButtonText (T ("导入基准", "Import"));
    albumClearButton.setButtonText (T ("清空", "Clear"));
    albumExportMarkers.setButtonText (T ("导出标记", "Export markers"));
    albumImportMarkers.setButtonText (T ("导入标记", "Import markers"));

    genreBox.clear (juce::dontSendNotification);

    for (int i = 0; i < ml::presets::customIndex; ++i)
        genreBox.addItem (T (ml::presets::entries[i].zh, ml::presets::entries[i].en), i + 1);

    genreBox.setSelectedId (juce::jlimit (1, ml::presets::customIndex,
                                          state.targetGenreIndex + 1),
                            juce::dontSendNotification);
    genreBox.setTooltip (T ("曲风参照：标准值来自 1.5 曲风表；可用下方滑块调整响度战争参与度。",
                            "Genre reference from the static table; use the slider to set loudness-war participation."));

    applyTarget.setButtonText (T ("应用为目标", "Apply as target"));

    fileChoose.setButtonText (T ("选择文件", "Choose files"));
    fileFolder.setButtonText (T ("选择文件夹", "Choose folder"));
    fileAddAlbum.setButtonText (T ("加入专辑", "Add to album"));
    fileExport.setButtonText (T ("导出表", "Export CSV"));
    fileClear.setButtonText (T ("清空结果", "Clear results"));
    fileChoose.setTooltip (T ("离线分析 WAV / AIFF / FLAC，秒级出完整报告",
                               "Offline analysis of WAV / AIFF / FLAC in seconds"));
    fileFolder.setTooltip (T ("递归分析整个专辑文件夹，并自动填入门禁表",
                              "Recursively analyse an album folder and fill the gate table"));
    fileAddAlbum.setTooltip (T ("把当前文件结果重新写入专辑快照",
                                "Write the current file results to the album snapshot"));

    const juce::String ruleLabels[5] {
        T ("平台限幅", "Platform"),
        T ("真峰值", "True peak"),
        T ("信号缺陷", "Signal"),
        T ("单声道", "Mono"),
        T ("中位偏离", "Deviation")
    };

    for (int i = 0; i < (int) albumRuleButtons.size(); ++i)
        albumRuleButtons[(size_t) i].setButtonText (ruleLabels[i]);
}

void DetailArea::invalidateDisplays()
{
    backgroundDirty = true;
    spectrum.invalidateBackground();
    bands.invalidateBackground();
    history.invalidateBackground();
    repaint();
}

void DetailArea::hideAllChildren()
{
    tiles.setVisible (false);
    levelBars.setVisible (false);
    history.setVisible (false);
    spectrum.setVisible (false);
    spectrogram.setVisible (false);
    bands.setVisible (false);

    for (auto& b : spectrumViewButtons)
        b.setVisible (false);
    meters.setVisible (false);
    issues.setVisible (false);
    refImport.setVisible (false);
    refClear.setVisible (false);
    refPlay.setVisible (false);
    refAlign.setVisible (false);
    reportButton.setVisible (false);
    markButton.setVisible (false);
    markerExport.setVisible (false);
    markerImport.setVisible (false);
    albumCapture.setVisible (false);
    albumProof.setVisible (false);
    albumImport.setVisible (false);
    albumClearButton.setVisible (false);
    albumExportMarkers.setVisible (false);
    albumImportMarkers.setVisible (false);
    albumTrackName.setVisible (false);
    albumNameEditor.setVisible (false);
    albumTrackLength.setVisible (false);
    genreBox.setVisible (false);
    warSlider.setVisible (false);
    applyTarget.setVisible (false);

    fileChoose.setVisible (false);
    fileFolder.setVisible (false);
    fileAddAlbum.setVisible (false);
    fileClear.setVisible (false);
    fileExport.setVisible (false);

    for (auto& b : albumRuleButtons)
        b.setVisible (false);
}

juce::Rectangle<int> DetailArea::contentArea() const
{
    return getLocalBounds().reduced (16, 12);
}

void DetailArea::setPage (PageId p)
{
    if (page == p)
        return;

    page = p;
    resized();
    repaint();
}

void DetailArea::setHistory (const std::vector<float>& curve, float targetLufs, float lra)
{
    if (page == PageId::loudness)
    {
        history.setData (curve, targetLufs);
        history.setBand (lra);
    }
}

void DetailArea::expandFirstIssueForSnapshot()
{
    page = PageId::diagnostics;
    resized();
    issues.expandFirstForSnapshot();
    repaint();
}

void DetailArea::setIssues (std::vector<Issue> items)
{
    issues.setIssues (std::move (items));
    repaint();
}

void DetailArea::setReferenceInfo (bool hasRef, const juce::String& status)
{
    if (refReady == hasRef && refStatus == status)
        return;

    refReady = hasRef;
    refStatus = status;
    repaint();
}

void DetailArea::setSpectrumView (int view)
{
    spectrumView = juce::jlimit (0, 2, view);

    for (int i = 0; i < (int) spectrumViewButtons.size(); ++i)
        spectrumViewButtons[(size_t) i].setToggleState (i == spectrumView,
                                                         juce::dontSendNotification);

    if (spectrumView == 0)
        spectrum.setVisible (true);
    else
        spectrum.setVisible (false);

    spectrogram.setMode (spectrumView == 2 ? SpectrogramDisplay::Mode::bands
                                           : SpectrogramDisplay::Mode::spectrogram);
    spectrogram.setVisible (spectrumView != 0);
    bands.setVisible (true);
    resized();
    repaint();
}

void DetailArea::setReferencePlaybackState (bool playing, const juce::String&)
{
    refPlaying = playing;
    refPlay.setButtonText (refPlaying ? T ("参考 A/B 开", "Ref A/B on")
                                      : T ("参考 A/B", "Ref A/B"));
    refPlay.setColour (juce::TextButton::buttonOnColourId,
                       refPlaying ? theme::accentSoft : theme::cardAlt);
    repaint();
}

void DetailArea::setFileResults (std::vector<FileAnalysisResult> results, bool busy,
                                 int done, int total, const juce::String& status)
{
    fileResults = std::move (results);
    fileBusy = busy;
    fileDone = done;
    fileTotal = total;
    fileStatus = status;

    if (fileSelected >= (int) fileResults.size())
        fileSelected = -1;

    repaint();
}

void DetailArea::mouseDown (const juce::MouseEvent& e)
{
    if (page == PageId::files)
    {
        handleFileMouseDown (e);
        return;
    }

    if (page == PageId::album)
    {
        auto area = contentArea().withTrimmedTop (30);
        area.removeFromBottom (70);
        area.removeFromTop (44);

        const int tableW = juce::roundToInt ((float) area.getWidth() * 0.62f);
        area.removeFromLeft (tableW);
        area.removeFromLeft (tokens::gap);
        auto side = area;

        side.removeFromTop (68 + 8 + 18);   // gate card + gap + ranking title

        for (int i = 0; i < (int) state.albumGate.risks.size() && i < 4; ++i)
        {
            auto row = side.removeFromTop (26);

            if (row.contains (e.getPosition()) && onAuditionRiskDetail != nullptr)
            {
                onAuditionRiskDetail (state.albumGate.risks[(size_t) i].dimension
                                      + " " + state.albumGate.risks[(size_t) i].detail);
                return;
            }
        }

        return;
    }

    if (page != PageId::signal)
        return;

    for (int i = 0; i < 4 && i < (int) state.signalProblems.size(); ++i)
    {
        if (signalChipBounds[(size_t) i].contains (e.getPosition())
            && onAuditionSignalRegion != nullptr)
        {
            const auto& p = state.signalProblems[(size_t) i];
            onAuditionSignalRegion (p.card, p.start, p.end);
            return;
        }
    }

    const auto grid = signalGridBounds;
    const int gap = tokens::gap;
    const int w = (grid.getWidth() - gap * 2) / 3;
    const int h = (grid.getHeight() - gap) / 2;

    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 3; ++col)
        {
            const juce::Rectangle<int> cell (grid.getX() + col * (w + gap),
                                             grid.getY() + row * (h + gap), w, h);

            if (cell.contains (e.getPosition()) && onAuditionSignal != nullptr)
            {
                onAuditionSignal (row * 3 + col);
                return;
            }
        }
}

void DetailArea::setState (UiState s)
{
    state = std::move (s);
    ++tick;

    const bool empty = ! state.stats.hasSignal && page != PageId::album
                       && page != PageId::target && page != PageId::files;

    if (empty != emptyMode)
    {
        emptyMode = empty;
        resized();
        repaint();
    }

    if (emptyMode)
        return;

    switch (page)
    {
        case PageId::loudness:
            if ((tick % 3) == 0)
            {
                tiles.setValues (state.stats, state.basis, state.platform,
                                 state.target, state.ceiling, state.stats.hasSignal);
                levelBars.setValues (state.stats, state.target);

                const double now = state.stats.hostPosition > 0.0
                                       ? state.stats.hostPosition : state.stats.elapsed;
                std::vector<HistoryGraph::Event> evs;

                for (const auto& m : state.markers)
                    if (now >= m.time)
                        evs.push_back ({ (float) (now - m.time), m.severity });

                history.setEvents (std::move (evs));
            }
            break;

        case PageId::spectrum:
            if (state.analyzer != nullptr)
            {
                spectrum.setAnalyzer (state.analyzer);

                if (spectrumView != 0 && (tick % 3) == 0)
                    spectrogram.capture (*state.analyzer);
            }

            spectrum.repaint();

            if ((tick % 2) == 0)
                bands.setDeviations (state.bandDevs);
            break;

        case PageId::stereo:
            if ((tick % 3) == 0)
                meters.setValues (state.stats);
            break;

        case PageId::delivery:
        case PageId::signal:
        case PageId::diagnostics:
        case PageId::timeline:
        case PageId::album:
            if ((tick % 12) == 0)
            {
                const auto& r = state.album.rules;
                const bool states[5] { r.blockPlatform, r.blockTp, r.blockSignal,
                                       r.blockMono, r.blockInconsistency };

                for (int i = 0; i < 5; ++i)
                {
                    albumRuleButtons[(size_t) i].setToggleState (states[i], juce::dontSendNotification);
                    albumRuleButtons[(size_t) i].setColour (juce::TextButton::buttonOnColourId,
                                                            states[i] ? theme::bad.withAlpha (0.55f)
                                                                      : theme::cardAlt);
                }

                repaint();
            }
            break;

        case PageId::target:
            if ((tick % 6) == 0)
            {
                genreBox.setSelectedId (juce::jlimit (1, ml::presets::customIndex,
                                                      state.targetGenreIndex + 1),
                                        juce::dontSendNotification);
                warSlider.setValue (state.targetWarPct, juce::dontSendNotification);
                repaint();
            }
            break;
        case PageId::truePeak:
            if ((tick % 4) == 0)
                repaint();
            break;

        case PageId::files:
            if ((tick % 6) == 0)
                repaint();
            break;
    }
}

void DetailArea::resized()
{
    hideAllChildren();

    auto area = contentArea();
    area.removeFromTop (30);   // page header

    if (emptyMode && page != PageId::files && page != PageId::album && page != PageId::target)
        return;

    switch (page)
    {
        case PageId::loudness:
        {
            const bool compact = area.getHeight() < 300;
            const int gap = compact ? 6 : tokens::gap;
            const int cardsH = area.getWidth() >= 560
                                   ? (compact ? 92 : tokens::cardHeight)
                                   : tokens::cardHeight * 2 + tokens::gap;

            tiles.setBounds (area.removeFromTop (juce::jmin (cardsH, area.getHeight() / 2)));
            area.removeFromTop (gap);

            const int metersH = compact ? 68 : juce::jlimit (88, 96, area.getHeight() / 3);
            levelBars.setBounds (area.removeFromTop (metersH));
            area.removeFromTop (gap);

            history.setBounds (area);
            tiles.setVisible (true);
            levelBars.setVisible (true);
            history.setVisible (true);
            break;
        }

        case PageId::spectrum:
        {
            const int footerH = juce::jlimit (86, 118, area.getHeight() / 4);
            const int buttonsH = 34;

            auto buttons = area.removeFromBottom (buttonsH);
            area.removeFromBottom (6);
            auto footer = area.removeFromBottom (footerH);
            area.removeFromBottom (tokens::gap);

            auto chipRow = footer.removeFromBottom (28);

            spectrum.setBounds (area);
            spectrogram.setBounds (area);
            bands.setBounds (footer);
            juce::ignoreUnused (chipRow);

            auto row = buttons.withTrimmedRight (4);
            refClear.setBounds (row.removeFromRight (58));
            row.removeFromRight (5);
            refAlign.setBounds (row.removeFromRight (66));
            row.removeFromRight (5);
            refImport.setBounds (row.removeFromRight (102));
            row.removeFromRight (5);
            refPlay.setBounds (row.removeFromRight (84));

            auto modes = row.withTrimmedRight (8);
            const int modeW = juce::jlimit (54, 76, (modes.getWidth() - 10) / 3);

            for (auto& b : spectrumViewButtons)
            {
                b.setBounds (modes.removeFromLeft (modeW).withSizeKeepingCentre (modeW, 28));
                modes.removeFromLeft (5);
                b.setVisible (true);
            }

            spectrum.setVisible (spectrumView == 0);
            spectrogram.setVisible (spectrumView != 0);
            bands.setVisible (true);
            refImport.setVisible (true);
            refClear.setVisible (true);
            refPlay.setVisible (true);
            refAlign.setVisible (true);
            break;
        }

        case PageId::stereo:
            meters.setBounds (area);
            meters.setVisible (true);
            break;

        case PageId::diagnostics:
        {
            auto summary = area.removeFromTop (34);
            juce::ignoreUnused (summary);

            issues.setBounds (area.withWidth (juce::roundToInt ((float) area.getWidth() * 0.56f)));
            issues.setVisible (true);
            break;
        }

        case PageId::timeline:
        {
            auto buttons = area.removeFromBottom (34);
            markButton.setBounds (buttons.removeFromRight (104).withSizeKeepingCentre (104, 32));
            buttons.removeFromRight (6);
            markerExport.setBounds (buttons.removeFromRight (118).withSizeKeepingCentre (118, 32));
            buttons.removeFromRight (6);
            markerImport.setBounds (buttons.removeFromRight (118).withSizeKeepingCentre (118, 32));
            markButton.setVisible (true);
            markerExport.setVisible (true);
            markerImport.setVisible (true);
            break;
        }

        case PageId::album:
        {
            auto bottom = area.removeFromBottom (66);
            auto top = bottom.removeFromTop (32);

            albumNameEditor.setBounds (top.removeFromLeft (130).reduced (0, 2));
            top.removeFromLeft (5);
            albumTrackName.setBounds (top.removeFromLeft (112).reduced (0, 2));
            top.removeFromLeft (5);
            albumTrackLength.setBounds (top.removeFromLeft (62).reduced (0, 2));
            top.removeFromLeft (5);

            albumProof.setBounds (top.removeFromRight (88).reduced (0, 1));
            top.removeFromRight (6);
            albumCapture.setBounds (top.removeFromRight (100).reduced (0, 1));

            bottom.removeFromTop (4);
            auto rules = bottom;
            albumClearButton.setBounds (rules.removeFromRight (56).reduced (0, 1));
            rules.removeFromRight (6);
            albumImport.setBounds (rules.removeFromRight (76).reduced (0, 1));
            rules.removeFromRight (8);

            const int ruleW = juce::jmax (56, (rules.getWidth() - 16) / 5);

            for (auto& b : albumRuleButtons)
            {
                b.setBounds (rules.removeFromLeft (ruleW).reduced (0, 1));
                rules.removeFromLeft (4);
            }

            for (auto* c : { static_cast<juce::Component*> (&albumCapture),
                             static_cast<juce::Component*> (&albumProof),
                             static_cast<juce::Component*> (&albumImport),
                             static_cast<juce::Component*> (&albumClearButton),
                             static_cast<juce::Component*> (&albumTrackName),
                             static_cast<juce::Component*> (&albumNameEditor),
                             static_cast<juce::Component*> (&albumTrackLength) })
                c->setVisible (true);

            for (auto& b : albumRuleButtons)
                b.setVisible (true);
            break;
        }

        case PageId::signal:
        {
            auto content = area;
            content.removeFromTop (40 + 8);   // verdict strip + gap

            const int footerH = juce::jlimit (86, 104, content.getHeight() / 3);
            auto bottom = content.removeFromBottom (footerH);

            signalProblemRowBounds = bottom.removeFromTop (28);
            bottom.removeFromTop (4);
            signalAttributionBounds = bottom;

            content.removeFromBottom (tokens::gap);
            signalGridBounds = content;

            auto row = signalProblemRowBounds;
            row.removeFromLeft (juce::jmin (78, row.getWidth() / 5));
            const int chipGap = 6;
            const int chipW = (row.getWidth() - chipGap * 3) / 4;

            for (int i = 0; i < 4; ++i)
            {
                signalChipBounds[(size_t) i] = row.removeFromLeft (chipW);
                row.removeFromLeft (chipGap);
            }

            break;
        }

        case PageId::files:
        {
            auto buttons = area.removeFromBottom (34);
            fileClear.setBounds (buttons.removeFromRight (76).withSizeKeepingCentre (76, 30));
            buttons.removeFromRight (6);
            fileExport.setBounds (buttons.removeFromRight (102).withSizeKeepingCentre (102, 30));
            buttons.removeFromRight (6);
            fileAddAlbum.setBounds (buttons.removeFromRight (92).withSizeKeepingCentre (92, 30));
            buttons.removeFromRight (6);
            fileFolder.setBounds (buttons.removeFromRight (96).withSizeKeepingCentre (96, 30));
            buttons.removeFromRight (6);
            fileChoose.setBounds (buttons.removeFromRight (96).withSizeKeepingCentre (96, 30));

            fileChoose.setVisible (true);
            fileFolder.setVisible (true);
            fileAddAlbum.setVisible (true);
            fileClear.setVisible (true);
            fileExport.setVisible (true);
            break;
        }

        case PageId::target:
        {
            auto controls = area.removeFromTop (58);
            auto row = controls;
            const int genreW = juce::jmin (210, row.getWidth() / 3);
            genreBox.setBounds (row.removeFromLeft (genreW)
                                   .withSizeKeepingCentre (genreW, 28));
            row.removeFromLeft (10);
            applyTarget.setBounds (row.removeFromRight (118).withSizeKeepingCentre (118, 28));
            row.removeFromRight (10);
            warSlider.setBounds (row.withSizeKeepingCentre (row.getWidth(), 28));

            genreBox.setVisible (true);
            warSlider.setVisible (true);
            applyTarget.setVisible (true);
            break;
        }

        default:
            break;
    }
}
} // namespace ml

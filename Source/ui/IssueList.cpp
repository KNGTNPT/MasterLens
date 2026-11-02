#include "Widgets.h"

namespace ml
{
//==============================================================================
IssueList::IssueList()
{
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    viewport.setViewedComponent (&content, false);
    viewport.setOpaque (false);
    addAndMakeVisible (viewport);

    content.onRowClicked = [this] (const juce::String& id)
    {
        if (expanded.count (id) > 0)
            expanded.erase (id);
        else
            expanded.insert (id);

        content.setItems (viewIssuesCache, &expanded);
        content.setSize (juce::jmax (140, getWidth() - 12), content.totalHeight());
        content.repaint();
    };
}

void IssueList::setIssues (std::vector<Issue> items)
{
    juce::String signature;

    for (const auto& i : items)
    {
        // Collapsed rows never show the detail text, so numeric churn inside it
        // must not force a repaint of the whole list.
        const bool open = expanded.count (i.id) > 0;

        signature << i.id << '|' << i.title << '|' << (int) i.severity << '|'
                  << (i.timed ? i.detectedAt : 0.0) << '|'
                  << (open ? i.detail : juce::String()) << ';';
    }

    if (signature == lastSignature)
        return;

    lastSignature = signature;
    viewIssuesCache = std::move (items);

    const int scroll = viewport.getViewPositionY();

    content.setItems (viewIssuesCache, &expanded);
    content.setSize (juce::jmax (140, getWidth() - 12),
                     juce::jmax (content.totalHeight(), viewport.getHeight()));

    viewport.setViewPosition (0, juce::jmin (scroll, juce::jmax (0, content.getHeight() - viewport.getHeight())));
}

void IssueList::resized()
{
    viewport.setBounds (getLocalBounds());

    content.setSize (juce::jmax (140, viewport.getMaximumVisibleWidth()),
                     juce::jmax (content.totalHeight(), viewport.getHeight()));
    grid::alignChildren (*this);
}

void IssueList::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void IssueList::expandFirstForSnapshot()
{
    if (viewIssuesCache.empty())
        return;

    expanded.insert (viewIssuesCache.front().id);
    content.setItems (viewIssuesCache, &expanded);
    content.setSize (juce::jmax (140, getWidth() - 12), content.totalHeight());
    content.repaint();
}

//==============================================================================
void IssueList::Content::setItems (std::vector<Issue> newItems, const std::set<juce::String>* expandedSet)
{
    items = std::move (newItems);
    expandedIds = expandedSet;

    rowHeights.assign (items.size(), collapsedHeight);

    const float available = (float) juce::jmax (200, getWidth() - 110);
    const auto detailFont = getLabelFont();

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (expandedIds == nullptr || expandedIds->count (items[i].id) == 0)
            continue;

        const float textWidth = juce::GlyphArrangement::getStringWidth (detailFont, items[i].detail);
        const int lines = juce::jlimit (1, 4, (int) std::ceil (textWidth / available));
        rowHeights[i] = collapsedHeight + 8 + lines * 19;
    }

    setSize (getWidth(), juce::jmax (1, totalHeight()));
    repaint();
}

int IssueList::Content::rowHeightAt (int index) const noexcept
{
    if (index < 0 || index >= (int) rowHeights.size())
        return 0;

    return rowHeights[(size_t) index];
}

int IssueList::Content::rowTop (int index) const noexcept
{
    int y = 0;

    for (int i = 0; i < index && i < (int) rowHeights.size(); ++i)
        y += rowHeights[(size_t) i];

    return y;
}

int IssueList::Content::totalHeight() const noexcept
{
    int h = 0;

    for (int v : rowHeights)
        h += v;

    return h;
}

int IssueList::Content::indexAtY (int y) const noexcept
{
    int top = 0;

    for (int i = 0; i < (int) items.size(); ++i)
    {
        const int h = rowHeightAt (i);

        if (y >= top && y < top + h)
            return i;

        top += h;
    }

    return -1;
}

void IssueList::Content::mouseDown (const juce::MouseEvent& e)
{
    const int index = indexAtY (e.y);

    if (index >= 0 && onRowClicked != nullptr)
        onRowClicked (items[(size_t) index].id);
}

//==============================================================================
void IssueList::Content::paint (juce::Graphics& g)
{
    g.fillAll (theme::card);

    if (cachedThemeRevision != theme::revision())
        cachedThemeRevision = theme::revision();

    if (items.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (getLabelFont());
        g.drawFittedText (T ("未发现需要处理的项目", "No action items found"),
                          getLocalBounds().reduced (24).withHeight (40),
                          juce::Justification::centred, 2);

        g.setColour (theme::textFaint);
        g.setFont (getLabelFont());
        g.drawFittedText (T ("下方为全部检查项，可逐条核对",
                             "Every check is listed below for verification"),
                          getLocalBounds().reduced (24).withTrimmedTop (44).withHeight (24),
                          juce::Justification::centred, 2);
        return;
    }

    int y = 0;

    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        const int h = rowHeightAt ((int) i);
        const bool open = expandedIds != nullptr && expandedIds->count (item.id) > 0;
        const auto row = juce::Rectangle<int> (0, y, getWidth(), h);
        const auto colour = severityColour (item.severity);

        if (i > 0)
        {
            g.setColour (theme::borderSoft);
            g.drawHorizontalLine (row.getY(), 12.0f, (float) getWidth() - 12.0f);
        }

        g.setColour (colour);
        g.fillEllipse (12.0f, (float) row.getY() + 19.0f, 6.0f, 6.0f);

        g.setColour (theme::text);
        g.setFont (getLabelFont());
        g.drawText (item.title, 30, row.getY() + 11, getWidth() - 30 - 170, 20,
                    juce::Justification::centredLeft);

        const juce::String tag = item.severity == Severity::critical ? T ("需要处理", "Action")
                               : item.severity == Severity::warn ? T ("建议检查", "Review")
                               : item.severity == Severity::info ? T ("可优化", "Improve")
                                                                  : T ("良好", "Good");

        const auto chip = juce::Rectangle<int> (getWidth() - 166, row.getY() + 14, 74, 19);
        g.setColour (colour.withAlpha (0.12f));
        g.fillRoundedRectangle (chip.toFloat(), 9.0f);
        g.setColour (colour);
        g.setFont (getLabelFont());
        g.drawText (tag, chip, juce::Justification::centred);

        if (item.timed)
        {
            g.setColour (theme::textFaint);
            g.setFont (getValueFont());
            g.drawText (fmtTime (item.detectedAt),
                        juce::Rectangle<int> (getWidth() - 86, row.getY() + 17, 50, 14),
                        juce::Justification::centredRight);
        }

        const float cx = (float) getWidth() - 16.0f;
        const float cy = (float) row.getY() + 23.0f;
        juce::Path chevron;

        if (open)
        {
            chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
            chevron.lineTo (cx, cy + 2.5f);
            chevron.lineTo (cx + 4.0f, cy - 2.0f);
        }
        else
        {
            chevron.startNewSubPath (cx - 2.5f, cy - 4.0f);
            chevron.lineTo (cx + 2.0f, cy);
            chevron.lineTo (cx - 2.5f, cy + 4.0f);
        }

        g.setColour (theme::textFaint);
        g.strokePath (chevron, juce::PathStrokeType (1.5f));

        if (open)
        {
            g.setColour (theme::textDim);
            g.setFont (getLabelFont());
            g.drawFittedText (item.detail, 30, row.getY() + 40, getWidth() - 64,
                              juce::jmax (16, h - 46), juce::Justification::topLeft, 4);
        }

        y += h;
    }
}

} // namespace ml

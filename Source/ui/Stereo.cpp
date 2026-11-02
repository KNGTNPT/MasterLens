#include "Widgets.h"

namespace ml
{
void StereoMeters::setValues (const StatsSnapshot& s)
{
    ++updateCount;
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float alpha = lastUpdateMs > 0.0
        ? 1.0f - std::exp (-(float) juce::jlimit (0.001, 0.1, (now-lastUpdateMs)/1000.0)/0.12f) : 1.0f;
    lastUpdateMs = now;
    shownCorrelation += (s.correlation-shownCorrelation)*alpha;
    shownWidth += (s.widthPct-shownWidth)*alpha;
    shownLowSide += (s.lowSidePct-shownLowSide)*alpha;
    stats = s;
    stats.correlation = shownCorrelation;
    stats.widthPct = shownWidth;
    stats.lowSidePct = shownLowSide;
    repaint();
}

void StereoMeters::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();
    if (area.getWidth()<160 || area.getHeight()<140) return;
    auto bands = area.removeFromBottom (juce::jmin (100, area.getHeight()/3));
    area.removeFromBottom (16);
    auto dial = area.removeFromLeft (area.getWidth()*45/100).toFloat();
    area.removeFromLeft (24);
    const float corr = stats.stereo ? stats.correlation : 1.0f;
    const auto status = !stats.stereo ? theme::textFaint
        : corr<0 ? theme::bad : corr<0.15f ? theme::warn : theme::accent;
    const float size = juce::jmin (dial.getWidth()-24.0f, dial.getHeight()-32.0f);
    const float cx = dial.getCentreX(), cy = dial.getCentreY()-6.0f, radius = size*0.44f;
    g.setFont (getLabelFont());
    g.setColour (theme::textDim);
    g.drawText (T("声道相关性", "CHANNEL CORRELATION"),
                dial.withHeight (18.0f), juce::Justification::centred, false);
    juce::Path arc;
    constexpr float start=-2.25f, end=2.25f;
    arc.addCentredArc (cx,cy,radius,radius,0,start,end,true);
    g.setColour (theme::background);
    g.strokePath (arc,juce::PathStrokeType(10.0f));
    g.setColour (theme::border);
    g.strokePath (arc,juce::PathStrokeType(1.0f));
    for(int i=0;i<=20;++i)
    {
        const float angle=start+(end-start)*(float)i/20.0f;
        const float length=i%5==0 ? 10.0f : 4.0f;
        g.setColour (i<10 ? theme::warn.withAlpha(0.55f) : theme::accent.withAlpha(0.7f));
        g.drawLine(cx+std::sin(angle)*(radius+8),cy-std::cos(angle)*(radius+8),
                   cx+std::sin(angle)*(radius+8+length),cy-std::cos(angle)*(radius+8+length),1.0f);
    }
    const float angle=start+(end-start)*juce::jlimit(0.0f,1.0f,(corr+1.0f)*0.5f);
    g.setColour(status);
    g.drawLine(cx,cy,cx+std::sin(angle)*(radius-12),cy-std::cos(angle)*(radius-12),1.6f);
    g.setGradientFill(juce::ColourGradient(theme::raised,cx-5,cy-5,theme::background,cx+5,cy+5,false));
    g.fillEllipse(cx-5,cy-5,10,10);
    g.setColour(theme::textDim);
    g.drawEllipse(cx-5,cy-5,10,10,0.7f);
    g.setColour(theme::text);
    g.setFont(getValueFont());
    g.drawText(stats.stereo ? sgn(corr,2) : noValue(),
        juce::Rectangle<float>(dial.getX(),cy+18,dial.getWidth(),38),juce::Justification::centred,false);
    g.setFont(getValueFont());
    g.setColour(theme::textFaint);
    g.drawText("-1",juce::Rectangle<float>(cx-radius-12,cy+radius*0.75f,30,14),juce::Justification::centred,false);
    g.drawText("+1",juce::Rectangle<float>(cx+radius-18,cy+radius*0.75f,30,14),juce::Justification::centred,false);

    const auto value = [&] (juce::Rectangle<int> cell,const juce::String& label,const juce::String& number)
    {
        g.setFont(getLabelFont()); g.setColour(theme::textFaint);
        g.drawText(label,cell.removeFromTop(20),juce::Justification::centredLeft,false);
        g.setFont(getValueFont());g.setColour(theme::text);
        g.drawText(number,cell,juce::Justification::centredLeft,false);
    };
    auto peaks=area.removeFromTop(juce::jmin(66,area.getHeight()/3));
    const int peakW=peaks.getWidth()/2;
    value(peaks.removeFromLeft(peakW),"L / TRUE PEAK",fmtDb(stats.truePeakLeft)+" dBTP");
    value(peaks,"R / TRUE PEAK",fmtDb(stats.truePeakRight)+" dBTP");
    const auto bar = [&] (juce::Rectangle<int> cell,const juce::String& label,float amount,float range,float threshold)
    {
        auto line=cell.removeFromTop(20);
        g.setFont(getLabelFont());g.setColour(theme::textDim);
        g.drawText(label,line,juce::Justification::centredLeft,false);
        g.setFont(getValueFont());g.setColour(theme::text);
        g.drawText(num(amount)+"%",line,juce::Justification::centredRight,false);
        cell.removeFromTop(8);
        auto track=cell.removeFromTop(8).toFloat();
        surface::recessed(g,track,1.0f);
        g.setColour(amount>threshold ? theme::warn : theme::accent);
        g.fillRect(track.withWidth(track.getWidth()*juce::jlimit(0.0f,1.0f,amount/range)));
    };
    const int rowH=juce::jmin(64,area.getHeight()/3);
    bar(area.removeFromTop(rowH),T("立体声宽度", "STEREO WIDTH"),stats.widthPct,200.0f,150.0f);
    bar(area.removeFromTop(rowH),T("低频侧向", "LOW-END SIDE"),stats.lowSidePct,100.0f,35.0f);
    g.setFont(getValueFont());g.setColour(theme::textDim);
    g.drawText(T("单声道保留 ","MONO RETAIN ")+num(stats.monoRetainPct)+"%  /  "+fmtDb(stats.monoFoldLossDb)+" dB",
               area.removeFromTop(20),juce::Justification::centredLeft,false);
    g.setColour(theme::textFaint);
    g.drawText("M "+fmtDb(stats.midLufs)+"  /  S "+fmtDb(stats.sideLufs)+" LUFS",
               area.removeFromTop(20),juce::Justification::centredLeft,false);
    g.drawText("SIDE TP "+fmtDb(stats.sideTruePeak)+" dBTP",area.removeFromTop(20),juce::Justification::centredLeft,false);

    surface::hairlineH(g,(float)bands.getX(),(float)bands.getRight(),(float)bands.getY());
    bands.removeFromTop(12);
    const int w=bands.getWidth()/4;
    const char* namesZh[]{"低频","中频","高频"};
    const char* namesEn[]{"LOW","MID","HIGH"};
    const char* ranges[]{"<150 Hz","150 Hz - 2 kHz",">2 kHz"};
    const float correlations[]{stats.bandCorrLow,stats.bandCorrMid,stats.bandCorrHigh};
    const float sides[]{stats.bandSideLow,stats.bandSideMid,stats.bandSideHigh};
    for(int i=0;i<3;++i)
    {
        auto cell=bands.removeFromLeft(w);
        g.setFont(getLabelFont());g.setColour(theme::textDim);
        g.drawText(T(namesZh[i],namesEn[i])+" / "+ranges[i],cell.removeFromTop(20),juce::Justification::centredLeft,false);
        g.setFont(getValueFont());g.setColour(theme::text);
        g.drawText(sgn(correlations[i],2),cell.removeFromTop(24),juce::Justification::centredLeft,false);
        g.setFont(getValueFont());g.setColour(theme::textFaint);
        g.drawText(T("侧向 ","SIDE ")+num(sides[i])+"%",cell.removeFromTop(16),juce::Justification::centredLeft,false);
    }
    g.setFont(getLabelFont());g.setColour(theme::textFaint);
    g.drawText(T("事件计数", "EVENT COUNTS"),bands.removeFromTop(20),juce::Justification::centredLeft,false);
    g.setFont(getValueFont());
    g.drawText("CLIP "+juce::String(stats.clipEvents)+" / TP "+juce::String(stats.oversCount),
               bands.removeFromTop(24),juce::Justification::centredLeft,false);
    g.drawText("DC "+juce::String(stats.dcOffsetDb,1)+" dBFS",bands.removeFromTop(20),juce::Justification::centredLeft,false);
}
} // namespace ml

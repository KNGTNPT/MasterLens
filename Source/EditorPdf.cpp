#include "PluginEditor.h"

#include "Delivery.h"
#include "GenreWar.h"
#include "Version.h"

namespace
{
juce::String pdfSafe (juce::String text)
{
    // The built-in Helvetica PDF font cannot render CJK; keep the PDF an
    // English, ASCII-only measurement summary, while TXT/JSON retain the
    // bilingual report.
    juce::String out;

    for (auto c : text)
    {
        if (c == '\\' || c == '(' || c == ')')
        {
            out << '\\' << c;
        }
        else if (c == 0x2212 || c == 0x2013)
        {
            out << '-';
        }
        else if (c >= 32 && c <= 126)
        {
            out << c;
        }
        else if (c == '\n' || c == '\r' || c == '\t')
        {
            out << ' ';
        }
        else
        {
            out << '?';
        }
    }

    return out;
}

juce::String addObject (juce::String& file, int id, const juce::String& body,
                        std::vector<size_t>& offsets)
{
    offsets.push_back ((size_t) file.getNumBytesAsUTF8());
    file << id << " 0 obj\n" << body << "\nendobj\n";
    return {};
}
} // namespace

juce::MemoryBlock MasterLensAudioProcessorEditor::buildReportPdf() const
{
    const auto& s = stats;
    const auto platform = ml::platformSummary (s);
    const auto basis = currentBasis();
    const auto gate = ml::album::Session::evaluate (albumSnapshot);

    std::vector<juce::String> lines;
    lines.push_back ("MasterLens " + juce::String (ml::kVersion) + " - mastering QC report");
    lines.push_back ("Generated: " + juce::Time::getCurrentTime().toISO8601 (true));
    lines.push_back ("Sample rate: " + juce::String (processor.getSampleRateValue(), 0)
                     + " Hz   Channels: " + (s.stereo ? "stereo" : "mono"));
    lines.push_back ("Basis: " + basis.label() + "   Coverage: "
                     + juce::String (s.coveragePct, 1) + "%");
    lines.push_back ("");
    lines.push_back ("LOUDNESS / DYNAMICS");
    lines.push_back ("Integrated: " + ml::fmtDb (s.integrated) + " LUFS");
    lines.push_back ("Momentary / short-term: " + ml::fmtDb (s.momentary) + " / "
                     + ml::fmtDb (s.shortTerm) + " LUFS");
    lines.push_back ("LRA / PLR: " + ml::fmt1 (s.lra) + " LU / " + ml::fmt1 (s.getPlr()) + " dB");
    lines.push_back ("Mid / Side integrated: " + ml::fmtDb (s.midLufs) + " / "
                     + ml::fmtDb (s.sideLufs) + " LUFS");
    lines.push_back ("True peak: " + ml::fmtDb (s.truePeakDb) + " dBTP   ceiling "
                     + ml::num (processor.getCeilingDb(), 1) + " dBTP");
    lines.push_back ("L / R / Side true peak: " + ml::fmtDb (s.truePeakLeft) + " / "
                     + ml::fmtDb (s.truePeakRight) + " / " + ml::fmtDb (s.sideTruePeak) + " dBTP");
    lines.push_back ("Correlation / width: " + ml::sgn (s.correlation, 2) + " / "
                     + ml::fmt1 (s.widthPct) + "%");
    lines.push_back ("Clipping / clicks / overs: " + juce::String (s.clipEvents) + " / "
                     + juce::String (s.clickEvents) + " / " + juce::String (s.oversCount));
    lines.push_back ("");
    lines.push_back ("PLATFORM NORMALISATION");
    lines.push_back ("Worst cut: " + ml::fmt1 (platform.maxCutDb) + " dB   wasted "
                     + ml::fmt1 (platform.maxWastedDb) + " LU   service limiting "
                     + ml::fmt1 (platform.maxLimitedDb) + " dB");

    for (int i = 0; i < ml::numPlatforms; ++i)
    {
        const auto& p = ml::platform (i);
        const auto r = ml::evaluate (s, p);
        lines.push_back ("  " + juce::String (p.nameEn) + ": gain " + ml::sgn (r.gainDb)
                         + " dB, predicted TP " + ml::fmt1 (r.predictedTp)
                         + " dBTP, limited " + ml::fmt1 (r.limitedDb) + " dB");
    }

    lines.push_back ("");
    lines.push_back ("ALBUM GATE");
    lines.push_back ("Album: " + albumSnapshot.albumName + "   revision "
                     + juce::String (albumSnapshot.revision) + "   tracks "
                     + juce::String ((int) albumSnapshot.tracks.size()));
    lines.push_back (gate.blocked ? "Result: BLOCKED" : "Result: PASS");
    lines.push_back ("Blockers: " + juce::String (gate.blockers));

    for (int i = 0; i < juce::jmin (5, (int) gate.risks.size()); ++i)
    {
        const auto& r = gate.risks[(size_t) i];
        lines.push_back ("  " + r.track + " - " + r.dimension + " - " + r.detail);
    }

    if (lines.size() > 58)
        lines.resize (58);

    juce::String content;
    content << "BT\n/F1 9 Tf\n50 790 Td\n12 TL\n";

    for (const auto& line : lines)
        content << "(" << pdfSafe (line) << ") Tj\nT*\n";

    content << "ET\n";

    juce::String file;
    file << "%PDF-1.4\n";
    std::vector<size_t> offsets;

    addObject (file, 1, "<< /Type /Catalog /Pages 2 0 R >>", offsets);
    addObject (file, 2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>", offsets);
    addObject (file, 3,
               "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
               "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>",
               offsets);
    addObject (file, 4,
               "<< /Length " + juce::String (content.getNumBytesAsUTF8()) + " >>\nstream\n"
                   + content + "endstream",
               offsets);
    addObject (file, 5, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>", offsets);

    const size_t xref = (size_t) file.getNumBytesAsUTF8();
    file << "xref\n0 6\n0000000000 65535 f \n";

    for (size_t offset : offsets)
        file << juce::String (offset).paddedLeft ('0', 10) << " 00000 n \n";

    file << "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n"
         << juce::String (xref) << "\n%%EOF\n";

    juce::MemoryBlock block;
    block.append (file.toRawUTF8(), (size_t) file.getNumBytesAsUTF8());
    return block;
}

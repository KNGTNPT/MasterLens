#include "PluginEditor.h"

using ml::T;

#include <algorithm>

namespace
{
juce::String timecode (double seconds)
{
    const int totalMs = juce::jmax (0, (int) std::round (seconds * 1000.0));
    const int h = totalMs / 3600000;
    const int m = (totalMs / 60000) % 60;
    const int s = (totalMs / 1000) % 60;
    const int ms = totalMs % 1000;

    return juce::String (h).paddedLeft ('0', 2) + ":"
         + juce::String (m).paddedLeft ('0', 2) + ":"
         + juce::String (s).paddedLeft ('0', 2) + "."
         + juce::String (ms).paddedLeft ('0', 3);
}

juce::StringArray csvFields (const juce::String& line)
{
    juce::StringArray out;
    juce::String current;
    bool quoted = false;

    for (int i = 0; i < line.length(); ++i)
    {
        const auto c = line[i];

        if (c == '"')
        {
            if (quoted && i + 1 < line.length() && line[i + 1] == '"')
            {
                current += '"';
                ++i;
            }
            else
            {
                quoted = ! quoted;
            }
        }
        else if (c == ',' && ! quoted)
        {
            out.add (current.trim());
            current.clear();
        }
        else
        {
            current += c;
        }
    }

    out.add (current.trim());
    return out;
}

double parseTime (const juce::String& text)
{
    auto t = text.trim().unquoted();

    if (t.containsChar (':'))
    {
        auto parts = juce::StringArray::fromTokens (t, ":", "");
        parts.removeEmptyStrings();

        if (parts.size() == 3)
            return parts[0].getDoubleValue() * 3600.0
                 + parts[1].getDoubleValue() * 60.0
                 + parts[2].getDoubleValue();

        if (parts.size() == 2)
            return parts[0].getDoubleValue() * 60.0 + parts[1].getDoubleValue();
    }

    return t.getDoubleValue();
}
} // namespace

void MasterLensAudioProcessorEditor::exportMarkers()
{
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M");
    const auto file = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                          .getChildFile ("MasterLens-Markers-" + stamp + ".csv");

    juce::StringArray lines;
    lines.add ("timecode,time_seconds,severity,category,text");

    for (const auto& m : markers)
    {
        auto text = m.text.replace ("\"", "\"\"");
        lines.add (timecode (m.time) + "," + juce::String (m.time, 3) + ","
                   + juce::String (m.severity) + "," + juce::String (m.category)
                   + ",\"" + text + "\"");
    }

    if (! ml::writeTextAtomic (file, lines.joinIntoString ("\n"))) { setExportResult (false); return; }

    footerStatusText = T ("标记已导出 ", "Markers exported ") + file.getFileName();
    footerStatusColour = ml::theme::good;
    repaint();
}

void MasterLensAudioProcessorEditor::importMarkers()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        T ("导入 DAW 标记 / 标记 CSV", "Import DAW markers / marker CSV"),
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "*.csv;*.txt");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this, lifetime = juce::Component::SafePointer<MasterLensAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (lifetime == nullptr) return;
                                  const auto file = fc.getResult();

                                  if (file == juce::File {})
                                      return;

                                  auto lines = juce::StringArray::fromLines (file.loadFileAsString());
                                  markers.erase (std::remove_if (markers.begin(), markers.end(),
                                                                 [] (const ml::Marker& m)
                                                                 { return m.category == 5; }),
                                                 markers.end());

                                  bool first = true;
                                  int added = 0;

                                  for (const auto& line : lines)
                                  {
                                      if (line.trim().isEmpty())
                                          continue;

                                      auto fields = csvFields (line);

                                      if (first)
                                      {
                                          first = false;

                                          if (line.containsIgnoreCase ("time"))
                                              continue;
                                      }

                                      if (fields.isEmpty())
                                          continue;

                                      double time = -1.0;
                                      juce::String text;
                                      int severity = 1;

                                      for (const auto& f : fields)
                                      {
                                          const bool looksTime = f.containsChar (':')
                                              || (f.containsOnly ("0123456789.-")
                                                  && f.getDoubleValue() >= 0.0
                                                  && (f.containsChar ('.') || f.length() > 1));

                                          if (time < 0.0 && looksTime)
                                              time = parseTime (f);
                                          else if (f.containsOnly ("0123456789") && f.length() == 1)
                                              severity = f.getIntValue();
                                          else if (f.length() > 3)
                                              text = f;
                                      }

                                      if (time < 0.0)
                                          continue;

                                      if (text.isEmpty())
                                          text = "DAW marker";

                                      ml::Marker m;
                                      m.time = time;
                                      m.severity = juce::jlimit (0, 2, severity);
                                      m.category = 5;
                                      m.text = text;
                                      markers.push_back (m);
                                      ++added;
                                  }

                                  std::sort (markers.begin(), markers.end(),
                                             [] (const ml::Marker& a, const ml::Marker& b)
                                             { return a.time < b.time; });

                                  timeline.setMarkers (markers, juce::jmax (30.0, stats.elapsed));
                                  footerStatusText = T ("已导入 ", "Imported ") + juce::String (added)
                                                   + T (" 个标记", " markers");
                                  footerStatusColour = ml::theme::good;
                                  repaint();
                              });
}

#pragma once
#include <juce_core/juce_core.h>
namespace ml
{
inline bool writeTextAtomic (const juce::File& file, const juce::String& text)
{
    if (! file.getParentDirectory().isDirectory() || file.hasFileExtension ("wav;aif;aiff;flac;mp3;m4a")) return false;
    juce::TemporaryFile temp (file);
    return temp.getFile().replaceWithText (text) && temp.overwriteTargetFileWithTemporary();
}
inline bool writeDataAtomic (const juce::File& file, const juce::MemoryBlock& data)
{
    if (! file.getParentDirectory().isDirectory()) return false;
    juce::TemporaryFile temp (file);
    return temp.getFile().replaceWithData (data.getData(), data.getSize()) && temp.overwriteTargetFileWithTemporary();
}
}

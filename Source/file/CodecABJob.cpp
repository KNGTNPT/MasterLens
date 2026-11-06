#include "CodecABJob.h"

#include "CodecCheck.h"

namespace ml
{
CodecABJob::CodecABJob()
    : juce::Thread ("MasterLens codec A/B")
{
}

CodecABJob::~CodecABJob()
{
    signalThreadShouldExit();
    notify();
    stopThread (4000);

    const juce::SpinLock::ScopedLockType sl (lock);

    if (workDirectory != juce::File())
        workDirectory.deleteRecursively();
}

void CodecABJob::start (const juce::File& source, const juce::String& codec)
{
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        pendingSource = source;
        pendingCodec = codec;
        statusText = "encoding";
        hasResult = false;
    }

    busy.store (true, std::memory_order_release);
    pending.store (true, std::memory_order_release);
    request.fetch_add (1, std::memory_order_acq_rel);

    if (! isThreadRunning())
        startThread();
    else
        notify();
}

void CodecABJob::cancel()
{
    pending.store (false, std::memory_order_release);
    request.fetch_add (1, std::memory_order_acq_rel);
    busy.store (false, std::memory_order_release);

    const juce::SpinLock::ScopedLockType sl (lock);
    statusText = "cancelled";
    hasResult = false;
}

void CodecABJob::clear()
{
    cancel();
    signalThreadShouldExit();
    notify();
    stopThread (4000);

    const juce::SpinLock::ScopedLockType sl (lock);

    if (workDirectory != juce::File())
    {
        workDirectory.deleteRecursively();
        workDirectory = juce::File();
    }
}

juce::String CodecABJob::getStatus() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return statusText;
}

bool CodecABJob::takeResult (CodecABResult& out)
{
    const juce::SpinLock::ScopedLockType sl (lock);

    if (! hasResult)
        return false;

    out = result;
    hasResult = false;
    return true;
}

juce::File CodecABJob::getDecodedFile() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return hasResult ? result.decodedFile : juce::File();
}

void CodecABJob::run()
{
    while (! threadShouldExit())
    {
        if (! pending.exchange (false, std::memory_order_acq_rel))
        {
            wait (40);
            continue;
        }

        const auto serial = request.load (std::memory_order_acquire);
        juce::File source;
        juce::String codec;

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            source = pendingSource;
            codec = pendingCodec;
        }

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            statusText = "encoding";
        }

        const auto parent = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("MasterLensCodec");
        auto preview = createCodecPreview (source, codec, parent);

        if (request.load() != serial)
        {
            if (preview.workDirectory != juce::File())
                preview.workDirectory.deleteRecursively();

            continue;
        }

        CodecABResult measured;
        measured.codec = preview.codec;
        measured.label = preview.codec == "aac" ? "AAC 256k" : "MP3 320k";
        measured.decodedFile = preview.decodedFile;
        measured.encodedBytes = preview.encodedBytes;

        if (! preview.ok)
        {
            measured.error = preview.error;

            const juce::SpinLock::ScopedLockType sl (lock);
            result = measured;
            hasResult = true;
            statusText = "failed";
            busy.store (false, std::memory_order_release);
            continue;
        }

        {
            const juce::SpinLock::ScopedLockType sl (lock);
            statusText = "measuring";
        }

        FileAnalyzer analyzer;
        analyzer.analyse ({ preview.decodedFile.getFullPathName() });

        for (int wait = 0; wait < 2400 && analyzer.isBusy(); ++wait)
        {
            if (threadShouldExit() || request.load() != serial)
                break;

            juce::Thread::sleep (10);
        }

        auto results = analyzer.takeResults();

        if (request.load() != serial)
        {
            preview.workDirectory.deleteRecursively();
            continue;
        }

        if (results.size() != 1 || ! results[0].ok)
        {
            measured.error = "decoded file analysis failed";
            preview.workDirectory.deleteRecursively();
        }
        else
        {
            measured.ok = true;
            measured.decodedLufs = results[0].stats.integrated;
            measured.decodedTp = results[0].stats.truePeakDb;
        }

        {
            const juce::SpinLock::ScopedLockType sl (lock);

            if (workDirectory != juce::File() && workDirectory != preview.workDirectory)
                workDirectory.deleteRecursively();

            workDirectory = measured.ok ? preview.workDirectory : juce::File();
            result = measured;
            hasResult = true;
            statusText = measured.ok ? "ready" : "failed";
        }

        busy.store (false, std::memory_order_release);
    }
}
} // namespace ml

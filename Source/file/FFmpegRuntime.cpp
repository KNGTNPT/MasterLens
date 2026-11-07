#include "FFmpegRuntime.h"

#include <thread>

#if JUCE_MAC
 #include <dlfcn.h>
 #include <unistd.h>
#endif

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace ml
{
namespace
{
constexpr int kOutputCap = 8192;

juce::String env (const char* name)
{
    return juce::SystemStats::getEnvironmentVariable (name, {}).trim();
}

bool isExecutable (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

#if JUCE_WINDOWS
    return true;
#else
    return ::access (file.getFullPathName().toRawUTF8(), X_OK) == 0;
#endif
}

void appendBounded (juce::String& target, const juce::String& line)
{
    target << line << "\n";

    if (target.length() > kOutputCap)
        target = target.substring (target.length() - kOutputCap);
}

/** Captures a short command's combined output for capability probing. */
int captureOutput (const juce::String& executable, const juce::StringArray& arguments,
                   int timeoutMs, juce::String& output)
{
    juce::ChildProcess process;
    juce::StringArray args;
    args.add (executable);
    args.addArray (arguments);

    if (! process.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return -1;

    // Probe output is small (< 64 KB pipe buffer), so wait for exit first and then
    // drain synchronously. A reader thread used to race very short scripts: the
    // child could exit before the thread performed its first read.
    if (! process.waitForProcessToFinish (timeoutMs))
    {
        process.kill();
        process.waitForProcessToFinish (1000);
        return -2;
    }

    char buffer[4096];

    for (;;)
    {
        const int n = process.readProcessOutput (buffer, (int) sizeof (buffer));

        if (n <= 0)
            break;

        output << juce::String::fromUTF8 (buffer, n);
    }

    return (int) process.getExitCode();
}
} // namespace

FFmpegRuntime& FFmpegRuntime::get()
{
    // Intentionally leaked: DAW unload / exit order must not run a destructor
    // while a background job is still reading the cached executable path.
    static FFmpegRuntime* instance = new FFmpegRuntime();
    return *instance;
}

FFmpegRuntime::FFmpegRuntime() = default;

void FFmpegRuntime::refresh()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    resolved = false;
    executable = juce::File();
    source = "none";
    status = "unresolved";
    capabilities = {};
}

bool FFmpegRuntime::resolve()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return resolveLocked();
}

bool FFmpegRuntime::resolveLocked()
{
    if (resolved)
        return executable.existsAsFile() && capabilities.aacEncode;

    resolved = true;

    const auto overridePath = env ("MASTERLENS_FFMPEG");

    if (overridePath.isNotEmpty())
    {
        const juce::File candidate (overridePath);
        source = "override";

        if (! candidate.existsAsFile())
        {
            status = "override path missing: " + overridePath;
            return false;
        }

        executable = candidate.getFullPathName();
        juce::String why;

        if (! probe (why))
        {
            status = "override unusable: " + why;
            return false;
        }

        return true;
    }

    const auto bundled = findBundledExecutable();

    if (bundled.existsAsFile())
    {
        source = "bundled";
        executable = bundled;

        juce::String why;

        if (! probe (why))
        {
            // Never fall back behind a broken bundled runtime: that would hide a
            // packaging mistake.
            status = "bundled runtime unusable: " + why;
            return false;
        }

        return true;
    }

    if (env ("MASTERLENS_ALLOW_SYSTEM_FFMPEG") == "1")
    {
        const auto system = findOnPath ("ffmpeg");

        if (system.existsAsFile())
        {
            source = "system";
            executable = system;

            juce::String why;

            if (! probe (why))
            {
                status = "system runtime unusable: " + why;
                return false;
            }

            return true;
        }
    }

    status = "not bundled";
    return false;
}
bool FFmpegRuntime::probe (juce::String& why)
{
    capabilities = {};

    if (! executable.existsAsFile())
    {
        why = "missing executable";
        return false;
    }

    if (! isExecutable (executable))
    {
        why = "not executable";
        return false;
    }

    juce::String versionOut;
    const int versionCode = captureOutput (executable.getFullPathName(),
                                           { "-hide_banner", "-version" }, 8000, versionOut);

    if (versionCode != 0 || versionOut.isEmpty())
    {
        why = "version probe failed (exit " + juce::String (versionCode) + ")";
        return false;
    }

    const auto lines = juce::StringArray::fromLines (versionOut);

    for (const auto& line : lines)
    {
        if (line.startsWith ("ffmpeg version"))
            capabilities.version = line.fromFirstOccurrenceOf ("ffmpeg version ", false, false)
                                       .upToFirstOccurrenceOf (" ", false, false);
        else if (line.contains ("configuration:"))
            capabilities.configuration = line.fromFirstOccurrenceOf ("configuration:", false, false).trim();
    }

    juce::String encoderOut;
    const int encoderCode = captureOutput (executable.getFullPathName(),
                                           { "-hide_banner", "-encoders" }, 8000, encoderOut);

    if (encoderCode != 0)
    {
        why = "encoder probe failed (exit " + juce::String (encoderCode) + ")";
        return false;
    }

    capabilities.aacEncode = encoderOut.contains (" aac ");
    capabilities.mp3Encode = encoderOut.contains (" libmp3lame ");
    capabilities.licenceLgpl = ! capabilities.configuration.contains ("--enable-gpl")
                               && ! capabilities.configuration.contains ("--enable-nonfree");

    if (! capabilities.aacEncode)
    {
        why = "AAC encoder missing (need an LGPL build with the native aac encoder)";
        return false;
    }

    status = (source == "bundled" ? "bundled " : source == "override" ? "override " : "system ")
             + capabilities.version
             + (capabilities.licenceLgpl ? " (LGPL)" : " (GPL/nonfree - do not redistribute)");
    return true;
}

bool FFmpegRuntime::isAvailable() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return executable.existsAsFile() && capabilities.aacEncode;
}

bool FFmpegRuntime::supports (const juce::String& codec) const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    const auto c = codec.toLowerCase();

    if (c == "aac") return capabilities.aacEncode;
    if (c == "mp3") return capabilities.mp3Encode;
    return false;
}

juce::File FFmpegRuntime::getExecutable() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return executable;
}

juce::String FFmpegRuntime::getSource() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return source;
}

juce::String FFmpegRuntime::getStatusText() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status;
}

juce::String FFmpegRuntime::getDiagnostics() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return status + (capabilities.configuration.isNotEmpty()
                         ? "\n" + capabilities.configuration : juce::String());
}

const FFmpegRuntime::Capabilities& FFmpegRuntime::getCapabilities() const
{
    return capabilities;
}

bool FFmpegRuntime::runtimeRequiredForRelease()
{
    return env ("MASTERLENS_REQUIRE_RUNTIME") == "1";
}
juce::File FFmpegRuntime::findOnPath (const juce::String& name)
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("PATH", {});
   #if JUCE_WINDOWS
    const juce::String separator (";");
   #else
    const juce::String separator (":");
   #endif

    for (const auto& dir : juce::StringArray::fromTokens (path, separator, ""))
    {
        if (dir.isEmpty())
            continue;

        for (const auto& candidate : { name, name + ".exe" })
        {
            const juce::File file (juce::File (dir).getChildFile (candidate));

            if (file.existsAsFile())
                return file.getFullPathName();
        }
    }

    return {};
}

juce::File FFmpegRuntime::findBundledExecutable()
{
    juce::File module;

#if JUCE_MAC
    Dl_info info {};

    if (dladdr ((const void*) &FFmpegRuntime::findBundledExecutable, &info) != 0
        && info.dli_fname != nullptr)
        module = juce::File (juce::String::fromUTF8 (info.dli_fname)).getFullPathName();
#elif JUCE_WINDOWS
    HMODULE handle = nullptr;

    if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR) &FFmpegRuntime::findBundledExecutable, &handle))
    {
        wchar_t path[MAX_PATH * 4] {};

        if (GetModuleFileNameW (handle, path, (DWORD) (sizeof (path) / sizeof (wchar_t))) > 0)
            module = juce::File (juce::String (path));
    }
#endif

    if (module == juce::File())
        return {};

    auto dir = module.getParentDirectory();

    for (int depth = 0; depth < 10; ++depth)
    {
#if JUCE_MAC
        if (dir.hasFileExtension ("vst3") || dir.hasFileExtension ("component")
            || dir.hasFileExtension ("app"))
        {
            const auto candidate = dir.getChildFile ("Contents").getChildFile ("Resources")
                                      .getChildFile ("ffmpeg");

            if (candidate.existsAsFile())
                return candidate;
        }
#elif JUCE_WINDOWS
        if (dir.getFileName().endsWithIgnoreCase (".vst3"))
        {
            const auto candidate = dir.getChildFile ("Contents").getChildFile ("Resources")
                                      .getChildFile ("ffmpeg").getChildFile ("ffmpeg.exe");

            if (candidate.existsAsFile())
                return candidate;
        }

        {
            const auto candidate = dir.getChildFile ("ffmpeg").getChildFile ("ffmpeg.exe");

            if (candidate.existsAsFile())
                return candidate;
        }
#endif
        const auto parent = dir.getParentDirectory();

        if (parent == dir)
            break;

        dir = parent;
    }

    return {};
}
FFmpegRuntime::RunResult FFmpegRuntime::run (const juce::StringArray& arguments,
                                             double timeoutSeconds,
                                             const std::atomic<bool>* cancelFlag,
                                             const std::function<void (juce::int64)>& onProgress)
{
    RunResult result;

    if (! resolve())
    {
        result.error = "unavailable";
        result.message = getStatusText();
        return result;
    }

    const auto executablePath = getExecutable().getFullPathName();
    juce::StringArray args;
    args.add (executablePath);
    args.addArray (arguments);

    juce::ChildProcess process;

    if (! process.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
    {
        result.error = "launch";
        result.message = "could not start " + executablePath;
        return result;
    }

    juce::String output;
    juce::SpinLock outputLock;
    std::atomic<double> lastOutputMs { juce::Time::getMillisecondCounterHiRes() };

    struct Reader final : juce::Thread
    {
        Reader (juce::ChildProcess& p, juce::String& sink, juce::SpinLock& l,
                const std::function<void (juce::int64)>& cb, std::atomic<double>& last)
            : juce::Thread ("MasterLens ffmpeg output"), process (p), output (sink),
              lock (l), progress (cb), lastOutput (last) {}

        void run() override
        {
            char buffer[4096];
            juce::String pending;

            while (! threadShouldExit())
            {
                const int n = process.readProcessOutput (buffer, (int) sizeof (buffer));

                if (n > 0)
                {
                    lastOutput.store (juce::Time::getMillisecondCounterHiRes());
                    pending << juce::String::fromUTF8 (buffer, n);

                    int lineEnd = 0;

                    while ((lineEnd = pending.indexOfChar ('\n')) >= 0)
                    {
                        const auto line = pending.substring (0, lineEnd).trim();
                        pending = pending.substring (lineEnd + 1);

                        {
                            const juce::SpinLock::ScopedLockType sl (lock);
                            output << line << "\n";

                            if (output.length() > kOutputCap)
                                output = output.substring (output.length() - kOutputCap);
                        }

                        if (progress)
                        {
                            if (line.startsWith ("out_time_us="))
                                progress (line.fromFirstOccurrenceOf ("=", false, false).getLargeIntValue());
                            else if (line.startsWith ("out_time_ms="))
                                progress (line.fromFirstOccurrenceOf ("=", false, false).getLargeIntValue());
                        }
                    }
                }
                else if (! process.isRunning())
                {
                    break;
                }
            }
        }

        juce::ChildProcess& process;
        juce::String& output;
        juce::SpinLock& lock;
        const std::function<void (juce::int64)>& progress;
        std::atomic<double>& lastOutput;
    };

    Reader reader (process, output, outputLock, onProgress, lastOutputMs);
    reader.startThread();

    const double deadline = juce::Time::getMillisecondCounterHiRes()
                            + juce::jmax (1.0, timeoutSeconds) * 1000.0;
    bool cancelled = false, timedOut = false, stalled = false;

    while (process.isRunning())
    {
        const double now = juce::Time::getMillisecondCounterHiRes();

        if (cancelFlag != nullptr && cancelFlag->load (std::memory_order_acquire))
        {
            cancelled = true;
            process.kill();
            break;
        }

        if (now > deadline)
        {
            timedOut = true;
            process.kill();
            break;
        }

        if (onProgress && now - lastOutputMs.load() > 20000.0)
        {
            stalled = true;
            process.kill();
            break;
        }

        juce::Thread::sleep (20);
    }

    process.waitForProcessToFinish (2000);
    reader.signalThreadShouldExit();
    reader.notify();
    reader.stopThread (2000);

    result.exitCode = process.isRunning() ? -1 : (int) process.getExitCode();

    {
        const juce::SpinLock::ScopedLockType sl (outputLock);
        result.outputTail = output.length() <= 1600 ? output : output.substring (output.length() - 1600);
    }

    result.ok = ! cancelled && ! timedOut && ! stalled && ! process.isRunning()
                && result.exitCode == 0;

    if (cancelled)
    {
        result.error = "cancelled";
        result.message = "cancelled";
    }
    else if (timedOut || stalled)
    {
        result.error = "timeout";
        result.message = stalled ? "no output for 20 s" : "timeout";
    }
    else if (! result.ok)
    {
        result.error = "failed";
        result.message = "ffmpeg exited " + juce::String (result.exitCode)
                         + (result.outputTail.isNotEmpty() ? "\n" + result.outputTail : juce::String());
    }

    return result;
}
} // namespace ml

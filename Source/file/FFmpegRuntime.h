#pragma once

#include <atomic>
#include <functional>

#include <juce_core/juce_core.h>

namespace ml
{
/** One resolved FFmpeg runtime shared by the editor, the file QC job and the CLI.

    Resolution order (never a silent PATH lookup in packaged builds):
      1. `MASTERLENS_FFMPEG` — explicit override. If set but unusable the runtime
         reports an error instead of falling back, so a misconfiguration is visible.
      2. The executable shipped inside the current bundle
         (`Contents/Resources/ffmpeg`, `.../ffmpeg/ffmpeg.exe`).
      3. `PATH` only when `MASTERLENS_ALLOW_SYSTEM_FFMPEG=1` (development / CI).

    The runtime also owns the child-process contract: bounded output draining,
    progress parsing, cancellation, timeout and exit-code classification.
*/
class FFmpegRuntime
{
public:
    struct Capabilities
    {
        bool aacEncode = false;
        bool mp3Encode = false;
        bool licenceLgpl = false;
        juce::String version;
        juce::String configuration;
    };

    struct RunResult
    {
        bool ok = false;
        int exitCode = -1;
        juce::String error;       // launch | unavailable | cancelled | timeout | failed | ""
        juce::String message;     // human readable, includes the output tail
        juce::String outputTail;
    };

    static FFmpegRuntime& get();

    /** Resolves and probes once; returns true when a usable runtime is present. */
    bool resolve();
    /** Drops the cached resolution (tests install a fake runtime, env changes). */
    void refresh();

    bool isAvailable() const;
    bool supports (const juce::String& codec) const;
    juce::File getExecutable() const;
    juce::String getSource() const;          // bundled | override | system | none
    juce::String getStatusText() const;      // one line for tooltips / reports
    juce::String getDiagnostics() const;     // version + source + licence
    const Capabilities& getCapabilities() const;

    /** Runs ffmpeg (the executable is prepended automatically).

        @param overallTimeoutSeconds  hard wall-clock limit, must be > 0
        @param cancelFlag             polled every few ms; kills the child
        @param onProgress             receives ffmpeg's out_time_us values
    */
    RunResult run (const juce::StringArray& arguments,
                   double overallTimeoutSeconds,
                   const std::atomic<bool>* cancelFlag = nullptr,
                   const std::function<void (juce::int64)>& onProgress = {});

    /** Executable shipped with the current VST3 / AU / Standalone / CLI module. */
    static juce::File findBundledExecutable();

    /** True when the build is expected to ship the runtime (release gate). */
    static bool runtimeRequiredForRelease();

    static juce::String defaultTimeoutEnvName() { return "MASTERLENS_CODEC_TIMEOUT_MS"; }

private:
    FFmpegRuntime();

    bool probe (juce::String& why);
    bool resolveLocked();
    static juce::File findOnPath (const juce::String& name);

    mutable juce::SpinLock lock;
    bool resolved = false;
    juce::File executable;
    juce::String source { "none" };
    juce::String status { "unresolved" };
    Capabilities capabilities;
};
} // namespace ml

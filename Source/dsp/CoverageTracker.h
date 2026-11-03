#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace ml
{
/** Audio-thread-only exact union of visited host-time intervals. Fixed capacity,
    no allocation/locks. Overflow is explicit, so it cannot produce a false pass. */
class CoverageTracker
{
public:
    void reset() noexcept { count = 0; overflow = false; }
    void add (double start, double end) noexcept
    {
        if (! std::isfinite(start) || ! std::isfinite(end) || end <= start || end <= 0) return;
        start = std::max (0.0, start);
        size_t first = 0;
        while (first < count && spans[first].end < start - 1.0e-9) ++first;
        size_t last = first;
        while (last < count && spans[last].start <= end + 1.0e-9)
        { start = std::min (start, spans[last].start); end = std::max (end, spans[last].end); ++last; }
        if (first == last)
        {
            if (count == spans.size()) { overflow = true; return; }
            for (size_t i = count; i > first; --i) spans[i] = spans[i-1];
            ++count;
        }
        else
        {
            const auto removed = last - first - 1;
            for (size_t i = first + 1; i + removed < count; ++i) spans[i] = spans[i + removed];
            count -= removed;
        }
        spans[first] = { start, end };
    }
    double seconds (double duration) const noexcept
    {
        if (overflow || ! std::isfinite(duration) || duration <= 0) return -1.0;
        double sum = 0;
        for (size_t i = 0; i < count && spans[i].start < duration; ++i)
            sum += std::max (0.0, std::min (duration, spans[i].end) - spans[i].start);
        return std::min (duration, sum);
    }
private:
    struct Interval { double start = 0, end = 0; };
    std::array<Interval,4096> spans {};
    size_t count = 0;
    bool overflow = false;
};
}

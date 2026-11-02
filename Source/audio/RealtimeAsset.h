#pragma once
#include <atomic>
#include <memory>
#include <vector>
#include <algorithm>
namespace ml
{
/** Single audio-reader / single worker-publisher exchange. All ownership and
    reclamation stay on the worker. A callback never locks, allocates, retries,
    or releases an owning reference. A raced publication skips one block. */
template <class T> class RealtimeAsset
{
public:
    struct Read
    {
        RealtimeAsset& owner;
        const T* ptr;
        explicit Read (RealtimeAsset& o) noexcept : owner (o), ptr (o.published.load())
        {
            o.hazard.store (ptr);
            if (ptr != o.published.load()) { ptr = nullptr; o.hazard.store (nullptr); }
        }
        ~Read() { owner.hazard.store (nullptr); }
        Read (const Read&) = delete;
        const T* operator->() const noexcept { return ptr; }
        explicit operator bool() const noexcept { return ptr != nullptr; }
    };
    Read acquire() noexcept { return Read (*this); }
    void publish (std::unique_ptr<T> next)
    {
        auto old = std::move (owned);
        owned = std::move (next);
        published.store (owned.get());
        if (old) retired.push_back (std::move (old));
        collect();
    }
    void collect()
    {
        const auto* reading = hazard.load();
        retired.erase (std::remove_if (retired.begin(), retired.end(),
            [reading] (const auto& p) { return p.get() != reading; }), retired.end());
    }
private:
    std::atomic<const T*> published { nullptr }, hazard { nullptr };
    std::unique_ptr<T> owned;
    std::vector<std::unique_ptr<T>> retired;
};
}

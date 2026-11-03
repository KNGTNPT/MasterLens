#pragma once
#include <cstdlib>
#include <new>
namespace rtAudit
{
inline thread_local bool active = false;
inline thread_local unsigned allocations = 0, releases = 0;
struct Scope
{
    Scope() { allocations = releases = 0; active = true; }
    ~Scope() { active = false; }
};
}
void* operator new (std::size_t size)
{
    if (rtAudit::active) ++rtAudit::allocations;
    if (auto* p = std::malloc (size == 0 ? 1 : size)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size) { return ::operator new (size); }
void operator delete (void* p) noexcept { if (p && rtAudit::active) ++rtAudit::releases; std::free(p); }
void operator delete[] (void* p) noexcept { ::operator delete(p); }
void operator delete (void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[] (void* p, std::size_t) noexcept { ::operator delete(p); }

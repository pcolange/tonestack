// Verifies the real-time contract: once prepared, a node's process() performs no heap
// allocation. Global operator new — including the C++17 over-aligned forms — is overridden
// to count allocations while a guard flag is set; the flag is only active around the
// process() loop.

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#if defined(_MSC_VER)
  #include <malloc.h>
#endif

#include "test_framework.h"
#include "tonestack/Chain.h"
#include "tonestack/nodes/BiquadNode.h"
#include "tonestack/nodes/GainNode.h"
#include "tonestack/nodes/IirNode.h"
#include "tonestack/nodes/OversampledNode.h"
#include "tonestack/nodes/WaveshaperNode.h"

namespace {
std::atomic<bool> g_guard{false};
std::atomic<int>  g_allocs{0};

inline void countIfGuarded() {
    if (g_guard.load(std::memory_order_relaxed))
        g_allocs.fetch_add(1, std::memory_order_relaxed);
}

inline void* tracked_alloc(std::size_t n) {
    countIfGuarded();
    void* p = std::malloc(n != 0 ? n : 1);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

inline void* tracked_aligned_alloc(std::size_t n, std::size_t align) {
    countIfGuarded();
#if defined(_MSC_VER)
    void* p = _aligned_malloc(n != 0 ? n : 1, align);
#else
    void* p = nullptr;
    const std::size_t al = align < alignof(void*) ? alignof(void*) : align;
    if (posix_memalign(&p, al, n != 0 ? n : 1) != 0)
        p = nullptr;
#endif
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

inline void aligned_free(void* p) noexcept {
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
} // namespace

void* operator new(std::size_t n) { return tracked_alloc(n); }
void* operator new[](std::size_t n) { return tracked_alloc(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

void* operator new(std::size_t n, std::align_val_t al) {
    return tracked_aligned_alloc(n, static_cast<std::size_t>(al));
}
void* operator new[](std::size_t n, std::align_val_t al) {
    return tracked_aligned_alloc(n, static_cast<std::size_t>(al));
}
void operator delete(void* p, std::align_val_t) noexcept { aligned_free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { aligned_free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { aligned_free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { aligned_free(p); }

using namespace tonestack;

TS_TEST(process_does_not_allocate) {
    Chain chain;
    auto g = std::make_unique<nodes::GainNode>();
    g->parameters().get("gain").setProportion(0.7f);
    chain.add(std::move(g));
    chain.prepare(ProcessSpec{48000.0, 512, 2}); // allocation permitted here

    std::vector<float> l(512, 0.3f), r(512, 0.3f);
    float* chans[2] = {l.data(), r.data()};
    AudioBlock block(chans, 2, 512);

    g_allocs.store(0);
    g_guard.store(true);
    for (int i = 0; i < 100; ++i)
        chain.process(block);
    g_guard.store(false);

    TS_CHECK(g_allocs.load() == 0);
}

TS_TEST(biquad_process_does_not_allocate) {
    static const float axis0[2] = {0.0f, 1.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {2};
    static const nodes::BiquadSection sections[2] = {
        {0.2f, 0.1f, 0.05f, -0.3f, 0.1f},
        {0.3f, 0.1f, 0.05f, -0.3f, 0.1f},
    };
    nodes::BiquadCoeffTable table{axes, dims, 1, sections, 48000.0};

    Chain chain;
    auto biquad = std::make_unique<nodes::BiquadNode>(
        ParameterDesc{"drive", 0.0f, 1.0f, 0.5f, ParamSkew::Linear, 0.5f, 0.0f}, table);
    chain.add(std::move(biquad));
    chain.prepare(ProcessSpec{48000.0, 512, 2}); // allocation permitted here

    std::vector<float> l(512, 0.3f), r(512, 0.3f);
    float* chans[2] = {l.data(), r.data()};
    AudioBlock block(chans, 2, 512);

    g_allocs.store(0);
    g_guard.store(true);
    for (int i = 0; i < 100; ++i) {
        chain.at(0).parameters().byIndex(0).setProportion(static_cast<float>(i % 2)); // sweep -> re-interpolate
        chain.process(block);
    }
    g_guard.store(false);

    TS_CHECK(g_allocs.load() == 0);
}

TS_TEST(oversampled_waveshaper_process_does_not_allocate) {
    auto shaper = std::make_unique<nodes::WaveshaperNode<nodes::AsinhDiodeShape>>(
        ParameterDesc{"drive", 0.0f, 1.0f, 0.5f, ParamSkew::Linear, 0.5f, 0.0f},
        nodes::AsinhDiodeShape{2.52e-9f, 45.3e-3f, 51e3f, 500e3f});

    Chain chain;
    chain.add(std::make_unique<nodes::OversampledNode>(std::move(shaper)));
    chain.prepare(ProcessSpec{48000.0, 512, 2}); // allocation permitted here

    std::vector<float> l(512, 0.3f), r(512, 0.3f);
    float* chans[2] = {l.data(), r.data()};
    AudioBlock block(chans, 2, 512);

    g_allocs.store(0);
    g_guard.store(true);
    for (int i = 0; i < 50; ++i) {
        chain.at(0).parameters().byIndex(0).setProportion(static_cast<float>(i % 2));
        chain.process(block);
    }
    g_guard.store(false);

    TS_CHECK(g_allocs.load() == 0);
}

TS_TEST(iir_process_does_not_allocate) {
    static const float rows[2 * 9] = {
        0.2f, 0.1f, -0.05f, 0.02f, 0.01f, -0.8f, 0.3f, -0.1f, 0.02f,
        0.3f, 0.1f, -0.05f, 0.02f, 0.01f, -0.8f, 0.3f, -0.1f, 0.02f,
    };
    static const float axis0[2] = {0.0f, 1.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {2};
    nodes::IirCoeffTable table{axes, dims, 1, rows, 4, 48000.0};

    Chain chain;
    chain.add(std::make_unique<nodes::IirNode>(
        ParameterDesc{"volume", 0.0f, 1.0f, 0.5f, ParamSkew::Linear, 0.5f, 0.0f}, table));
    chain.prepare(ProcessSpec{48000.0, 512, 2}); // allocation permitted here

    std::vector<float> l(512, 0.3f), r(512, 0.3f);
    float* chans[2] = {l.data(), r.data()};
    AudioBlock block(chans, 2, 512);

    g_allocs.store(0);
    g_guard.store(true);
    for (int i = 0; i < 50; ++i) {
        chain.at(0).parameters().byIndex(0).setProportion(static_cast<float>(i % 2));
        chain.process(block);
    }
    g_guard.store(false);

    TS_CHECK(g_allocs.load() == 0);
}

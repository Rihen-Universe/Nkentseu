#include <chrono>
#include <cstdio>
#include "NKPlatform/NkFoundationLog.h"

#include "NKMath/NKMath.h"

using namespace nkentseu;
using namespace nkentseu::math;

int main() {
    constexpr nk_uint32 kCount = 4096u;
    constexpr nk_uint32 kIters = 512u;

    nk_float32 values[kCount];
    for (nk_uint32 i = 0; i < kCount; ++i) {
        values[i] = 1.0f + static_cast<nk_float32>(i) * 0.001f;
    }

    const auto t0 = std::chrono::high_resolution_clock::now();

#if NK_SIMD_AVAILABLE && defined(NK_ENABLE_AVX2)
    for (nk_uint32 iter = 0; iter < kIters; ++iter) {
        for (nk_uint32 i = 0; i + 8u <= kCount; i += 8u) {
            nk_simd_vec8f in = _mm256_loadu_ps(&values[i]);
            nk_simd_vec8f out = simd::NkSqrtVec8(in);
            _mm256_storeu_ps(&values[i], out);
        }
    }
#else
    for (nk_uint32 iter = 0; iter < kIters; ++iter) {
        for (nk_uint32 i = 0; i < kCount; ++i) {
            values[i] = NkSqrt(values[i]);
        }
    }
#endif

    const auto t1 = std::chrono::high_resolution_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();

    nk_float32 sink = 0.0f;
    for (nk_uint32 i = 0; i < kCount; ++i) {
        sink += values[i];
    }

    NK_FOUNDATION_LOG_INFO("\n[NKMath SIMD Benchmark] sqrt loop\n");
    NK_FOUNDATION_LOG_INFO("  NK_SIMD_AVAILABLE=%d\n", static_cast<int>(NK_SIMD_AVAILABLE));
#if defined(NK_ENABLE_AVX2)
    NK_FOUNDATION_LOG_INFO("  NK_ENABLE_AVX2=1\n");
#else
    NK_FOUNDATION_LOG_INFO("  NK_ENABLE_AVX2=0\n");
#endif
    NK_FOUNDATION_LOG_INFO("  Time: %.2f ns total (sink=%f)\n", ns, static_cast<double>(sink));

    return 0;
}


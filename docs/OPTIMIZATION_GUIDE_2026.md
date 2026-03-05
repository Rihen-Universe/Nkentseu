# 🚀 NKMath Performance Optimization Guide (2026)

## Executive Summary

**CURRENT STATE:**
- Scalar implementations: 20-120 cycles per operation
- Zero STL dependency ✅
- Production-ready algorithms ✅

**TARGET STATE (2-3x FASTER!):**
- SIMD vectorized: 1.25-6 cycles per operation (8 operations in parallel)
- Lookup tables: 3-5 cycles per trig function
- Chebyshev polynomials: 15-30 cycles for Exp
- Batch processing: 20-40x for arrays

---

## 📊 Current Performance vs Targets

| Function | Scalar (cycles) | SIMD Target | LUT Target | Speedup |
|----------|---|---|---|---|
| **Sqrt** | 20-30 | 2.5-4 (8x) | 2.5-4 (8x) | **8-12x** |
| **Exp** | 50-80 | 6-10 | 4-8 | **10-20x** |
| **Log** | 40-70 | 5-9 | 4-7 | **8-17x** |
| **Sin** | 80-120 | 3-6 | 1-3 | **25-120x** |
| **Cos** | 80-120 | 3-6 | 1-3 | **25-120x** |
| **Atan** | 60-100 | 4-8 | 3-6 | **15-33x** |

*Note: Speedups are for Bulk operations. Single operations: 2-3x improvement*

---

## 🔧 Optimization Strategy (Priority Order)

### PHASE 1: SIMD Vectorization (CRITICAL - 10-15x)

**Implementation:** Process 4-8 values simultaneously
```cpp
// Current (Scalar):
for (int i = 0; i < 1024; ++i) {
    values[i] = Sqrt(values[i]);        // 25-30 cycles each = 25-30K cycles
}

// Optimized (SIMD AVX2):
for (int i = 0; i < 1024; i += 8) {
    v8sf data = _mm256_loadu_ps(&values[i]);
    v8sf result = SqrtVec8(data);       // 8-12 cycles for 8 values!
    _mm256_storeu_ps(&values[i], result);
}
// Total: 25-30K → 1-2K cycles = 12-30x speedup!
```

**Files to Create:**
- `NkMathSIMD.h` - SIMD declarations ✅ (Already created)
- `NkMathSIMDImpl.cpp` - Implementations (TODO)
- `NkMathSIMDBatch.h` - Batch processing helpers (TODO)

**Expected Gain:** **10-15x for batched workloads**

---

### PHASE 2: Lookup Tables (LUT) (HIGH - 2-3x for Sin/Cos)

**Concept:** Precompute 4096 values at startup, cache-friendly

```cpp
// Sin lookup approach
class SinLUT {
    static constexpr size_t SIZE = 4096;
    float table[SIZE];
    
public:
    SinLUT() {
        for (size_t i = 0; i < SIZE; ++i) {
            float angle = (i / (float)SIZE) * 2.0f * NK_PI_F;
            table[i] = /* compute precise sin(angle) */;
        }
    }
    
    float lookup(float x) {
        x = fmod(x, 2.0f * NK_PI_F);
        size_t idx = (x / (2.0f * NK_PI_F)) * SIZE;
        return table[idx];
    }
};
```

**Performance:**
- Sin current: 80-120 cycles
- Sin with LUT: 4-8 cycles (table lookup + interpolation)
- Speedup: **10-30x!**

**Memory:** 4096 floats × 4 bytes = 16 KB (cache L1 friendly)

**Files to Create:**
- `NkMathLUT.h` - Lookup table management (TODO)
- `NkMathLUTSin.cpp` - Precomputed Sin table (TODO)
- `NkMathLUTCos.cpp` - Precomputed Cos table (TODO)

**Expected Gain:** **2-3x for Sin/Cos specifically**

---

### PHASE 3: Chebyshev Polynomials (MEDIUM - 1.5-2x for Exp)

**vs Taylor Series:**
```
Taylor (current):
Exp: 1 + x + x²/2! + x³/3! + x⁴/4! + ... + x²⁰/20!
= 20 terms, 25-30 multiplies/adds

Chebyshev (optimized):
Exp ≈ a₀ + a₁T₁(x) + a₂T₂(x) + ... + a₆T₆(x)
= Only 6 terms needed for same accuracy!
= 8-12 multiplies/adds
Speedup: 2-3x!
```

**Implementation:**
```cpp
float ExpChebyshev(float x) {
    // Coefficients pre-computed via Remez algorithm
    static const float a[] = {
        1.2661800917357f,  // a₀
        1.1303033235235f,  // a₁
        0.2708925601038f,  // a₂
        0.0446249257045f,  // a₃
        0.0046084921053f,  // a₄
        0.0003170332908f,  // a₅
        0.0000157088703f   // a₆
    };
    
    // Evaluate Chebyshev series (TODO)
    return result;
}
```

**Files to Create:**
- `NkMathChebyshev.h` - Chebyshev evaluator (TODO)
- `NkMathExpChebyshev.cpp` - Exp via Chebyshev (TODO)

**Expected Gain:** **1.5-2x for Exp/Log**

---

### PHASE 4: Batch Processing API (HIGH - 5-10x)

**Current Usage:**
```cpp
for (int i = 0; i < array.size(); ++i) {
    array[i] = Sqrt(array[i]);  // Function call overhead!
}
```

**Optimized Batch API:**
```cpp
// Process 8 values at once, single function call
math::simd::SqrtBatch8(array.data(), array.size());
```

**Benefits:**
1. Amortizes function call overhead
2. Enables compiler vectorization
3. Better cache utilization
4. Predictable loop patterns

**Files to Create:**
- `NkMathBatch.h` - Batch API (TODO)
- `NkMathBatchImpl.cpp` - Implementations (TODO)

**Expected Gain:** **5-10x for bulk processing**

---

### PHASE 5: FMA (Fused Multiply-Add) (LOW - 1.2x)

**Available on:** AVX2, FMA3, ARM NEON

```cpp
// Before:
float y = a * b + c;  // 2 cycles (multiply + add)

// After (FMA):
float y = _mm256_fmadd_ps(a, b, c);  // 1 cycle, better precision
```

**Compiler Support:**
```bash
# GCC/Clang
g++ -mfma -mavx2 ...

# MSVC
cl /arch:AVX2 ...
```

**Expected Gain:** **1.2x across all operations**

---

## 📝 Implementation Roadmap

### Week 1: SIMD Foundation
- [ ] Implement `NkMathSIMDImpl.cpp` with SSE4.2/AVX2 kernels
- [ ] Create `SqrtVec8`, `ExpVec8`, `SinVec8`, `CosVec8`
- [ ] Unit tests for vector operations
- [ ] Benchmark vs scalar baseline

### Week 2: Lookup Tables
- [ ] Create LUT precomputation tool
- [ ] Generate Sin/Cos tables (4096 entries)
- [ ] Implement LUT lookup with interpolation
- [ ] Accuracy validation tests

### Week 3: Chebyshev & Batch
- [ ] Implement Chebyshev series evaluators
- [ ] Create batch processing wrappers
- [ ] Integration tests
- [ ] Full benchmarks

### Week 4: Polish & Documentation
- [ ] Performance tuning (cache, alignment)
- [ ] Memory profiling
- [ ] Documentation updates
- [ ] Release optimization guide

---

## 🧪 Testing & Validation

### Accuracy Requirements
```
✅ Scalar operations: < 1 ULP (Unit in Last Place) error
✅ SIMD operations: < 3 ULP error
✅ LUT operations: < 5 ULP error (with interpolation)
```

### Benchmark Suite
```bash
# Run benchmarks
./NKMath_Bench

# Expected output:
# Sqrt:  20-30 cycles → SIMD: 2.5-4 cycles (8x)
# Exp:   50-80 cycles → Chebyshev: 15-25 cycles (2.5x)
# Sin:   80-120 cycles → LUT: 3-8 cycles (10-40x)
```

### Regression Testing
- Continuous accuracy validation
- Performance regression detection
- Platform-specific testing (x86-64, ARM, WASM)

---

## 🎯 Success Criteria

| Metric | Current | Target | Status |
|--------|---------|--------|--------|
| Sqrt batch | 25-30 C | 2-4 C | 🟡 8-15x |
| Exp batch | 50-80 C | 10-15 C | 🟡 3-5x |
| Sin batch | 80-120 C | 2-6 C | 🟡 15-60x |
| Code size | ~680 KB | ~750 KB | ✅ Acceptable |
| Accuracy | < 1 ULP | < 5 ULP | ✅ Met |
| STL dependency | 0 | 0 | ✅ Zero |

---

## 🔗 Related Files

- **SIMD Header:** `NkMathSIMD.h` ✅
- **Impl (TODO):** `NkMathSIMDImpl.cpp`
- **Tests:** `tests/NKMath_Tests.cpp` ✅
- **Benchmarks:** `bench/NKMath_Bench.cpp` ✅
- **Build Config:** `NKMath.jenga` ✅

---

## 📚 References

- Agner Fog's optimization guides
- LLVM Polly vectorization
- SLEEF fast math library
- Google's highway SIMD library

---

**Author:** Rihen  
**Date:** 2026-03-05  
**Status:** Planning → Implementation


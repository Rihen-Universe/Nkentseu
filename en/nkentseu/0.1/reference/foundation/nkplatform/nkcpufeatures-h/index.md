# NkCPUFeatures.h

`#include "NKPlatform/NkCPUFeatures.h"`

## Types

- [nkentseu::platform::CacheInfo](../nkentseu-platform-cacheinfo/index.md)
- [nkentseu::platform::CPUTopology](../nkentseu-platform-cputopology/index.md)
- [nkentseu::platform::SIMDFeatures](../nkentseu-platform-simdfeatures/index.md)
- [nkentseu::platform::ExtendedFeatures](../nkentseu-platform-extendedfeatures/index.md)
- [nkentseu::platform::CPUFeatures](../nkentseu-platform-cpufeatures/index.md)

## Functions

### NkGetCPUFeatures

```cpp
const NkCPUFeatures &NkGetCPUFeatures()
```

Gets the CPUFeatures instance through the Nk alias

### NkHasSSE2

```cpp
bool NkHasSSE2()
```

Checks SSE2 support at run time

### NkHasAVX

```cpp
bool NkHasAVX()
```

Checks AVX support at run time

### NkHasAVX2

```cpp
bool NkHasAVX2()
```

Checks AVX2 support at run time

### NkHasAVX512

```cpp
bool NkHasAVX512()
```

Checks AVX-512 support at run time

### NkHasNEON

```cpp
bool NkHasNEON()
```

Checks NEON support at run time

### NkHasFMA

```cpp
bool NkHasFMA()
```

Checks FMA support at run time

### NkGetCacheLineSize

```cpp
int32_t NkGetCacheLineSize()
```

Gets the cache line size in bytes

### NkGetPhysicalCoreCount

```cpp
int32_t NkGetPhysicalCoreCount()
```

Gets the number of physical cores

### NkGetLogicalCoreCount

```cpp
int32_t NkGetLogicalCoreCount()
```

Gets the number of logical cores (threads)

## Aliases

### NkCacheInfo

```cpp
using NkCacheInfo = CacheInfo
```

Alias for CacheInfo (Nk API compatibility)

### NkCPUTopology

```cpp
using NkCPUTopology = CPUTopology
```

Alias for CPUTopology (Nk API compatibility)

### NkSIMDFeatures

```cpp
using NkSIMDFeatures = SIMDFeatures
```

Alias for SIMDFeatures (Nk API compatibility)

### NkExtendedFeatures

```cpp
using NkExtendedFeatures = ExtendedFeatures
```

Alias for ExtendedFeatures (Nk API compatibility)

### NkCPUFeatures

```cpp
using NkCPUFeatures = CPUFeatures
```

Alias for CPUFeatures (Nk API compatibility)


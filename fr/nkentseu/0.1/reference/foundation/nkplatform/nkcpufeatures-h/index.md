# NkCPUFeatures.h

`#include "NKPlatform/NkCPUFeatures.h"`

## Types

- [nkentseu::platform::CacheInfo](../nkentseu-platform-cacheinfo/index.md)
- [nkentseu::platform::CPUTopology](../nkentseu-platform-cputopology/index.md)
- [nkentseu::platform::SIMDFeatures](../nkentseu-platform-simdfeatures/index.md)
- [nkentseu::platform::ExtendedFeatures](../nkentseu-platform-extendedfeatures/index.md)
- [nkentseu::platform::CPUFeatures](../nkentseu-platform-cpufeatures/index.md)

## Fonctions

### NkGetCPUFeatures

```cpp
const NkCPUFeatures &NkGetCPUFeatures()
```

Obtenir l'instance CPUFeatures via l'alias Nk

### NkHasSSE2

```cpp
bool NkHasSSE2()
```

Vérifier le support de SSE2 à l'exécution

### NkHasAVX

```cpp
bool NkHasAVX()
```

Vérifier le support de AVX à l'exécution

### NkHasAVX2

```cpp
bool NkHasAVX2()
```

Vérifier le support de AVX2 à l'exécution

### NkHasAVX512

```cpp
bool NkHasAVX512()
```

Vérifier le support de AVX-512 à l'exécution

### NkHasNEON

```cpp
bool NkHasNEON()
```

Vérifier le support de NEON à l'exécution

### NkHasFMA

```cpp
bool NkHasFMA()
```

Vérifier le support de FMA à l'exécution

### NkGetCacheLineSize

```cpp
int32_t NkGetCacheLineSize()
```

Obtenir la taille de ligne de cache en octets

### NkGetPhysicalCoreCount

```cpp
int32_t NkGetPhysicalCoreCount()
```

Obtenir le nombre de cœurs physiques

### NkGetLogicalCoreCount

```cpp
int32_t NkGetLogicalCoreCount()
```

Obtenir le nombre de cœurs logiques (threads)

## Alias

### NkCacheInfo

```cpp
using NkCacheInfo = CacheInfo
```

Alias pour CacheInfo (compatibilité API Nk)

### NkCPUTopology

```cpp
using NkCPUTopology = CPUTopology
```

Alias pour CPUTopology (compatibilité API Nk)

### NkSIMDFeatures

```cpp
using NkSIMDFeatures = SIMDFeatures
```

Alias pour SIMDFeatures (compatibilité API Nk)

### NkExtendedFeatures

```cpp
using NkExtendedFeatures = ExtendedFeatures
```

Alias pour ExtendedFeatures (compatibilité API Nk)

### NkCPUFeatures

```cpp
using NkCPUFeatures = CPUFeatures
```

Alias pour CPUFeatures (compatibilité API Nk)


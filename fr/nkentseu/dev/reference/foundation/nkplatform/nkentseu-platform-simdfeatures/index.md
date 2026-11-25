# nkentseu::platform::SIMDFeatures

Détection des extensions SIMD disponibles

`#include "NKPlatform/NkCPUFeatures.h"`

## Hiérarchie

- Classes de base : aucune
- Classes dérivées : aucune

## Membres

### hasMMX

```cpp
bool hasMMX
```

Support de MMX (MultiMedia eXtensions)

### hasSSE

```cpp
bool hasSSE
```

Support de SSE (Streaming SIMD Extensions)

### hasSSE2

```cpp
bool hasSSE2
```

Support de SSE2

### hasSSE3

```cpp
bool hasSSE3
```

Support de SSE3

### hasSSSE3

```cpp
bool hasSSSE3
```

Support de SSSE3 (Supplemental SSE3)

### hasSSE41

```cpp
bool hasSSE41
```

Support de SSE4.1

### hasSSE42

```cpp
bool hasSSE42
```

Support de SSE4.2

### hasAVX

```cpp
bool hasAVX
```

Support de AVX (Advanced Vector Extensions)

### hasAVX2

```cpp
bool hasAVX2
```

Support de AVX2

### hasAVX512F

```cpp
bool hasAVX512F
```

Support de AVX-512 Foundation

### hasAVX512DQ

```cpp
bool hasAVX512DQ
```

Support de AVX-512 Doubleword & Quadword

### hasAVX512BW

```cpp
bool hasAVX512BW
```

Support de AVX-512 Byte & Word

### hasAVX512VL

```cpp
bool hasAVX512VL
```

Support de AVX-512 Vector Length

### hasFMA

```cpp
bool hasFMA
```

Support de FMA (Fused Multiply-Add)

### hasFMA4

```cpp
bool hasFMA4
```

Support de FMA4 (AMD-specific)

### hasNEON

```cpp
bool hasNEON
```

Support de NEON (ARM SIMD)

### hasSVE

```cpp
bool hasSVE
```

Support de SVE (Scalable Vector Extension)

### hasSVE2

```cpp
bool hasSVE2
```

Support de SVE2

### SIMDFeatures

```cpp
SIMDFeatures()
```

Constructeur par défaut avec toutes les fonctionnalités désactivées

# nkentseu::platform::SIMDFeatures

Detection of the available SIMD extensions

`#include "NKPlatform/NkCPUFeatures.h"`

## Hierarchy

- Base classes : none
- Derived classes : none

## Members

### hasMMX

```cpp
bool hasMMX
```

MMX (MultiMedia eXtensions) support

### hasSSE

```cpp
bool hasSSE
```

SSE (Streaming SIMD Extensions) support

### hasSSE2

```cpp
bool hasSSE2
```

SSE2 support

### hasSSE3

```cpp
bool hasSSE3
```

SSE3 support

### hasSSSE3

```cpp
bool hasSSSE3
```

SSSE3 (Supplemental SSE3) support

### hasSSE41

```cpp
bool hasSSE41
```

SSE4.1 support

### hasSSE42

```cpp
bool hasSSE42
```

SSE4.2 support

### hasAVX

```cpp
bool hasAVX
```

AVX (Advanced Vector Extensions) support

### hasAVX2

```cpp
bool hasAVX2
```

AVX2 support

### hasAVX512F

```cpp
bool hasAVX512F
```

AVX-512 Foundation support

### hasAVX512DQ

```cpp
bool hasAVX512DQ
```

AVX-512 Doubleword & Quadword support

### hasAVX512BW

```cpp
bool hasAVX512BW
```

AVX-512 Byte & Word support

### hasAVX512VL

```cpp
bool hasAVX512VL
```

AVX-512 Vector Length support

### hasFMA

```cpp
bool hasFMA
```

FMA (Fused Multiply-Add) support

### hasFMA4

```cpp
bool hasFMA4
```

FMA4 support (AMD-specific)

### hasNEON

```cpp
bool hasNEON
```

NEON (ARM SIMD) support

### hasSVE

```cpp
bool hasSVE
```

SVE (Scalable Vector Extension) support

### hasSVE2

```cpp
bool hasSVE2
```

SVE2 support

### SIMDFeatures

```cpp
SIMDFeatures()
```

Default constructor with every feature disabled
